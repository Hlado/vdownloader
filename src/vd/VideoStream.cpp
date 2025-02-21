#include "VideoStream.h"
#include "Libav.h"

#include <algorithm>
#include <future>
#include <ranges>
#include <span>

namespace vd
{

using namespace libav;

namespace internal
{

UniquePtr<AVFrame> ConvertFrame(const AVFrame &frame, AVPixelFormat format)
{
    auto ctx = sws_getContext(frame.width, frame.height, static_cast<AVPixelFormat>(frame.format),
                              frame.width, frame.height, format,
                              SWS_ACCURATE_RND & SWS_FULL_CHR_H_INT,
                              NULL, NULL, NULL);
    if(ctx == nullptr)
    {
        throw Error{"failed to create sws context"};
    }
    Defer freeContext([ctx]() { sws_freeContext(ctx); });

    auto res = MakeFrame();
    if(!res)
    {
        throw Error{"failed to create frame"};
    }

    res->format = format;
    res->width = frame.width;
    res->height = frame.height;
    if(auto err = av_frame_get_buffer(res.get(), 0); err != 0)
    {
        throw LibraryCallError{"av_frame_get_buffer", err};
    }

    sws_scale(ctx,
              frame.data,
              frame.linesize,
              0,
              frame.height,
              res->data,
              res->linesize);

    return res;
}

//Only ARGB/RGBA is supported
Image ToImage(const AVFrame &frame, AVPixelFormat format)
{
    if(format != AV_PIX_FMT_ARGB &&
           format != AV_PIX_FMT_RGBA &&
           format != AV_PIX_FMT_BGRA)
    {
        throw ArgumentError{Format(R"(argument "format"({}) is not supported)",
                                   static_cast<int>(format))};
    }

    auto converted = ConvertFrame(frame, format);

    auto numPixels = converted->width * converted->height;
    auto rowSize = IntCast<std::size_t>(converted->width * 4);
    auto dataSize = IntCast<std::size_t>(numPixels * 4);

    std::vector<std::byte> buf;
    buf.reserve(dataSize);

    for(int i = 0; i < converted->height; ++i)
    {
        auto src = reinterpret_cast<std::byte *>(converted->data[0] + converted->linesize[0]*i);
        buf.insert(buf.end(), src, src + rowSize);
    }

    return Image{ .data = std::move(buf),
                  .width = IntCast<std::size_t>(converted->width),
                  .height = IntCast<std::size_t>(converted->height)};
}

}//namespace internal

using namespace internal;



Frame::Frame(std::shared_ptr<const AVFrame> rawFrame,
             Nanoseconds timestamp,
             Nanoseconds duration)
    : mFrame(std::move(rawFrame)),
      mTimestamp(timestamp),
      mDuration(duration)
{

}

Frame Frame::Create(std::shared_ptr<const AVFrame> rawFrame,
                    AVRational timeBase)
{
    //Logic behind rounding here is that we don't want frame to be presented
    //later than original and it's duration to be less than original
    Nanoseconds timestamp = sentinelTs;
    if(rawFrame->pts != AV_NOPTS_VALUE)
    {
        timestamp = ToNano(rawFrame->pts, timeBase, AV_ROUND_ZERO);
    }

    return Frame(std::move(rawFrame),
                 timestamp,
                 ToNano(rawFrame->duration, timeBase, AV_ROUND_INF));
}

Image Frame::ArgbImage() const
{
    return ToImage(*mFrame, AV_PIX_FMT_ARGB);
}

Image Frame::RgbaImage() const
{
    return ToImage(*mFrame, AV_PIX_FMT_RGBA);
}

Image Frame::BgraImage() const
{
    return ToImage(*mFrame, AV_PIX_FMT_BGRA);
}

Nanoseconds Frame::Timestamp() const noexcept
{
    return mTimestamp;
}

Nanoseconds Frame::Duration() const noexcept
{
    return mDuration;
}



struct MediaContext final
{
    std::unique_ptr<Reader> reader;
    UniquePtr<AVFormatContext> formatCtx;
    UniquePtr<AVIOContext> ioCtx;
    UniquePtr<AVCodecContext> codecCtx;
    UniquePtr<AVCodecParserContext> parserCtx;
    UniquePtr<AVPacket> packet;
};



MediaParams MediaParams::Default()
{
    return MediaParams{ .picker = [](auto) { return 0; },
                        .skipNonRef = false};
}



namespace
{

UniquePtr<AVIOContext> CreateIoContext(Reader *reader, int bufferSize)
{
    return MakeIoContext(bufferSize,
                         0,
                         reader,
                         &Reader::ReadPacket,
                         nullptr,
                         &Reader::Seek);
}

AVStream &PickStream(AVFormatContext &ctx, const StreamPicker &picker)
{
    using namespace std::views;

    std::vector<AVStream *> streams;
    for(unsigned int i = 0; i < ctx.nb_streams; ++i)
    {
        if(ctx.streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            streams.push_back(ctx.streams[i]);
        }
    }

    auto index = picker(streams);
    if(index >= streams.size())
    {
        throw RangeError{Format("stream {} is requested, but media source contains only {}",
                                index,
                                streams.size())};
    }

    return *streams[index];
}

UniquePtr<AVCodecContext> OpenCodecContext(const AVStream &stream)
{
    auto codec = avcodec_find_decoder(stream.codecpar->codec_id);
    if(codec == nullptr)
    {
        throw Error{Format(R"(can't find decoder for codec "{}")",
                           std::string(avcodec_get_name(stream.codecpar->codec_id)))};
    }

    auto res = MakeCodecContext(codec);

    if(auto err = avcodec_parameters_to_context(res.get(), stream.codecpar); err < 0)
    {
        throw LibraryCallError{"avcodec_parameters_to_context", err};
    }

    if(auto err = avcodec_open2(res.get(), res->codec, nullptr); err != 0)
    {
        throw LibraryCallError{"avcodec_open2", err};
    }

    return res;
}

std::unique_ptr<MediaContext> MakeContext(std::unique_ptr<Reader> &&reader, const StreamPicker &picker)
{
    static const int bufferSize = 1 < 15;

    if(!reader)
    {
        throw ArgumentError{R"("reader" parameter is null pointer)"};
    }

    auto ctx = std::unique_ptr<MediaContext>{new MediaContext{}};
    ctx->reader = std::move(reader);

    ctx->formatCtx = MakeFormatContext();

    ctx->ioCtx = CreateIoContext(ctx->reader.get(), bufferSize);
    ctx->formatCtx->pb = ctx->ioCtx.get();

    AVFormatContext *tmp = ctx->formatCtx.get();
    if(int err = avformat_open_input(&tmp, nullptr, nullptr, nullptr); err < 0)
    {
        //avformat_open_input frees context on failure, so we have to release to
        //avoid double deletion
        ctx->formatCtx.release();
        throw LibraryCallError{"avformat_open_input", err};
    }

    if(int err = avformat_find_stream_info(ctx->formatCtx.get(), nullptr); err < 0)
    {
        throw LibraryCallError{"avformat_find_stream_info", err};
    }

    auto &stream = PickStream(*ctx->formatCtx, picker);
    for(auto s : std::span(ctx->formatCtx->streams, ctx->formatCtx->nb_streams))
    {
        if(s->index != stream.index)
        {
            s->discard = AVDISCARD_ALL;
        }
    }

    ctx->codecCtx = OpenCodecContext(stream);

    ctx->parserCtx = MakeParserContext(stream.codecpar->codec_id);

    ctx->packet = MakePacket();

    return ctx;
}

void AssertPtsIsSet(const AVFrame &frame)
{
    if(frame.pts == AV_NOPTS_VALUE)
    {
        throw Error{"frame pts is not set, seeking is impossible"};
    }
}

} //unnamed namespace



VideoStream OpenMediaSource(ReaderFactory readerFactory,
                            MediaParams params)
{

    using namespace std::ranges::views;

    auto activeCtx = MakeContext(readerFactory(), params.picker);

    auto streams = std::span<AVStream *>(activeCtx->formatCtx->streams,
                                         activeCtx->formatCtx->nb_streams);
    auto stream = *(streams | filter([](auto s) { return s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO; })).begin();

    auto seekingCtx = MakeContext(readerFactory(), [stream](auto) { return stream->index; });

    return VideoStream{std::move(activeCtx), std::move(seekingCtx), *stream};
}

VideoStream::VideoStream(std::unique_ptr<MediaContext> activeContext,
                         std::unique_ptr<MediaContext> seekingContext,
                         AVStream &stream,
                         bool skipNonRef)
    : mActiveCtx(std::move(activeContext)),
      mSeekingCtx(std::move(seekingContext)),
      mStream(stream)
{
    if(skipNonRef)
    {
        mActiveCtx->codecCtx->skip_frame = AVDISCARD_NONREF;
        mSeekingCtx->codecCtx->skip_frame = AVDISCARD_NONREF;
    }
}

VideoStream::VideoStream(VideoStream &&other) = default;
VideoStream &VideoStream::operator=(VideoStream &&other) = default;
VideoStream::~VideoStream() = default;

std::optional<Frame> VideoStream::NextFrame(Nanoseconds timestamp)
{
    std::shared_ptr<AVFrame> frame;

    if(timestamp == Frame::sentinelTs)
    {
        frame = ReturnFrame();
    } else {
        frame = SeekAndReturnFrame(timestamp);
    }

    if(!frame)
    {
        return std::nullopt;
    }

    return Frame::Create(std::move(frame), mStream.get().time_base);
}

std::shared_ptr<AVFrame> VideoStream::TakeFrame(MediaContext &ctx)
{
    auto frame = MakeFrame();

    while(true)
    {
        auto err = avcodec_receive_frame(ctx.codecCtx.get(), frame.get());
        if(err >= 0)
        {
            return frame;
        } else {
            if(err != AVERROR_EOF && err != AVERROR(EAGAIN))
            {
                throw LibraryCallError{"avcodec_receive_frame", err};
            }
        }
            
        //We assume that freshly allocated packet has data set to null pointer,
        //although it's not stated clearly in docs it seems
        if(ctx.packet->data != nullptr)
        {
            av_packet_unref(ctx.packet.get());
        }

        AVPacket *packetPtr = ctx.packet.get();
        err = av_read_frame(ctx.formatCtx.get(), packetPtr);
        if(err < 0)
        {
            if(err == AVERROR_EOF)
            {
                packetPtr = nullptr;
            }
        }

        err = avcodec_send_packet(ctx.codecCtx.get(), packetPtr);
        if(err != 0)
        {
            //It means we flushed decoder already, so no more frames
            if(err == AVERROR_EOF)
            {
                return nullptr;
            }

            throw LibraryCallError{"avcodec_send_packet", err};
        }
    }
}

std::shared_ptr<AVFrame> VideoStream::SeekAndTakeFrame(MediaContext &ctx,
                                                       int streamIndex,
                                                       std::int64_t start,
                                                       std::int64_t timestamp)
{
    auto err = avformat_seek_file(ctx.formatCtx.get(),
                                  streamIndex,
                                  start,
                                  timestamp,
                                  timestamp,
                                  0);
            
    if(err < 0)
    {
        throw LibraryCallError{"avformat_seek_file", err};
    }

    avcodec_flush_buffers(ctx.codecCtx.get());

    return TakeFrame(ctx);
}

std::shared_ptr<AVFrame> VideoStream::ReturnFrame()
{
    if(!mFramesQueue.empty())
    {
        auto frame = mFramesQueue.front();
        mFramesQueue.pop_front();
        mLastReturnedFrame = frame;
        return frame;
    }

    auto frame = TakeFrame(*mActiveCtx);

    return frame;
}

std::shared_ptr<AVFrame> VideoStream::SeekAndReturnFrame(Nanoseconds timestamp)
{
    auto &stream = mStream.get();

    auto target = FromNano(timestamp, stream.time_base, AV_ROUND_ZERO);

    if(target > stream.duration)
    {
        throw RangeError{"attempted seeking past the end of stream"};
    }

    auto startTime = std::int64_t{0};
    if(stream.start_time != AV_NOPTS_VALUE)
    {
        //It's not clear for what type of streams it may be the case,
        //probably it's better to throw but for our use case it must
        //be impossible
        startTime = stream.start_time;
    }

    if(!mLastReturnedFrame)
    {
        //Stream is just created or there was some failure, in any case we just seek and return
        auto frame = SeekAndTakeFrame(*mActiveCtx, stream.index, startTime, startTime + target);
        mFramesQueue.clear();

        return DropFramesUntilTimestamp(std::move(frame), target);
    } else {
        AssertPtsIsSet(*mLastReturnedFrame);

        auto frame = SeekAndTakeFrame(*mSeekingCtx, stream.index, startTime, startTime + target);

        if(!frame)
        {
            //Again that means bug in libav likely
            mLastReturnedFrame.reset();
            mFramesQueue.clear();
            return nullptr;
        }

        AssertPtsIsSet(*frame);

        if(frame->pts >= mLastReturnedFrame->pts || mLastReturnedFrame->pts > target)
        {
            //Seeking is faster
            mLastReturnedFrame.reset();
            mFramesQueue.clear();
            mActiveCtx.swap(mSeekingCtx);

            return DropFramesUntilTimestamp(std::move(frame), target);
        } else {
            //Just skip frames till we get requested, but first put last returned frame into queue
            mFramesQueue.push_front(std::move(mLastReturnedFrame));

            //Actually, at this point we probably already have needed frame in queue but
            //for simplicity of code we do little unnecessary (possibly) work here
            frame = TakeFrame(*mActiveCtx);

            return DropFramesUntilTimestamp(std::move(frame), target);
        }
    }
}

std::shared_ptr<AVFrame>
    VideoStream::DropFramesUntilTimestamp(std::shared_ptr<AVFrame> currentFrame,
                                          std::int64_t target)
{
    auto frame = std::move(currentFrame);

    while(true)
    {
        if(!frame)
        {
            break;
        }

        AssertPtsIsSet(*frame);

        mFramesQueue.push_back(frame);

        if(frame->pts >= target)
        {
            break;
        }

        //This is needed to free memory as soon as possible and usually we
        //really don't need more than 2 frames in queue, except one case
        //when we seek immediately after another seek and there are already
        //2 frames, so we have to keep this check after timestamp check
        if(mFramesQueue.size() > 2)
        {
            mFramesQueue.erase(mFramesQueue.begin(), std::prev(mFramesQueue.end(), 2));
        }

        frame = TakeFrame(*mActiveCtx);
    }

    //Possibilities here - empty queue, it means there is a bug in libav likely,
    //but we don't care and pretend there is no more frames
    //
    //Or if queue is not empty, there is also 2 possibilities, first is that
    //last added frame is before target - it means we requested seek near the
    //end and we must clear queue and return that last frame or
    // 
    //We found frame with pts >= target, then we return first frame from
    //end with pts <= target (it may be last or second to last) and clear queue
    //up to this frame (inclusive)
    if(mFramesQueue.empty())
    {
        return nullptr;
    } else {
        //It's guaranteed that all items in queue have pts set
        if(mFramesQueue.back()->pts < target)
        {
            mLastReturnedFrame = std::move(mFramesQueue.back());
            mFramesQueue.clear();
            return mLastReturnedFrame;
        } else {
            auto it = std::find_if(mFramesQueue.rbegin(),
                                   mFramesQueue.rend(),
                                   [target](auto f) { return f->pts <= target; });
            mLastReturnedFrame = std::move(*it);
            mFramesQueue.erase(mFramesQueue.begin(), it.base());
            return mLastReturnedFrame;
        }
    }
}

} //namespace vd