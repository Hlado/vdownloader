#ifndef VDOWNLOADER_VD_DECODING_UTILS_H_
#define VDOWNLOADER_VD_DECODING_UTILS_H_

#include <vector>

namespace vd
{

struct ArgbImage final
{
    //little-endian
    std::vector<std::byte> data;
    std::size_t width;
    std::size_t height;

    static ArgbImage SentinelValue();
};

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

ArgbImage ToArgb(const I420Image &img);

} //namespace vd

#endif //VDOWNLOADER_VD_DECODING_UTILS_H_