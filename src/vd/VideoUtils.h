#ifndef VDOWNLOADER_VD_VIDEO_UTILS_H_
#define VDOWNLOADER_VD_VIDEO_UTILS_H_

#include <chrono>
#include <span>

namespace vd
{

using Nanoseconds = std::chrono::nanoseconds;



//Class to represent 4-byte RGB images with unspecified component order
//assuming by default left-to-right, top-to-bottom pixel orderind, without
//paddings
class Rgb32Image final
{
public:
    Rgb32Image(std::size_t width, std::size_t height);

    bool operator==(const Rgb32Image &r) const;
    bool operator!=(const Rgb32Image &r) const;

    //Ok, here we give up and use uint8_t instead of bytes to avoid exhausting
    //conversions, because 3rdparty code is not aware of bytes
    std::span<std::uint8_t> Data();
    std::span<const std::uint8_t> Data() const;
    std::size_t Width() const;
    std::size_t Height() const;
    std::size_t RowSize() const;
    std::span<std::uint8_t, 4> At(std::size_t x, std::size_t y);
    std::span<const std::uint8_t, 4> At(std::size_t x, std::size_t y) const;
    void Resize(std::size_t width, std::size_t height);

private:
    std::vector<std::uint8_t> mData;
    std::size_t mWidth;

    std::size_t Offset(std::size_t x, std::size_t y);
    void AssertInside(std::size_t x, std::size_t y);
    void AssertDimensionsGood(std::size_t width, std::size_t height);
};

}//namespace vd

#endif //VDOWNLOADER_VD_VIDEO_UTILS_H_