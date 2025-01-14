#include <vd/Ap4ByteStream.h>  
#include <vd/Mp4Container.h>
#include <vd/Decoder.h>
#include <vd/Options.h>
#include <vd/Tga.h>

#include <libyuv.h>

#include <future>
#include <limits>
#include <semaphore>

using namespace std::chrono_literals;
using namespace vd;

namespace
{

using Decoder = DecoderLibav;
using SerialDecoder = SerialDecoderBase<Decoder>;



const std::size_t gNumCachedChunks = 2;
const int gDefaultNumDecoderThreads = 2;

std::shared_ptr<AP4_ByteStream> OpenSource(const std::string &url, std::size_t chunkSize)
{
    std::shared_ptr<AP4_ByteStream> res;

    try
    {
        res = std::make_shared<Ap4HttpByteStream>(CachedSource{HttpSource{url}, gNumCachedChunks, chunkSize});
    }
    catch(...)
    {
        auto err = AP4_FileByteStream::Create(url.c_str(), AP4_FileByteStream::STREAM_MODE_READ, res);
        if(AP4_FAILED(err))
        {
            throw Error{std::format(R"(Unable to open video source "{}")", url)};
        }
    }

    return res;
}

template <typename RepT, typename PeriodT>
std::string ToString(const std::chrono::duration<RepT, PeriodT> &val)
{
    using namespace std::chrono;

    auto s = duration_cast<seconds>(val);
    auto ms = duration_cast<milliseconds>(val - s);
    return std::format("{}s{}ms", s.count(), ms.count());
}

SerialDecoder MakeDecoder(std::vector<vd::Segment> segments, const vd::DecodingConfig &config, std::uint8_t numThreads)
{
    auto decoders = std::vector<Decoder>{};
    for(auto &segment : segments)
    {
        decoders.emplace_back(config, std::move(segment), LibavH264Decoder{numThreads});
    }
    return SerialDecoder(std::move(decoders));
}

int PickNumDecoderThreads(std::uint8_t base, const std::atomic<int> &numActiveThreads) noexcept
{
    if(base != 0)
    {
        return IntCast<int>(base);
    }

    auto numAvailableThreads = IntCast<int>(GetNumCores()) - numActiveThreads.load();
    if(numAvailableThreads <= 0)
    {
        return gDefaultNumDecoderThreads;
    }
    else
    {
        return numAvailableThreads + 1;
    }
}



using FrameHandler = std::function<void(const ArgbImage &,std::chrono::nanoseconds,std::size_t)>;
using Semaphore = std::counting_semaphore<std::numeric_limits<decltype(Options::numThreads)>::max()>;

struct ThreadContext final
{
    std::vector<vd::Segment> videoSegments;
    vd::DecodingConfig config;
    Semaphore &semaphore;
    std::atomic<int> &numActiveThreads;
    Options::Segment range;
    std::uint8_t numDecoderThreads;
};

void ForEachFrame(ThreadContext ctx, FrameHandler callback)
{
    ctx.semaphore.acquire();
    
    auto numDecoderThreads = PickNumDecoderThreads(ctx.numDecoderThreads, ctx.numActiveThreads);
    //There's a problem - in theory multiple decoding threads may perform these calculations simultaneously
    //(although it's practically impossible), so in case of adaptive number of threads there will be some inaccuracy,
    //but even then it's not a big mistake, so for simplicity we ignore it
    ctx.numActiveThreads.fetch_add(numDecoderThreads);

    try
    {
        auto decoder = MakeDecoder(std::move(ctx.videoSegments), ctx.config, IntCast<std::uint8_t>(numDecoderThreads));
        auto interval = (ctx.range.to - ctx.range.from) / (ctx.range.numFrames + 1ll);

        //== 0 is very extreme case but still possible
        if(interval <= 0ns)
        {
            throw Error{std::format("too many frames ({}) requested in segment", ctx.range.numFrames)};
        }

        auto frame = decoder.GetNext().value();
        for(std::int64_t i = 0; i < ctx.range.numFrames + 2ll; ++i)
        {
            //There will be some inaccuracy due to integer arithmetic, but we use ns, so it's too small to care about
            std::chrono::nanoseconds timestamp = ctx.range.from + i*interval;
            //It's to ensure that last frame taken is exactly as requested
            if(i == ctx.range.numFrames + 1ll)
            {
                timestamp = ctx.range.to;
            }

            if(decoder.HasMore() && decoder.TimestampNext() <= timestamp)
            {
                decoder.SkipTo(timestamp);
                frame = decoder.GetNext().value();
            }

            callback(frame.image, timestamp, i + 1ull);
        }
    }
    catch(...)
    {
        ctx.numActiveThreads.fetch_sub(numDecoderThreads);
        ctx.semaphore.release();
        throw;
    }
    ctx.numActiveThreads.fetch_sub(numDecoderThreads);
    ctx.semaphore.release();
}

void SaveFrame(
    std::string_view pathTemplate,
    std::size_t segmentIndex,
    const ArgbImage &image,
    std::chrono::nanoseconds timestamp,
    std::size_t frameIndex)
{
    auto timestampStr = ToString(timestamp);
    auto pathStr = std::vformat(pathTemplate, std::make_format_args(segmentIndex, frameIndex, timestampStr));
    auto path = std::filesystem::path(pathStr);
    WriteTga(path, image.data, image.width);
}

}//unnamed namespace



int main(int argc, char *argv[])
{
    static auto semaphore = std::optional<Semaphore>{};
    static auto numActiveThreads = std::atomic<int>{0};

    try
    {
        auto options = ParseOptions(argc, argv);
        if(!options)
        {
            return 0;
        }

        semaphore.emplace(options->numThreads);
        Mp4Container container(OpenSource(options->videoUrl, options->chunkSize));

        auto futures = std::vector<std::future<void>>{};
        for(std::size_t segmentIndex = 0; auto &segment : options->segments)
        {
            ++segmentIndex;
            auto format = options->format;
            
            //For some reason, if Slice(...) throws, main thread hangs until already started decoders finish their work.
            //Likely it's because some waiting mechanism inside future destructor, but it's complex topic
            //and is not really worth to dig deep into it. That only delays failure, doesn't introduce any error
            auto future = std::async(
                ForEachFrame,
                    ThreadContext{
                        .videoSegments = container.GetTrack().Slice(segment.from, segment.to),
                        .config = container.GetTrack().GetDecodingConfig(),
                        .semaphore = *semaphore,
                        .numActiveThreads = numActiveThreads,
                        .range = segment,
                        .numDecoderThreads = options->numDecoderThreads
                    },
                    [format,segmentIndex](auto const &image, auto timestamp, auto index)
                    {
                        SaveFrame(format, segmentIndex, image, timestamp, index);
                    });

            futures.push_back(std::move(future));
        }

        //Not very elegant way to deal with exceptions, but nothing fatal will happen,
        //only corrupted output files at most
        for(auto &future : futures)
        {
            future.get();
        }

        return 0;
    }
    catch(const std::exception &e)
    {
        Errorln(e.what());
        return 1;
    }
}