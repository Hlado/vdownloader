#include <vd/Ap4ByteStream.h>  
#include <vd/Mp4Container.h>
#include <vd/Decoder.h>
#include <vd/OpenH264Decoder.h>
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

const std::size_t gChunkSize = 2000000; //~2mb
const std::size_t gNumCachedChunks = 2;



std::shared_ptr<AP4_ByteStream> OpenSource(const std::string &url)
{
    std::shared_ptr<AP4_ByteStream> res;

    try
    {
        res = std::make_shared<Ap4HttpByteStream>(CachedSource{HttpSource{url}, gNumCachedChunks, gChunkSize});
    }
    catch(...)
    {
        auto err = AP4_FileByteStream::Create(url.c_str(), AP4_FileByteStream::STREAM_MODE_READ, res);
        if(AP4_FAILED(err))
        {
            throw Error(std::format(R"(Unable to open video source "{}")", url));
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

SerialDecoder Slice(const Track &track, std::chrono::nanoseconds from, std::chrono::nanoseconds to)
{
    auto decoders = std::vector<Decoder>{};
    for(auto &segment : track.Slice(from, to))
    {
        decoders.emplace_back(track.GetDecodingConfig(), std::move(segment));
    }
    return SerialDecoder(std::move(decoders));
}

using FrameHandler = std::function<void(const ArgbImage &,std::chrono::nanoseconds,std::size_t)>;
using Semaphore = std::counting_semaphore<std::numeric_limits<std::ptrdiff_t>::max()>;

void ForEachFrame(Semaphore &semaphore, SerialDecoder decoder, Options::Segment segOpt, FrameHandler callback)
{
    semaphore.acquire();
    
    try
    {
        auto interval = (segOpt.to - segOpt.from) / (segOpt.numFrames + 1ll);

        //== 0 is very extreme case but still possible
        if(interval <= 0ns)
        {
            throw Error{std::format("too many frames ({}) requested in segment", segOpt.numFrames)};
        }

        auto frame = decoder.GetNext().value();
        for(std::int64_t i = 0; i < segOpt.numFrames + 2ll; ++i)
        {
            //There will be some inaccuracy due to integer arithmetic, but we use ns, so it's too small to care about
            std::chrono::nanoseconds timestamp = segOpt.from + i*interval;
            //It's to ensure that last frame taken is exactly as requested
            if(i == segOpt.numFrames + 1ll)
            {
                timestamp = segOpt.to;
            }

            while(decoder.HasMore() && decoder.TimestampNext() <= timestamp)
            {
                frame = decoder.GetNext().value();
            }

            callback(frame.image, timestamp, i + 1ull);
        }
    }
    catch(...)
    {
        semaphore.release();
        throw;
    }
    semaphore.release();
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

}



int main(int argc, char *argv[])
{
    static auto semaphore = std::optional<Semaphore>{};

    try
    {
        auto options = ParseOptions(argc, argv);
        if(!options)
        {
            return 0;
        }

        semaphore.emplace(options->numThreads);
        Mp4Container container(OpenSource(options->videoUrl));

        auto futures = std::vector<std::future<void>>{};
        for(std::size_t segmentIndex = 0; auto &segment : options->segments)
        {
            ++segmentIndex;
            auto format = options->format;
            
            auto future = std::async(
                ForEachFrame,
                    std::ref(*semaphore),
                    Slice(container.GetTrack(), segment.from, segment.to),
                    segment,
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