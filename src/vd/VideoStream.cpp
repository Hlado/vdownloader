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

UniquePtr<AVFrame> ConvertFrame(const AVFrame &frame, AVPixelFormat format);
//Only ARGB/RGBA is supported
Rgb32Image ToImage(const AVFrame &frame, AVPixelFormat format);

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

Frame::Frame(std::shared_ptr<const AVFrame> rawFrame,
             AVRational timeBase)
    : mFrame(std::move(rawFrame)),
      mTimestamp(sentinelTs)
{
    //Logic behind rounding here is that we don't want frame to be presented
    //later than original and it's duration to be less than original
    if(mFrame->pts != AV_NOPTS_VALUE)
    {
        mTimestamp = ToNano(mFrame->pts, timeBase, AV_ROUND_ZERO);
    }

    mDuration = ToNano(mFrame->duration, timeBase, AV_ROUND_INF);
}

Rgb32Image Frame::ArgbImage() const
{
    return ToImage(*mFrame, AV_PIX_FMT_ARGB);
}

Rgb32Image Frame::RgbaImage() const
{
    return ToImage(*mFrame, AV_PIX_FMT_RGBA);
}

Rgb32Image Frame::BgraImage() const
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

namespace
{

std::pair<std::unique_ptr<MediaContext>, AVStream *>
    CreateMediaContext(std::unique_ptr<Reader> &&reader,
                       const StreamPicker &picker,
                       bool skipNonRef);
void AssertPtsIsSet(const AVFrame &frame);
void AssertNextPtsIsNotLess(const AVFrame &l, const AVFrame &r);

}//unnamed namespace



VideoStream OpenMediaSource(ReaderFactory readerFactory,
                            OpeningParams params)
{

    using namespace std::ranges::views;

    auto [activeCtx, stream] = CreateMediaContext(readerFactory(), params.picker, params.skipNonRef);

    auto picker =
        [&stream](auto)
        {
            return IntCast<std::size_t>(stream->index);
        };
    auto [seekingCtx, unused] = CreateMediaContext(readerFactory(), picker, params.skipNonRef);

    return VideoStream{std::move(activeCtx), std::move(seekingCtx), *stream};
}



VideoStream::VideoStream(std::unique_ptr<MediaContext> activeContext,
                         std::unique_ptr<MediaContext> seekingContext,
                         AVStream &stream)
    : mActiveCtx(std::move(activeContext)),
      mSeekingCtx(std::move(seekingContext)),
      mStream(stream)
{

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

    return Frame{std::move(frame), mStream.get().time_base};
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
        //probably it's better to throw, but for our use case it must
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
            //That means bug in libav likely,
            //if it returns nothing after seeking successfully
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
            //Just skip frames till we get requested one,
            //but first put last returned frame into queue,
            //because when there is repeated seeking to the same position,
            //it will be the one requested
            mFramesQueue.push_front(std::move(mLastReturnedFrame));

            //Actually, at this point we probably already have requested frame
            //in queue but for simplicity of code we do little probably
            //unnecessary work here
            frame = TakeFrame(*mActiveCtx);

            return DropFramesUntilTimestamp(std::move(frame), target);
        }
    }
}

