#ifndef VDOWNLOADER_VD_LIBAVH264_DECODER_H_
#define VDOWNLOADER_VD_LIBAVH264_DECODER_H_

#include "DecodingUtils.h"

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <deque>
#include <optional>
#include <span>
#include <memory>

namespace vd
{

class LibavH264Decoder final
{
public:
    LibavH264Decoder();

    //Single NAL unit without size/start code prefix is expected
    std::optional<ArgbImage> Decode(std::span<const std::byte> nalUnit);
    std::optional<ArgbImage> Retrieve();

private:
    struct CodecContextDeleter
    {
        void operator()(AVCodecContext *p) const;
    };

    struct ParserContextDeleter
    {
        void operator()(AVCodecParserContext *p) const;
    };

    struct FrameDeleter
    {
        void operator()(AVFrame *p) const;
    };

    struct PacketDeleter
    {
        void operator()(AVPacket *p) const;
    };

    const AVCodec *mAvCodec;
    std::unique_ptr<AVCodecContext, CodecContextDeleter> mCodecCtx;
    std::unique_ptr<AVCodecParserContext, ParserContextDeleter> mParserCtx;
    std::unique_ptr<AVFrame, FrameDeleter> mFrame;
    std::unique_ptr<AVPacket, PacketDeleter> mPacket;
    //libav parser requires stream in Annex-B format,
    //so we have to prefix NAL units with 0x00000001 in this temporary buffer
    std::vector<std::byte> mUnit;
    std::deque<ArgbImage> mFrames;

    void Decode(const std::uint8_t *&start, int &size);
    void ReceiveFrames();
    std::optional<ArgbImage> ReturnFrame();
};

} //namespace vd

#endif //VDOWNLOADER_VD_LIBAVH264_DECODER_H_