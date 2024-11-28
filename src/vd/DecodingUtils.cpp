#include "DecodingUtils.h"
#include "Utils.h"

#include <libyuv.h>

namespace vd
{

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

}//namespace vd