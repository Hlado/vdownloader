#ifndef VDOWNLOADER_VD_DECODER_H_
#define VDOWNLOADER_VD_DECODER_H_

#include "Track.h"

#include <functional>
#include <span>

namespace vd
{

class Decoder final
{
public:
    explicit Decoder(AvcDecConfigRecord config);

    static void Decode(const AvcDecConfigRecord &config,
                       const Segment &segment,
                       const std::function<void (const std::byte *, std::size_t)> &handler);
    void Decode(const Segment &segment,
                const std::function<void (const std::byte *, std::size_t)> &handler) const;
    
private:
    AvcDecConfigRecord mAvcConfig;
};

} //namespace vd

#endif //VDOWNLOADER_VD_DECODER_H_