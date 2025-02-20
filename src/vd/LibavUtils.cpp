#include "LibavUtils.h"
#include "Utils.h"

namespace vd
{

void LibavCodecContextDeleter::operator()(const AVCodecContext *p) const
{
    if(p != nullptr)
    {
        avcodec_free_context(const_cast<AVCodecContext **>(&p));
    }
}

void LibavParserContextDeleter::operator()(const AVCodecParserContext *p) const
{
    if(p != nullptr)
    {
        av_parser_close(const_cast<AVCodecParserContext *>(p));
    }
}

void LibavPacketDeleter::operator()(const AVPacket *p) const
{
    if(p != nullptr)
    {
        av_packet_free(const_cast<AVPacket **>(&p));
    }
}

void LibavFormatContextDeleter::operator()(const AVFormatContext *p) const
{
    if(p != nullptr)
    {
        avformat_close_input(const_cast<AVFormatContext **>(&p));
    }
}

void LibavIoContextDeleter::operator()(const AVIOContext *p) const
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

void LibavFrameDeleter::operator()(const AVFrame *p) const
{
    if(p != nullptr)
    {
        av_frame_free(const_cast<AVFrame **>(&p));
    }
}

void LibavGenericDeleter::operator()(void *p) const
{
    if(p != nullptr)
    {
        av_free(p);
    }
}



LibavReader::LibavReader(std::shared_ptr<SourceBase> source,
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

int LibavReader::Read(std::span<std::byte> buf)
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

std::int64_t LibavReader::Seek(std::int64_t offset, int whence)
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

int LibavReader::ReadPacket(void *opaque, std::uint8_t *buf, int size)
{
    auto reader = reinterpret_cast<LibavReader *>(opaque);
    auto span = std::span<std::uint8_t>{buf, IntCast<std::size_t>(size)};
    return reader->Read(std::as_writable_bytes(span));
}

std::int64_t LibavReader::Seek(void *opaque, std::int64_t offset, int whence)
{
    auto reader = reinterpret_cast<LibavReader *>(opaque);
    return reader->Seek(offset, whence);
}

std::size_t LibavReader::GetContentLength() const
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

}//namespace vd