#ifndef VDOWNLOADER_VD_OPENH264_DECODER_H_
#define VDOWNLOADER_VD_OPENH264_DECODER_H_

#include "DecodingUtils.h"

#include <wels/codec_api.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace vd
{

class OpenH264Decoder final
{
public:
    class DecodedImage;

private:
    class DecodedImageImpl final
    {
        friend DecodedImage;
        
    public:
        //This class is supposed to be managed via shared_ptr, so copying/moving is meaningless
        DecodedImageImpl(const DecodedImageImpl &other) = delete;
        DecodedImageImpl &operator=(const DecodedImageImpl &other) = delete;
        DecodedImageImpl(DecodedImageImpl &&other) = delete;
        DecodedImageImpl &operator=(DecodedImageImpl &&other) = delete;

        DecodedImageImpl(std::shared_ptr<ISVCDecoder> decoder, const SBufferInfo &bufferInfo);

        ArgbImage Image() const;
        void Convert() const;

    private:
        std::shared_ptr<ISVCDecoder> mDecoder;
        SBufferInfo mBufferInfo;
        mutable std::optional<ArgbImage> mImage;
    };

public:
    class DecodedImage final
    {
    public:
        explicit DecodedImage(std::shared_ptr<DecodedImageImpl> impl);

        DecodedImage(const DecodedImage &other) = default;
        DecodedImage &operator=(const DecodedImage &other) = default;
        DecodedImage(DecodedImage &&other) = default;
        DecodedImage &operator=(DecodedImage &&other) = default;

        ArgbImage Image() const;

    private:
        std::shared_ptr<DecodedImageImpl> mImpl;
    };

    OpenH264Decoder();

    OpenH264Decoder(const OpenH264Decoder &other) = delete;
    OpenH264Decoder &operator=(const OpenH264Decoder &other) = delete;
    OpenH264Decoder(OpenH264Decoder &&other) = default;
    OpenH264Decoder &operator=(OpenH264Decoder &&other) = default;

    //Single NAL unit without size/start code prefix is expected
    std::optional<DecodedImage> Decode(std::span<const std::byte> nalUnit);
    std::optional<DecodedImage> Retrieve();

private:
    struct Deleter
    {
        void operator()(ISVCDecoder *p) const;
    };

    std::shared_ptr<ISVCDecoder> mDecoder;
    std::shared_ptr<DecodedImageImpl> mLastFrame;
    //unfrotunately, openh264 wants stream in Annex-B format,
    //so we have to prefix NAL units with 0x00000001 in this temporary buffer
    std::vector<std::byte> mUnit;

    std::optional<DecodedImage> ReturnImage(const SBufferInfo &info);
};

} //namespace vd

#endif //VDOWNLOADER_VD_OPENH264_DECODER_H_