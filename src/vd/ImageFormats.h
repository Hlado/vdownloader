#ifndef VD_PNG_H_
#define VD_PNG_H_

#include <filesystem>
#include <span>
#include <vector>

namespace vd
{

//Writes pixels in RGBA format
void WriteJpg(const std::filesystem::path &path,
              std::span<const std::byte> rgbaData,
              std::size_t width,
              std::uint8_t quality = 90);

//Writes pixels in RGBA format
void WritePng(const std::filesystem::path &path,
              std::span<const std::byte> rgbaData,
              std::size_t width);

//Writes pixels as is, so pixels representation must be BGRA
void WriteTga(std::ostream &stream,
              std::span<const std::byte> bgraData,
              std::size_t width);
void WriteTga(const std::filesystem::path &path,
              const std::vector<std::byte> &pixels,
              std::size_t width);

}//namespace vd

#endif //VD_PNG_H_