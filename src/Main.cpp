#include <vd/Options.h>
#include <vd/Tga.h>
#include <vd/VideoStream.h>

#include <libyuv.h>

#include <future>
#include <limits>
#include <semaphore>

using namespace std::chrono_literals;
using namespace vd;

namespace
{

template <SourceConcept SourceT>
std::shared_ptr<SourceBase> MakeSource(const std::string &url,
                                       std::size_t numChunks,
                                       std::size_t chunkSize)
{
    return
        std::shared_ptr<SourceBase>{
            new Source{
                CachedSource{
                    SourceT{url}, numChunks, chunkSize}}};
}

std::shared_ptr<SourceBase> OpenSource(const std::string &url,
                                       std::size_t numChunks,
                                       std::size_t chunkSize)
{
    std::shared_ptr<SourceBase> res;

    try
    {
        res = MakeSource<HttpSource>(url, numChunks, chunkSize);
    }
    catch(...)
    {
        res = MakeSource<FileSource>(url, numChunks, chunkSize);
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



using FrameHandler =
    std::function<void(const ArgbImage &, std::chrono::nanoseconds, std::size_t)>;
using Semaphore =
    std::counting_semaphore<std::numeric_limits<decltype(Options::numThreads)>::max()>;

struct ThreadContext final
{
    std::shared_ptr<SourceBase> source;
    Semaphore &semaphore;
    Options::Segment range;
    Options options;
};



void ForEachFrame(ThreadContext ctx, FrameHandler callback)
{
    ctx.semaphore.acquire();
    auto releaseSemaphore = Defer([&ctx]() { ctx.semaphore.release(); });

    auto params = MediaParams::Default();
    params.skipNonRef = ctx.options.skipping;

    auto stream =
        OpenMediaSource(
            [&ctx]()
            {
                return
                    std::make_unique<LibavReader>(
                        ctx.source,
                        LibavReader::SeekSizeMode::Cache);
            },
            params);

    auto interval = (ctx.range.to - ctx.range.from) / (ctx.range.numFrames + 1ll);
    //== 0 is very extreme case but still possible
    if(interval <= 0ns)
    {
        throw Error{std::format("too many frames ({}) requested in segment",
                                ctx.range.numFrames)};
    }

    for(std::int64_t i = 0; i < ctx.range.numFrames + 2ll; ++i)
    {
        auto timestamp = ctx.range.from + i * interval;
        auto frame = stream.NextFrame(timestamp).value();
        callback(frame.Image(), timestamp, i + 1ull);
    }
}

void SaveFrame(
    std::string_view pathTemplate,
    std::size_t segmentIndex,
    const ArgbImage &image,
    std::chrono::nanoseconds timestamp,
    std::size_t frameIndex)
{
    auto timestampStr = ToString(timestamp);
    auto pathStr =
        std::vformat(pathTemplate,
                     std::make_format_args(segmentIndex,
                                           frameIndex,
                                           timestampStr));
    WriteTga(pathStr, image.data, image.width);
}

std::size_t EstimateNumChunks(std::size_t numThreads)
{
    //We want to keep starting chunk containing metadata and at least one chunk
    //for each thread, so for safety we keep twice as much 
    return numThreads*2 + 1;
}

}//unnamed namespace



int main(int argc, char *argv[])
{
    auto semaphore = std::optional<Semaphore>{};
    auto futures = std::vector<std::future<void>>{};
    int err = 0;

    try
    {
        auto options = ParseOptions(argc, argv);
        if(!options)
        {
            return 0;
        }

        semaphore.emplace(options->numThreads);
        auto source = OpenSource(options->videoUrl,
                                 EstimateNumChunks(options->numThreads),
                                 options->chunkSize);

        for(std::size_t segmentIndex = 0; auto &segment : options->segments)
        {
            ++segmentIndex;
            auto format = options->format;
            
            //It's for exception safety of push_back. Has to be placed before async
            futures.reserve(futures.size() + 1);

            auto future = std::async(
                ForEachFrame,
                    ThreadContext{
                        .source = source,
                        .semaphore = *semaphore,
                        .range = segment,
                        .options = *options
                    },
                    [format,segmentIndex](auto const &image, auto timestamp, auto index)
                    {
                        SaveFrame(format, segmentIndex, image, timestamp, index);
                    });
            
            futures.push_back(std::move(future));
        }
    }
    catch(const std::exception &e)
    {
        Errorln(e.what());
        err = 1;
    }

    //It seems that standard doesn't guarantee that futures returned by async will wait for completion in destructor
    //(at least words "may" and "can" give hint about it), so we have to write this ugly loop
    for(auto &future : futures)
    {
        try
        {
            future.get();
        }
        catch(const std::exception &e)
        {
            Errorln(e.what());
            err = 1;
        }
    }

    return err;
}