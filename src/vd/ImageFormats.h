#ifndef VD_IMAGE_FORMATS_H_
#define VD_IMAGE_FORMATS_H_

#include "VideoUtils.h"

#include <filesystem>
#include <span>
#include <vector>

namespace vd
{

//Writes pixels in RGBA format
void WriteJpg(const std::filesystem::path &path,
              const Rgb32Image &rgbaImage,
              std::uint8_t quality = 90);

//Writes pixels in RGBA format
void WritePng(const std::filesystem::path &path,
              const Rgb32Image &rgbaImage);

//Writes pixels as is, so pixels representation must be BGRA
void WriteTga(std::ostream &stream,
              const Rgb32Image &bgraImage);
void WriteTga(const std::filesystem::path &path,
              const Rgb32Image &bgraImage);

}//namespace vd

#endif //VD_IMAGE_FORMATS_H_