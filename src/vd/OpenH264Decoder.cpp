#include "OpenH264Decoder.h"
#include "Errors.h"
#include "Utils.h"

#include <libyuv.h>

namespace vd
{

using namespace literals;

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



OpenH264Decoder::DecodedImageImpl::DecodedImageImpl(std::shared_ptr<ISVCDecoder> decoder, const SBufferInfo &bufferInfo)
    : mDecoder{decoder},
      mBufferInfo{bufferInfo}
{

}

ArgbImage OpenH264Decoder::DecodedImageImpl::Image() const
{
    Convert();
    return mImage.value();
}

void OpenH264Decoder::DecodedImageImpl::Convert() const
{
    if(mImage)
    {
        return;
    }

    mImage = ToArgb(ToI420Image(mBufferInfo));
}



OpenH264Decoder::DecodedImage::DecodedImage(std::shared_ptr<OpenH264Decoder::DecodedImageImpl> impl)
    : mImpl{std::move(impl)}
{


}

ArgbImage OpenH264Decoder::DecodedImage::Image() const
{
    return mImpl->Image();
}



OpenH264Decoder::OpenH264Decoder()
    : mUnit{0_b, 0_b, 0_b, 1_b}
{
    ISVCDecoder *pDecoder;
    if(auto err = WelsCreateDecoder(&pDecoder); err != 0)
    {
        throw LibraryCallError{"WelsCreateDecoder", err};
    }
    mDecoder = std::shared_ptr<ISVCDecoder>{pDecoder, Deleter{}};

    SDecodingParam sDecParam{.sVideoProperty = SVideoProperty{.size = sizeof(SVideoProperty),
                                                              .eVideoBsType = VIDEO_BITSTREAM_AVC}};
    if(auto err = mDecoder->Initialize(&sDecParam); err != 0)
    {
        throw LibraryCallError{"ISVCDecoder::Initialize", err};
    }
}

std::optional<OpenH264Decoder::DecodedImage> OpenH264Decoder::Decode(std::span<const std::byte> nalUnit)
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

    return ReturnImage(info);
}

std::optional<OpenH264Decoder::DecodedImage> OpenH264Decoder::Retrieve()
{
    SBufferInfo info;
    unsigned char *data[3];

    auto err = mDecoder->FlushFrame(data, &info);

    if(err != 0)
    {
        throw LibraryCallError{"ISVCDecoder::FlushFrame", err};
    }

    return ReturnImage(info);
}

std::optional<OpenH264Decoder::DecodedImage> OpenH264Decoder::ReturnImage(const SBufferInfo &info)
{
    if(info.iBufferStatus != 1)
    {
        return std::nullopt;
    }

    AssertImageFormat(info);

    if(mLastFrame && mLastFrame.use_count() > 1)
    {
        mLastFrame->Convert();
    }
    mLastFrame = std::make_shared<DecodedImageImpl>(mDecoder, info);

    return DecodedImage{mLastFrame};
}

void OpenH264Decoder::Deleter::operator()(ISVCDecoder *p) const
{
    if(p == nullptr)
    {
        return;
    }

    p->Uninitialize();
    WelsDestroyDecoder(p);
}

}//namespace vd