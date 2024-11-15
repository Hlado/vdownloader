#ifndef VDOWNLOADER_VD_OPTIONS_H_
#define VDOWNLOADER_VD_OPTIONS_H_

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace vd
{

struct Options final
{
    struct Segment final
    {
        std::chrono::nanoseconds from;
        std::chrono::nanoseconds to;
        std::uint32_t numFrames;
    };

    std::string format;
    std::string videoUrl;
    std::vector<Segment> segments;
};



//Returns nullopt if help was requested
std::optional<Options> ParseOptions(int argc, const char * const * argv);

} //namespace vd

#endif //VDOWNLOADER_VD_OPTIONS_H_