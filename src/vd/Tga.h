#ifndef VD_TGA_H_
#define VD_TGA_H_

#include <filesystem>
#include <iostream>
#include <vector>

namespace vd
{

//Writes pixels as is, so pixels representation must be little-endian ARGB
void WriteTga(std::ostream &stream, const std::vector<std::byte> &pixels, std::size_t width);
void WriteTga(const std::filesystem::path &path, const std::vector<std::byte> &pixels, std::size_t width);

}//namespace vd

#endif //VD_TGA_H_