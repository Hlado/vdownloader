#include "LibavH264Decoder.h"
#include "Errors.h"
#include "Utils.h"



namespace vd
{

using namespace literals;



namespace
{

I420Image ToI420Image(const AVFrame &frame)
{
    return I420Image{.y = reinterpret_cast<std::byte *>(frame.data[0]),
                     .u = reinterpret_cast<std::byte *>(frame.data[1]),
                     .v = reinterpret_cast<std::byte *>(frame.data[2]),
                     .yStride = IntCast<std::size_t>(frame.linesize[0]),
                     .uvStride = IntCast<std::size_t>(frame.linesize[1]),
                     .width = IntCast<std::size_t>(frame.width),
                     .height = IntCast<std::size_t>(frame.height)};
}

void AssertImageFormat(const AVFrame &frame)
{
    if(frame.format != AV_PIX_FMT_YUV420P)
    {
        throw ArgumentError{std::format("image format({}) is not I420", frame.format)};
    }
}

}//unnamed namespace



LibavH264Decoder::LibavH264Decoder()
    : mUnit{0_b, 0_b, 0_b, 1_b}
{
    if(mAvCodec = avcodec_find_decoder(AV_CODEC_ID_H264); !mAvCodec)
    {
        throw Error{"can't find H264 decoder"};
    }

    if(mCodecCtx.reset(avcodec_alloc_context3(mAvCodec)); !mCodecCtx)
    {
        throw Error{"failed to create H264 codec context"};
    }

    if(auto err = avcodec_open2(mCodecCtx.get(), mAvCodec, nullptr); err != 0)
    {
        throw LibraryCallError{"avcodec_open2", err};
    }

    if(mParserCtx.reset(av_parser_init(mAvCodec->id)); !mParserCtx)
    {
        throw Error{"failed to create codec parser context"};
    }

    if(mFrame.reset(av_frame_alloc()); !mFrame)
    {
        throw Error{"failed to allocate frame"};
    }

    if(mPacket.reset(av_packet_alloc()); !mPacket)
    {
        throw Error{"failed to allocate packet"};
    }
}

std::optional<ArgbImage> LibavH264Decoder::Decode(std::span<const std::byte> nalUnit)
{
    //practically impossible, but...
    if(WouldOverflowAdd<std::size_t>(nalUnit.size_bytes(), 4u, (std::size_t)AV_INPUT_BUFFER_PADDING_SIZE))
    {
        throw ArgumentError{"unit is too long"};
    }
    mUnit.resize(4);
    mUnit.insert(mUnit.end(), nalUnit.begin(), nalUnit.end());
    mUnit.insert(mUnit.end(), AV_INPUT_BUFFER_PADDING_SIZE, 0_b);

    auto size = UintCast<int>(nalUnit.size_bytes() + 4);
    auto start = reinterpret_cast<const std::uint8_t *>(mUnit.data());

    while(size > 0)
    {
        Decode(start, size);
    }

    return ReturnFrame();
}

//Ok, currently there's a problem, we need flush decoder to retrieve any queued frames, but it means that we close stream and
//can't decode more frames (unlike OpenH264). Actually, under normal circumstances higher level decoder won't try to decode
//anything after retrieval start, that's correct behavior for now but it may change eventually
std::optional<ArgbImage> LibavH264Decoder::Retrieve()
{
    const std::uint8_t *start = nullptr;
    auto size = int{0};
    Decode(start, size);

    if(auto err = avcodec_send_packet(mCodecCtx.get(), nullptr); err != 0 && err != AVERROR_EOF)
    {
        throw LibraryCallError{"avcodec_send_packet", err};
    }
    ReceiveFrames();

    return ReturnFrame();
}

void LibavH264Decoder::Decode(const std::uint8_t *&start, int &size)
{
    auto readBytes = av_parser_parse2(mParserCtx.get(),
                                      mCodecCtx.get(),
                                      &mPacket->data,
                                      &mPacket->size,
                                      start,
                                      size,
                                      0,
                                      0,
                                      0);

    start += readBytes;
    size -= readBytes;
    
    if(readBytes < 0)
    {
        throw LibraryCallError{"av_parser_parse2", readBytes};
    }

    if(mPacket->size == 0)
    {
        return;
    }

    if(auto err = avcodec_send_packet(mCodecCtx.get(), mPacket.get()); err != 0)
    {
        throw LibraryCallError{"avcodec_send_packet", err};
    }

    ReceiveFrames();
}

void LibavH264Decoder::ReceiveFrames()
{
    auto ret = int{0};
    while(ret == 0)
    {
        ret = avcodec_receive_frame(mCodecCtx.get(), mFrame.get());

        if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
        {
            break;
        }
        else if(ret != 0)
        {
            throw LibraryCallError{"avcodec_receive_frame", ret};
        }
        
        AssertImageFormat(*mFrame);
        mFrames.push_back(ToArgb(ToI420Image(*mFrame)));
    }
}

std::optional<ArgbImage> LibavH264Decoder::ReturnFrame()
{
    if(mFrames.empty())
    {
        return std::nullopt;
    }
    else
    {
        auto res = std::move(mFrames.front());
        mFrames.pop_front();
        return res;
    }
}

void LibavH264Decoder::CodecContextDeleter::operator()(AVCodecContext *p) const
{
    avcodec_free_context(&p);
}

void LibavH264Decoder::ParserContextDeleter::operator()(AVCodecParserContext *p) const
{
    av_parser_close(p);
}

void LibavH264Decoder::FrameDeleter::operator()(AVFrame *p) const
{
    av_frame_free(&p);
}

void LibavH264Decoder::PacketDeleter::operator()(AVPacket *p) const
{
    av_packet_free(&p);
}

}//namespace vd