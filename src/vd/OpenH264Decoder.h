#ifndef VDOWNLOADER_VD_OPENH264_DECODER_H_
#define VDOWNLOADER_VD_OPENH264_DECODER_H_

#include <wels/codec_api.h>

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace vd
{

class OpenH264Decoder final
{
public:
    OpenH264Decoder();

    SBufferInfo Decode(std::span<const std::byte> nalUnit);

private:
    struct Deleter
    {
        void operator()(ISVCDecoder *p) const;
    };
    
    std::unique_ptr<ISVCDecoder, Deleter> mDecoder;
    //unfrotunately, openh264 wants stream in Annex-B format,
    //so we have to prefix NAL units with 0x00000001 in this temporary buffer
    std::vector<std::byte> mUnit;
};

} //namespace vd

#endif //VDOWNLOADER_VD_OPENH264_DECODER_H_