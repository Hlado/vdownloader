#include <vd/ImageFormats.h>
#include <vd/Options.h>
#include <vd/VideoStream.h>

#include <algorithm>
#include <future>
#include <limits>
#include <semaphore>

using namespace std::chrono_literals;
using namespace vd;

namespace
{

using Semaphore =
    std::counting_semaphore<std::numeric_limits<decltype(Options::numThreads)>::max()>;

struct ThreadContext final
{
    std::shared_ptr<SourceBase> source;
    Semaphore &semaphore;
    Options options;
    std::size_t segIdx;
};



std::shared_ptr<SourceBase> OpenSource(const Options &options);
std::future<void> LaunchThread(ThreadContext ctx);
int SafeWait(std::vector<std::future<void>> &futures) noexcept;

}



int main(int argc, char *argv[])
{
    auto semaphore = std::optional<Semaphore>{};
    auto futures = std::vector<std::future<void>>{};
    int err = 0;

    //This block is needed to guarantee that deferred lambda executed before return
    {
        //It seems that standard doesn't guarantee that futures returned
        //by async will wait for completion in destructor (at least words
        //"may" and "can" give hint about it), so we have to write this
        Defer wait{
            [&err, &futures] ()
            {
                err += SafeWait(futures);
            }};

        try
        {
            auto options = ParseOptions(argc, argv);
            if(!options)
            {
                return 0;
            }

            //It's for exception safety of push_back. Has to be placed before async
            futures.reserve(options->segments.size());
            semaphore.emplace(options->numThreads);

            auto source = OpenSource(*options);

            for(std::size_t idx = 0; idx < options->segments.size(); ++idx)
            {
                auto ctx = ThreadContext{ .source = source,
                                          .semaphore = *semaphore,
                                          .options = *options,
                                          .segIdx = idx };
                futures.push_back(LaunchThread(std::move(ctx)));
            }
        
        }
        catch(const std::exception &e)
        {
            err = 1;
            Errorln(e.what());
        }
    }

    return err;
}

namespace
{

void ThreadMain(ThreadContext &ctx);

std::future<void> LaunchThread(ThreadContext ctx)
{
    return
        std::async(
            [ctx = std::move(ctx)]() mutable
            {
                ThreadMain(ctx);
            });
}

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

VideoStream OpenStream(std::shared_ptr<SourceBase> source,
                       const Options &options);
void SaveFrame(std::filesystem::path path, const Frame &frame);
std::filesystem::path MakePath(std::string_view pattern,
                               std::size_t segIndex,
                               std::size_t frameIndex,
                               Nanoseconds timestamp);

void ThreadMain(ThreadContext &ctx)
{
    ctx.semaphore.acquire();
    Defer release{
        [&ctx]()
        {
            ctx.semaphore.release();
        }};

    auto stream = OpenStream(ctx.source, ctx.options);

    auto seg = ctx.options.segments[ctx.segIdx];
    auto interval = (seg.to - seg.from) / (seg.numFrames + 1ll);
    //== 0 is very extreme case but still possible
    if(interval <= 0ns)
    {
        throw Error{std::format("too many frames ({}) requested in segment",
                                seg.numFrames)};
    }

    auto numFrames = seg.numFrames + 2ll;
    for(std::int64_t frameIdx = 0; frameIdx < numFrames; ++frameIdx)
    {
        auto timestamp = seg.from + interval*frameIdx;

        auto frame = stream.NextFrame(timestamp).value();
        
        auto path = MakePath(ctx.options.format,
                             ctx.segIdx,
                             IntCast<std::size_t>(frameIdx + 1),
                             timestamp);
        SaveFrame(path, frame);
    }
}

VideoStream OpenStream(std::shared_ptr<SourceBase> source,
                       const Options &options)
{
    OpeningParams params;
    params.skipNonRef = options.skipping;

    auto factory =
        [&source]()
        {
            return
                std::make_unique<libav::Reader>(
                    source,
                    libav::Reader::SeekSizeMode::Cache);
        };

    return OpenMediaSource(factory, params);
}

std::size_t CalcNumChunks(std::size_t numThreads)
{
    //We want to keep starting chunk containing metadata and at least one chunk
    //for each thread, so for safety we keep twice as much 
    return numThreads*2 + 1;
}

std::shared_ptr<SourceBase> OpenSource(const Options &options)
{
    return OpenSource(options.videoUrl,
                      CalcNumChunks(options.numThreads),
                      options.chunkSize);
}

bool HasValidExtension(const std::filesystem::path &path)
{
    static const auto exts =
        std::unordered_set<std::string>{".tga", ".jpg", ".png"};

    return path.has_extension() && exts.contains(path.extension().string());
}

void SaveFrame(std::filesystem::path path, const Frame &frame)
{
    if(!HasValidExtension(path))
    {
        path += ".jpg";
    }

    if(path.extension() == ".tga")
    {
        auto image = frame.BgraImage();
        WriteTga(path, image);
    }
    else if(path.extension() == ".png")
    {
        auto image = frame.RgbaImage();
        WritePng(path, image);
    }
    else
    {
        auto image = frame.RgbaImage();
        WriteJpg(path, image);
    }
}

int SafeWait(std::future<void> &fut) noexcept
{
    try
    {
        fut.get();
    }
    catch(const std::exception &e)
    {
        try { Errorln(e.what()); } catch(...) {}
        return 1;
    }

    return 0;
}

int SafeWait(std::vector<std::future<void>> &futures) noexcept
{
    auto err = int{0};

    for(auto &f : futures)
    {
        err += SafeWait(f);
    }

    return err;
}

template <typename RepT, typename PeriodT>
std::string ToString(const std::chrono::duration<RepT, PeriodT> &val)
{
    using namespace std::chrono;

    auto s = duration_cast<seconds>(val);
    auto ms = duration_cast<milliseconds>(val - s);
    return std::format("{}s{}ms", s.count(), ms.count());
}

std::filesystem::path MakePath(std::string_view format,
                               std::size_t segIndex,
                               std::size_t frameIndex,
                               Nanoseconds timestamp)
{
    return Format(format,
                  segIndex,
                  frameIndex,
                  ToString(timestamp));
}

}//unnamed namespace