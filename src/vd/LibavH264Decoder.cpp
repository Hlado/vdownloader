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



LibavH264Decoder::DecodedImage::DecodedImage(FramePointer &&frame, std::function<void(FramePointer &&)> backToPool)
    : frame{std::move(frame)},
      backToPool{backToPool}
{

}

LibavH264Decoder::DecodedImage::~DecodedImage()
{
    try
    {
        backToPool(std::move(frame));
    }
    catch(...) {}
}

ArgbImage LibavH264Decoder::DecodedImage::Image() const
{
    return ToArgb(ToI420Image(*frame));
}



LibavH264Decoder::LibavH264Decoder(std::uint8_t numThreads)
    : mUnit{0_b, 0_b, 0_b, 1_b},
      mFramesPool{std::make_shared<std::vector<FramePointer>>()}
{
    if(mAvCodec = avcodec_find_decoder(AV_CODEC_ID_H264); !mAvCodec)
    {
        throw Error{"can't find H264 decoder"};
    }

    if(mCodecCtx.reset(avcodec_alloc_context3(mAvCodec)); !mCodecCtx)
    {
        throw Error{"failed to create H264 codec context"};
    }

    if(mAvCodec->capabilities & AV_CODEC_CAP_FRAME_THREADS)
    {
        //It's unclear what happens if numThreads is 0, but we assume - nothing wrong
        mCodecCtx->thread_count = numThreads;
        mCodecCtx->thread_type = FF_THREAD_FRAME;
    }
    if(auto err = avcodec_open2(mCodecCtx.get(), mAvCodec, nullptr); err != 0)
    {
        throw LibraryCallError{"avcodec_open2", err};
    }

    if(mParserCtx.reset(av_parser_init(mAvCodec->id)); !mParserCtx)
    {
        throw Error{"failed to create codec parser context"};
    }

    if(mPacket.reset(av_packet_alloc()); !mPacket)
    {
        throw Error{"failed to allocate packet"};
    }
}

std::optional<LibavH264Decoder::DecodedImage>
    LibavH264Decoder::Decode(std::span<const std::byte> nalUnit)
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
std::optional<LibavH264Decoder::DecodedImage> LibavH264Decoder::Retrieve()
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
        auto frame = FromPool();
        ret = avcodec_receive_frame(mCodecCtx.get(), frame.get());

        if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
        {
            break;
        }
        else if(ret != 0)
        {
            throw LibraryCallError{"avcodec_receive_frame", ret};
        }
        
        AssertImageFormat(*frame);
        mReadyFrames.push_back(std::move(frame));
    }
}

std::optional<LibavH264Decoder::DecodedImage>
    LibavH264Decoder::ReturnFrame()
{
    if(mReadyFrames.empty())
    {
        return std::nullopt;
    }

    auto frame = std::move(mReadyFrames.front());
    mReadyFrames.pop_front();

    std::weak_ptr<std::vector<FramePointer>> pool;
    DecodedImage res{
        std::move(frame),
        [pool](FramePointer &&frame)
            {
                if(pool.expired())
                {
                    return;
                }
                
                pool.lock()->push_back(std::move(frame));
            }
        };

    return res;
}

LibavH264Decoder::FramePointer LibavH264Decoder::FromPool()
{
    if(!mFramesPool->empty())
    {
        auto res = std::move(mFramesPool->back());
        mFramesPool->pop_back();
        return res;
    }

    auto res = FramePointer{av_frame_alloc()};
    if(!res)
    {
        throw Error{"failed to allocate frame"};
    }

    return res;
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