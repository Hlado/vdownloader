#include "Options.h"
#include "Errors.h"
#include "Utils.h"

#include <args.hxx>

#include <regex>

namespace vd
{

namespace
{

template <StdDurationConcept T>
T Convert(const std::string &str)
{
    if(!str.empty())
    {
        return T{std::stoi(str)};
    }

    return T{0};
}

std::chrono::nanoseconds ParseTimestamp(const std::string &str)
{
    using namespace std::chrono_literals;

    static auto re = std::regex(R"(^(?=.)(?:(\d+)s)?(?:(\d+)ms)?$)");

    auto matches = std::smatch{};
    if(!std::regex_match(str,matches,re))
    {
        throw ArgumentError(std::format(R"(segment timestamp "{}" has unknown format)", str));
    }

    auto msecs = Convert<std::chrono::milliseconds>(matches[2].str());
    if(msecs >= 1000ms)
    {
        throw RangeError(std::format(R"(milliseconds "{}" must be in range [0,1000))", matches[2].str()));
    }

    return Convert<std::chrono::seconds>(matches[1].str()) + msecs;
}

Options::Segment ParseSegment(const std::string &fromStr, const std::string &toStr, const std::string &numFramesStr)
{
    auto from = ParseTimestamp(fromStr);
    auto to = ParseTimestamp(toStr);

    if(from >= to)
    {
        throw ArgumentError(std::format("segment finish ({}) must be greater than start ({})", toStr, fromStr));
    }

    return Options::Segment {.from = std::move(from),
                             .to = std::move(to),
                             .numFrames = numFramesStr.empty() ? 0 : IntCast<std::uint32_t>(std::stoi(numFramesStr))};
}

Options::Segment ParseSegment(const std::string &str)
{
    static auto re = std::regex(R"(^(.+)-(.+?)(?::(\d+))?$)");

    auto matches = std::smatch{};
    if(!std::regex_match(str,matches,re))
    {
        throw ArgumentError(std::format(R"(segment string "{}" has unknown format)", str));
    }

    return ParseSegment(matches[1].str(), matches[2].str(), matches[3].str());
}

std::vector<Options::Segment> ParseSegments(const args::PositionalList<std::string> &segments)
{
    std::vector<Options::Segment> res;
    for(auto &segment : segments)
    {
        res.push_back(ParseSegment(segment));
    }

    return res;
}

}//unnamed namespace



std::optional<Options> ParseOptions(int argc, const char * const * argv)
{
    args::ArgumentParser parser("Video frame extractor program");
    args::HelpFlag help(parser, "help", "Show help", {'h', "help"});
    args::ValueFlag<std::string> format(
        parser,
        "format",
        "File names format (n - segment index (1-based), i - frame index (1-based), t - frame timestamp in XsYms format)",
        {'f',"format"},
        "s{n}f{i}({t}).tga");
    args::Positional<std::string> source(parser, "source", "Video source (url/file)", args::Options::Required);
    args::PositionalList<std::string> segments(
        parser,
        "segments",
        "Video segments to extract <from-to[:n]>... (from/to - timestamps in XsYms format, n - number of frames to extract besides first and last)",
        args::Options::Required);

    try
    {
        parser.ParseCLI(argc, argv);

        return Options{.format = format.Get(),
                       .videoUrl = source.Get(),
                       .segments = ParseSegments(segments)};
    }
    catch (args::Help)
    {
        Println(parser);
        return std::nullopt;
    }
}

}//namespace vd