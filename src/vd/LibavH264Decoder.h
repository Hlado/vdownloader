#ifndef VDOWNLOADER_VD_LIBAVH264_DECODER_H_
#define VDOWNLOADER_VD_LIBAVH264_DECODER_H_

#include "DecodingUtils.h"

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <memory>

namespace vd
{

class LibavH264Decoder final
{
private:
    struct FrameDeleter
    {
        void operator()(AVFrame *p) const;
    };
    
    using FramePointer = std::unique_ptr<AVFrame, FrameDeleter>;



public:
    class DecodedImage final
    {
    public:
        DecodedImage(FramePointer &&frame, std::function<void(FramePointer &&)> backToPool);

        ~DecodedImage();
        DecodedImage(const DecodedImage &other) = delete;
        DecodedImage &operator=(const DecodedImage &other) = delete;
        DecodedImage(DecodedImage &&other) = default;
        DecodedImage &operator=(DecodedImage &&other) = default;

        ArgbImage Image() const;

    private:
        FramePointer mFrame;
        std::function<void(FramePointer &&)> mBackToPool;
    };



    explicit LibavH264Decoder(std::uint8_t numThreads = 0u);

    LibavH264Decoder(const LibavH264Decoder &other) = delete;
    LibavH264Decoder &operator=(const LibavH264Decoder &other) = delete;
    LibavH264Decoder(LibavH264Decoder &&other) = default;
    LibavH264Decoder &operator=(LibavH264Decoder &&other) = default;

    //Single NAL unit without size/start code prefix is expected
    std::optional<DecodedImage> Decode(std::span<const std::byte> nalUnit);
    std::optional<DecodedImage> Retrieve();

private:
    struct CodecContextDeleter
    {
        void operator()(AVCodecContext *p) const;
    };

    struct ParserContextDeleter
    {
        void operator()(AVCodecParserContext *p) const;
    };

    struct PacketDeleter
    {
        void operator()(AVPacket *p) const;
    };

    

    const AVCodec *mAvCodec;
    std::unique_ptr<AVCodecContext, CodecContextDeleter> mCodecCtx;
    std::unique_ptr<AVCodecParserContext, ParserContextDeleter> mParserCtx;
    std::unique_ptr<AVPacket, PacketDeleter> mPacket;
    //libav parser requires stream in Annex-B format,
    //so we have to prefix NAL units with 0x00000001 in this temporary buffer.
    //But more important is that libav requires some padding in the end and it
    //cannot be guaranteed without additional buffer like this
    std::vector<std::byte> mUnit;
    std::deque<FramePointer> mReadyFrames;
    std::shared_ptr<std::vector<FramePointer>> mFramesPool;

    void Decode(const std::uint8_t *&start, int &size);
    void ReceiveFrames();
    std::optional<DecodedImage> ReturnFrame();
    FramePointer FromPool();
};

} //namespace vd

#endif //VDOWNLOADER_VD_LIBAVH264_DECODER_H_