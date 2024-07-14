#ifndef VDOWNLOADER_VD_DECODER_H_
#define VDOWNLOADER_VD_DECODER_H_

#include "OpenH264Decoder.h"
#include "Track.h"

#include <functional>
#include <optional>
#include <span>

namespace vd
{

//temporarily, obviously
using RgbImage = SBufferInfo;

class Decoder final
{
public:
    Decoder(AvcDecConfigRecord config, Segment segment);

    std::optional<RgbImage> GetFrame();
    
private:
    OpenH264Decoder mDecoder;
    AvcDecConfigRecord mAvcConfig;
    Segment mSegment;
    std::optional<std::span<const std::byte>> mRemainder;

    void Initialize();
};

} //namespace vd

#endif //VDOWNLOADER_VD_DECODER_H_