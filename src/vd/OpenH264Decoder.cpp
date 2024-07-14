#include "OpenH264Decoder.h"
#include "Errors.h"
#include "Utils.h"

namespace vd
{

using namespace literals;

OpenH264Decoder::OpenH264Decoder()
    : mUnit{0_b, 0_b, 0_b, 1_b}
{
    ISVCDecoder *pDecoder;
    if(auto err = WelsCreateDecoder(&pDecoder); err != 0)
    {
        throw LibraryCallError{"WelsCreateDecoder", err};
    }

    mDecoder.reset(pDecoder);

    SDecodingParam sDecParam{.sVideoProperty = SVideoProperty{.eVideoBsType = VIDEO_BITSTREAM_AVC}};
    mDecoder->Initialize(&sDecParam);
}

SBufferInfo OpenH264Decoder::Decode(std::span<const std::byte> nalUnit)
{
    SBufferInfo info;
    unsigned char *data[3];

    //practically impossible, but...
    if(UintOverflow<std::size_t>(nalUnit.size_bytes(),4))
    {
        throw ArgumentError{"unit is too long"};
    }
    mUnit.resize(4);
    mUnit.insert(mUnit.end(), nalUnit.begin(), nalUnit.end());

    auto err =
        mDecoder->DecodeFrameNoDelay(reinterpret_cast<const unsigned char *>(mUnit.data()),
                                     UintCast<int>(mUnit.size()),
                                     data,
                                     &info);
    if(err != 0)
    {
        throw LibraryCallError{"ISVCDecoder::DecodeFrameNoDelay", err};
    }

    return info;
}

void OpenH264Decoder::Deleter::operator()(ISVCDecoder *p) const
{
    p->Uninitialize();
    WelsDestroyDecoder(p);
}

}//namespace vd