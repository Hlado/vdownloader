#include "OpenH264Decoder.h"
#include "Errors.h"
#include "Utils.h"

#include <libyuv.h>

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

    SDecodingParam sDecParam{.sVideoProperty = SVideoProperty{.size = sizeof(SVideoProperty),
                                                              .eVideoBsType = VIDEO_BITSTREAM_AVC}};
    mDecoder->Initialize(&sDecParam);
}

namespace
{

I420Image ToI420Image(const SBufferInfo &info)
{
    return I420Image{.y = reinterpret_cast<std::byte *>(info.pDst[0]),
                     .u = reinterpret_cast<std::byte *>(info.pDst[1]),
                     .v = reinterpret_cast<std::byte *>(info.pDst[2]),
                     .yStride = IntCast<std::size_t>(info.UsrData.sSystemBuffer.iStride[0]),
                     .uvStride = IntCast<std::size_t>(info.UsrData.sSystemBuffer.iStride[1]),
                     .width = IntCast<std::size_t>(info.UsrData.sSystemBuffer.iWidth),
                     .height = IntCast<std::size_t>(info.UsrData.sSystemBuffer.iHeight)};
}

void AssertImageFormat(const SBufferInfo &info)
{
    auto format = info.UsrData.sSystemBuffer.iFormat;
    if(format != videoFormatI420)
    {
        throw ArgumentError{std::format("image format({}) is not I420", format)};
    }
}

}//unnamed namespace

std::optional<ArgbImage> OpenH264Decoder::Decode(std::span<const std::byte> nalUnit)
{
    SBufferInfo info;
    unsigned char *data[3];

    //practically impossible, but...
    if(WouldOverflowAdd<std::size_t>(nalUnit.size_bytes(),4u))
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

    if(info.iBufferStatus == 1)
    {
        AssertImageFormat(info);
        return ToArgb(ToI420Image(info));
    }

    return std::nullopt;
}

std::optional<ArgbImage> OpenH264Decoder::Retrieve()
{
    SBufferInfo info;
    unsigned char *data[3];

    auto err = mDecoder->FlushFrame(data, &info);

    if(err != 0)
    {
        throw LibraryCallError{"ISVCDecoder::FlushFrame", err};
    }

    if(info.iBufferStatus == 1)
    {
        AssertImageFormat(info);
        return ToArgb(ToI420Image(info));
    }

    return std::nullopt;
}

void OpenH264Decoder::Deleter::operator()(ISVCDecoder *p) const
{
    p->Uninitialize();
    WelsDestroyDecoder(p);
}

}//namespace vd