std::shared_ptr<AVFrame>
    VideoStream::DropFramesUntilTimestamp(std::shared_ptr<AVFrame> frame,
                                          std::int64_t target)
{
    while(true)
    {
        if(!frame)
        {
            break;
        }

        AssertPtsIsSet(*frame);
        if(!mFramesQueue.empty())
        {
            AssertNextPtsIsNotLess(*mFramesQueue.back(), *frame);
        }

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

    //Possibilities here:
    // 
    //1) Empty queue. It means that first frame taken after seeking has greater
    //timestamp or there wasn't frame at all. In any case it's bug and throw is
    //the best option.
    //
    //2) Queue is not empty and latest frame in it is before target. It means
    //that requested seeking timestamp is past last frame but before stream
    //ending and we must clear queue and return last frame
    // 
    //3) Queue is not empty and latest frame in it has timestamp greater or
    //equal to requested timestamp, it means that we return first frame from the
    //end which has pts less or equal to target (last or next to last always)
    if(mFramesQueue.empty())
    {
        throw NotFoundError{"no acceptable frame was found"};
    } else {
        //It's guaranteed that all items in queue have pts set and pts of
        //subsequent frame is not less
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

UniquePtr<AVFormatContext> CreateFormatContext(AVIOContext *ioCtx)
{
    auto res = MakeFormatContext();
    res->pb = ioCtx;

    AVFormatContext *tmp = res.get();
    if(int err = avformat_open_input(&tmp, nullptr, nullptr, nullptr); err < 0)
    {
        //avformat_open_input frees context on failure, so we have to release to
        //avoid double deletion
        res.release();
        throw LibraryCallError{"avformat_open_input", err};
    }

    if(int err = avformat_find_stream_info(res.get(), nullptr); err < 0)
    {
        throw LibraryCallError{"avformat_find_stream_info", err};
    }

    return res;
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

UniquePtr<AVCodecContext> CreateCodecContext(const AVStream &stream)
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

std::pair<std::unique_ptr<MediaContext>, AVStream *>
    CreateMediaContext(std::unique_ptr<Reader> &&reader,
                       const StreamPicker &picker,
                       bool skipNonRef)
{
    static const int bufferSize = 1 < 15;

    if(!reader)
    {
        throw ArgumentError{R"("reader" parameter is null pointer)"};
    }

    auto ctx = std::make_unique<MediaContext>();
    ctx->reader = std::move(reader);

    ctx->ioCtx = CreateIoContext(ctx->reader.get(), bufferSize);
    ctx->formatCtx = CreateFormatContext(ctx->ioCtx.get());

    auto &stream = PickStream(*ctx->formatCtx, picker);
    for(auto s : std::span(ctx->formatCtx->streams, ctx->formatCtx->nb_streams))
    {
        if(s->index != stream.index)
        {
            s->discard = AVDISCARD_ALL;
        }
    }

    ctx->codecCtx = CreateCodecContext(stream);
    if(skipNonRef)
    {
        ctx->codecCtx->skip_frame = AVDISCARD_NONREF;
    }

    ctx->parserCtx = MakeParserContext(stream.codecpar->codec_id);
    ctx->packet = MakePacket();

    return {std::move(ctx), &stream};
}

void AssertPtsIsSet(const AVFrame &frame)
{
    if(frame.pts == AV_NOPTS_VALUE)
    {
        throw Error{"frame pts is not set, seeking is impossible"};
    }
}

void AssertNextPtsIsNotLess(const AVFrame &cur, const AVFrame &next)
{
    if(cur.pts > next.pts)
    {
        throw Error{"next frame timestamp is lesser than current"};
    }
}

} //unnamed namespace



namespace internal
{

UniquePtr<AVFrame> ConvertFrame(const AVFrame &frame, AVPixelFormat format)
{
    auto ctx = sws_getContext(frame.width, frame.height,
                              static_cast<AVPixelFormat>(frame.format),
                              frame.width, frame.height,
                              format,
                              SWS_ACCURATE_RND & SWS_FULL_CHR_H_INT,
                              NULL, NULL, NULL);
    if(ctx == nullptr)
    {
        throw Error{"failed to create sws context"};
    }
    Defer freeContext([ctx]() { sws_freeContext(ctx); });

    auto res = MakeFrame();
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
Rgb32Image ToImage(const AVFrame &frame, AVPixelFormat format)
{
    if(format != AV_PIX_FMT_ARGB &&
           format != AV_PIX_FMT_RGBA &&
           format != AV_PIX_FMT_BGRA)
    {
        throw ArgumentError{Format(R"(argument "format"({}) is not supported)",
                                   static_cast<int>(format))};
    }

    auto converted = ConvertFrame(frame, format);

    auto image = Rgb32Image{IntCast<std::size_t>(converted->width),
                            IntCast<std::size_t>(converted->height)};

    auto avSize = av_image_get_buffer_size(format,
                                           converted->width,
                                           converted->height,
                                           1);
    if(!std::cmp_equal(avSize, image.Data().size_bytes()))
    {
        throw Error{"image conversion size mismatch"};
    }

    av_image_copy_to_buffer(image.Data().data(),
                            avSize,
                            converted->data,
                            converted->linesize,
                            format,
                            converted->width,
                            converted->height,
                            1);

    return image;
}

}//namespace internal

} //namespace vd