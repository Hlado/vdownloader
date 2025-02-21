#include "LibavUtils.h"
#include "Utils.h"

namespace vd::libav
{

void CodecContextDeleter::operator()(const AVCodecContext *p) const
{
    if(p != nullptr)
    {
        avcodec_free_context(const_cast<AVCodecContext **>(&p));
    }
}

void ParserContextDeleter::operator()(const AVCodecParserContext *p) const
{
    if(p != nullptr)
    {
        av_parser_close(const_cast<AVCodecParserContext *>(p));
    }
}

void PacketDeleter::operator()(const AVPacket *p) const
{
    if(p != nullptr)
    {
        av_packet_free(const_cast<AVPacket **>(&p));
    }
}

void FormatContextDeleter::operator()(const AVFormatContext *p) const
{
    if(p != nullptr)
    {
        avformat_close_input(const_cast<AVFormatContext **>(&p));
    }
}

void IoContextDeleter::operator()(const AVIOContext *p) const
{
    if(p == nullptr)
    {
        return;
    }

    if(p->buffer != nullptr)
    {
        av_free(p->buffer);
    }
    av_free(const_cast<AVIOContext *>(p));
}

void FrameDeleter::operator()(const AVFrame *p) const
{
    if(p != nullptr)
    {
        av_frame_free(const_cast<AVFrame **>(&p));
    }
}

void GenericDeleter::operator()(void *p) const
{
    if(p != nullptr)
    {
        av_free(p);
    }
}



Reader::Reader(std::shared_ptr<SourceBase> source,
               SeekSizeMode seekSizeMode)
    : mSrc{std::move(source)},
      mSeekSizeMode{seekSizeMode}
{
    if(!mSrc)
    {
        throw ArgumentError{R"("source" parameter is null pointer)"};
    }

    if(mSeekSizeMode == SeekSizeMode::Cache)
    {
        mSizeCache = mSrc->GetContentLength();
    }
}

int Reader::Read(std::span<std::byte> buf)
{
    try
    {
        if(mPos == GetContentLength())
        {
            return AVERROR_EOF;
        }

        auto available = Sub(GetContentLength(), mPos);
        auto subspan = buf.subspan(0, std::min(buf.size_bytes(), available));
        mSrc->Read(mPos, subspan);
        mPos += subspan.size_bytes();

        return IntCast<int>(subspan.size_bytes());
    }
    catch(...)
    {
        return AVERROR_EXTERNAL;
    }
}

std::int64_t Reader::Seek(std::int64_t offset, int whence)
{
    try
    {
        if((whence & AVSEEK_SIZE) != 0)
        {
            return IntCast<std::int64_t>(GetContentLength());
        }

        std::size_t posBase{0};
        if(whence == SEEK_CUR)
        {
            posBase = mPos;
        }
        else if(whence == SEEK_END)
        {
            posBase = GetContentLength();
        }
        else if(whence != SEEK_SET)
        {
            return AVERROR_EXTERNAL;
        }

        std::size_t newPos;
        auto uOffset = IntCast<std::size_t>(std::abs(offset));
        if(offset < 0)
        {
            newPos = Sub(posBase, uOffset);
        } else {
            newPos = Add(posBase, uOffset);
        }

        if(std::cmp_greater(newPos, GetContentLength()))
        {
            return AVERROR_EXTERNAL;
        }

        mPos = newPos;
        return IntCast<std::int64_t>(mPos);
    }
    catch(...)
    {
        return AVERROR_EXTERNAL;
    }
}

int Reader::ReadPacket(void *opaque, std::uint8_t *buf, int size)
{
    auto reader = reinterpret_cast<Reader *>(opaque);
    auto span = std::span<std::uint8_t>{buf, IntCast<std::size_t>(size)};
    return reader->Read(std::as_writable_bytes(span));
}

std::int64_t Reader::Seek(void *opaque, std::int64_t offset, int whence)
{
    auto reader = reinterpret_cast<Reader *>(opaque);
    return reader->Seek(offset, whence);
}

std::size_t Reader::GetContentLength() const
{
    if(mSeekSizeMode == SeekSizeMode::Cache)
    {
        return mSizeCache;
    }

    return mSrc->GetContentLength();
}



std::chrono::nanoseconds ToNano(std::int64_t timestamp,
                                AVRational timeBase,
                                AVRounding rounding)
{
     using std::chrono::nanoseconds;

    //it's not stated clearly for this specific function but we assume that
    //INT64_MIN is returned on failure
    auto res = av_rescale_q_rnd(timestamp,
                                timeBase,
                                AVRational{.num = nanoseconds::period::num, .den = nanoseconds::period::den},
                                rounding);

    if(res == INT64_MIN)
    {
        throw RangeError{"timestamp is not representable in requested time base"};
    }

    return nanoseconds(res);
}

std::int64_t FromNano(std::chrono::nanoseconds timestamp,
                      AVRational timeBase,
                      AVRounding rounding)
{
    using std::chrono::nanoseconds;

    //it's not stated clearly for this specific function but we assume that
    //INT64_MIN is returned on failure
    auto res = av_rescale_q_rnd(timestamp.count(),
                                AVRational{.num = nanoseconds::period::num, .den = nanoseconds::period::den},
                                timeBase,
                                rounding);

    if(res == INT64_MIN)
    {
        throw RangeError{"timestamp is not representable in requested time base"};
    }

    return res;
}



UniquePtr<AVCodecContext> MakeCodecContext(const AVCodec *codec)
{
    auto res = UniquePtr<AVCodecContext>{avcodec_alloc_context3(codec)};

    if(!res)
    {
        throw Error{Format(R"(failed to create codec context for codec "{}")",
                           std::string(codec->long_name))};
    }

    return res;
}

UniquePtr<AVCodecParserContext> MakeParserContext(AVCodecID codec_id)
{
    auto res = UniquePtr<AVCodecParserContext>{av_parser_init(codec_id)};

    if(!res)
    {
        throw Error{Format(R"(failed to create codec parser context for codec "{}")",
                           std::string(avcodec_get_name(codec_id)))};
    }

    return res;
}

UniquePtr<AVPacket> MakePacket()
{
    auto res = UniquePtr<AVPacket>{av_packet_alloc()};

    if(!res)
    {
        throw Error{"failed to allocate packet"};
    }

    return res;
}

UniquePtr<AVFormatContext> MakeFormatContext()
{
    auto res = UniquePtr<AVFormatContext>{avformat_alloc_context()};

    if(!res)
    {
        throw Error{"failed to create AVFormatContext"};
    }

    return res;
}

UniquePtr<AVIOContext> MakeIoContext(
    int buffer_size,
    int write_flag,
    void *opaque,
    int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
    int (*write_packet)(void *opaque, const uint8_t *buf, int buf_size),
    int64_t (*seek)(void *opaque, int64_t offset, int whence))
{
    auto buf = MakeBuffer(IntCast<std::size_t>(buffer_size));

    auto res =
        UniquePtr<AVIOContext>(
            avio_alloc_context(
                buf.get(),
                buffer_size,
                write_flag,
                opaque,
                read_packet,
                write_packet,
                seek));

    if(!res)
    {
        throw Error{"failed to create AVIOContext"};
    }
    buf.release();

    return res;
}

UniquePtr<AVFrame> MakeFrame()
{
    auto res = UniquePtr<AVFrame>{av_frame_alloc()};

    if(!res)
    {
        throw Error{"failed to allocate frame"};
    }

    return res;
}

BufferPtr MakeBuffer(std::size_t size)
{
    auto buf =
        BufferPtr((std::uint8_t *)av_malloc(IntCast<std::size_t>(size)));

    if(!buf && size > 0)
    {
        throw Error{"failed to allocate buffer with av_malloc"};
    }

    return buf;
}

}//namespace vd