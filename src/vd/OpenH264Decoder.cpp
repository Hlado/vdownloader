#include "OpenH264Decoder.h"
#include "Errors.h"
#include "Utils.h"

#include <libyuv.h>

namespace vd
{

using namespace literals;

namespace
{

struct I420Image final
{
    const std::byte *y;
    const std::byte *u;
    const std::byte *v;
    std::size_t yStride;
    std::size_t uvStride;
    std::size_t width;
    std::size_t height;
};

ArgbImage ToArgb(const I420Image &img)
{
    //Seems like endianess of pixels depends on machine endianess,
    //big-endian hardware is a rare beast, so just don't care about it for now
    static_assert(std::endian::native == std::endian::little);

    auto ret = ArgbImage{.data = std::vector<std::byte>(Mul<std::size_t>(img.width, img.height, 4u)),
                         .width = img.width,
                         .height = img.height};

    auto out = reinterpret_cast<std::uint8_t *>(ret.data.data());
    auto argbStride = IntCast<int>(img.width * 4);

    auto err = libyuv::I420ToARGB(reinterpret_cast<const std::uint8_t *>(img.y), IntCast<int>(img.yStride),
                                  reinterpret_cast<const std::uint8_t *>(img.u), IntCast<int>(img.uvStride),
                                  reinterpret_cast<const std::uint8_t *>(img.v), IntCast<int>(img.uvStride),
                                  out,
                                  argbStride,
                                  IntCast<int>(img.width), IntCast<int>(img.height));
    if(err != 0)
    {
        throw LibraryCallError{"libyuv::I420ToARGB", err};
    }

    return ret;
}

}//unnamed namespace



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