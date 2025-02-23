#include "VideoUtils.h"
#include "Errors.h"
#include "Utils.h"

namespace vd
{

Rgb32Image::Rgb32Image(std::size_t width, std::size_t height)
{
    Resize(width, height);
}

bool Rgb32Image::operator==(const Rgb32Image &r) const
{
    return mData == r.mData;
}

bool Rgb32Image::operator!=(const Rgb32Image &r) const
{
    return !(*this == r);
}

std::span<std::uint8_t> Rgb32Image::Data()
{
    return mData;
}

std::span<const std::uint8_t> Rgb32Image::Data() const
{
    return mData;
}

std::size_t Rgb32Image::Width() const
{
    return mWidth;
}

std::size_t Rgb32Image::Height() const
{
    return mData.size() / mWidth / 4;
}

std::size_t Rgb32Image::RowSize() const
{
    return Width()*4;
}

std::span<std::uint8_t, 4> Rgb32Image::At(std::size_t x, std::size_t y)
{
    return std::span<std::uint8_t, 4>(&mData[Offset(x, y)], 4);
}

std::span<const std::uint8_t, 4> Rgb32Image::At(std::size_t x, std::size_t y) const
{
    return const_cast<Rgb32Image *>(this)->At(x, y);
}

void Rgb32Image::Resize(std::size_t width, std::size_t height)
{
    AssertDimensionsGood(width, height);
    mWidth = width;
    mData.resize(Mul(width, height, 4u));
}

std::size_t Rgb32Image::Offset(std::size_t x, std::size_t y)
{
    AssertInside(x, y);

    return RowSize()*y + x*4;
}

void Rgb32Image::AssertInside(std::size_t x, std::size_t y)
{
    if(x >= Width() || y >= Height())
    {
        throw RangeError(std::format("pixel coordinates ({};{}) are out of bounds", x, y));
    }
}

void Rgb32Image::AssertDimensionsGood(std::size_t width, std::size_t height)
{
    if(width == 0 || height == 0)
    {
        throw ArgumentError{"zero image dimension is not allowed"};
    }
}

}//namespace vd