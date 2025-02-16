#ifndef VDOWNLOADER_VD_VIDEO_STREAM_H_
#define VDOWNLOADER_VD_VIDEO_STREAM_H_

#include "DecodingUtils.h"
#include "LibavUtils.h"
#include "Sources.h"

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

namespace vd
{

using Nanoseconds = std::chrono::nanoseconds;



class Frame final
{
public:
    static inline constexpr Nanoseconds sentinelTs = Nanoseconds::max();

    Frame(std::shared_ptr<const AVFrame> rawFrame,
          Nanoseconds timestamp,
          Nanoseconds duration);

    static Frame Create(std::shared_ptr<const AVFrame> rawFrame,
                        AVRational timeBase);

    ArgbImage Image() const;
    //Equal to Frame::sentinelTs when unknown
    Nanoseconds Timestamp() const noexcept;
    //Equal to zero when unknown
    Nanoseconds Duration() const noexcept;

private:
    std::shared_ptr<const AVFrame> mFrame;
    Nanoseconds mTimestamp;
    Nanoseconds mDuration;
};



class VideoStream;
using StreamPicker = std::function<std::size_t(std::span<const AVStream * const>)>;
using ReaderFactory = std::function<std::unique_ptr<LibavReader>()>;

VideoStream OpenMediaSource(ReaderFactory readerFactory,
                            const StreamPicker &picker = [](auto) { return 0; });



struct MediaContext;

class VideoStream final
{
    friend VideoStream OpenMediaSource(ReaderFactory readerFactory,
                                       const StreamPicker &picker);

private:
    VideoStream(std::unique_ptr<MediaContext> activeContext,
                std::unique_ptr<MediaContext> seekingContext,
                AVStream &stream);

public:
    VideoStream(const VideoStream &other) = delete;
    VideoStream &operator=(const VideoStream &other) = delete;
    VideoStream(VideoStream &&other);
    VideoStream &operator=(VideoStream &&other);
    ~VideoStream();

    std::optional<Frame> NextFrame(Nanoseconds timestamp = Frame::sentinelTs);

private:
    std::unique_ptr<MediaContext> mActiveCtx;
    std::unique_ptr<MediaContext> mSeekingCtx;
    std::reference_wrapper<AVStream> mStream;
    std::shared_ptr<AVFrame> mLastReturnedFrame;
    std::deque<std::shared_ptr<AVFrame>> mFramesQueue;

    static std::shared_ptr<AVFrame> TakeFrame(MediaContext &ctx);
    static std::shared_ptr<AVFrame> SeekAndTakeFrame(MediaContext &ctx,
                                                     int streamIndex,
                                                     std::int64_t start,
                                                     std::int64_t timestamp);

    std::shared_ptr<AVFrame> ReturnFrame();
    std::shared_ptr<AVFrame> SeekAndReturnFrame(Nanoseconds timestamp);
    std::shared_ptr<AVFrame> DropFramesUntilTimestamp(std::shared_ptr<AVFrame> currentFrame,
                                                      std::int64_t target);
};

} //namespace vd

#endif //VDOWNLOADER_VD_VIDEO_STREAM_H_