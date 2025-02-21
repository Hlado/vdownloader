#ifndef VDOWNLOADER_VD_VIDEO_STREAM_H_
#define VDOWNLOADER_VD_VIDEO_STREAM_H_

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




struct Image final
{
    std::vector<std::byte> data;
    std::size_t width;
    std::size_t height;
};



class Frame final
{
public:
    static inline constexpr Nanoseconds sentinelTs = Nanoseconds::max();

    Frame(std::shared_ptr<const AVFrame> rawFrame,
          Nanoseconds timestamp,
          Nanoseconds duration);

    static Frame Create(std::shared_ptr<const AVFrame> rawFrame,
                        AVRational timeBase);

    Image ArgbImage() const;
    Image BgraImage() const;
    Image RgbaImage() const;

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
using ReaderFactory = std::function<std::unique_ptr<libav::Reader>()>;

struct MediaParams final
{
    StreamPicker picker;
    bool skipNonRef;

    //Picks first stream, no skipping allowed
    static MediaParams Default();
};

VideoStream OpenMediaSource(ReaderFactory readerFactory,
                            MediaParams params = MediaParams::Default());



struct MediaContext;

class VideoStream final
{
    friend VideoStream OpenMediaSource(ReaderFactory readerFactory,
                                       MediaParams params);

private:
    VideoStream(std::unique_ptr<MediaContext> activeContext,
                std::unique_ptr<MediaContext> seekingContext,
                AVStream &stream,
                bool skipNonRef = false);

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



namespace internal
{

libav::UniquePtr<AVFrame> ConvertFrame(const AVFrame &frame, AVPixelFormat format);
//Only ARGB/RGBA is supported
Image ToImage(const AVFrame &frame, AVPixelFormat format);

}//namespace internal

} //namespace vd

#endif //VDOWNLOADER_VD_VIDEO_STREAM_H_