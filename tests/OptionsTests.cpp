#include <vd/Options.h>
#include <vd/Errors.h>

#include <args.hxx>
#include <gtest/gtest.h>

#include <array>

using namespace std::chrono_literals;
using namespace vd;

namespace
{

template <size_t N>
std::optional<Options> Parse(const std::array<const char *, N> &argv)
{
    return ParseOptions((int)argv.size(), argv.data());
}

TEST(OptionsTests, HelpRequested)
{
    auto argv = std::array{"app_path","--help"};
    
    ASSERT_FALSE(Parse(argv));
}

TEST(OptionsTests, MissingSource)
{
    auto argv = std::array{"app_path"};
    
    ASSERT_THROW(Parse(argv), args::RequiredError);
}

TEST(OptionsTests, MissingSegments)
{
    auto argv = std::array{"app_path", "url"};
    
    ASSERT_THROW(Parse(argv), args::RequiredError);
}

TEST(OptionsTests, BadSegment)
{
    auto argv = std::array{"app_path", "url", "10s-20ms"};
    ASSERT_THROW(Parse(argv), Error);

    argv[2] = "10s-10s";
    ASSERT_THROW(Parse(argv), ArgumentError);

    argv[2] = "1s-1020ms";
    ASSERT_THROW(Parse(argv), RangeError);
}

TEST(OptionsTests, DefaultFormat)
{
    auto argv = std::array{"app_path", "url", "1s500ms-2s300ms:22"};
    auto options = Parse(argv);
    ASSERT_TRUE(options);
    ASSERT_EQ("s{n}f{i}({t}).tga", options->format);
}

TEST(OptionsTests, CorrectSegmentFull)
{
    auto argv = std::array{"app_path", "-f", "some_format", "url", "1s500ms-2s300ms:22"};
    auto options = Parse(argv);
    ASSERT_TRUE(options);
    ASSERT_EQ("some_format", options->format);
    ASSERT_EQ("url", options->videoUrl);
    ASSERT_EQ(1, options->segments.size());
    ASSERT_EQ(22, options->segments[0].numFrames);
    ASSERT_EQ(1500ms, options->segments[0].from);
    ASSERT_EQ(2300ms, options->segments[0].to);
}

TEST(OptionsTests, CorrectSegmentOmittedNumFrames)
{
    auto argv = std::array{"app_path", "url", "1s500ms-2s300ms"};
    auto options = Parse(argv);
    ASSERT_TRUE(options);
    ASSERT_EQ(0, options->segments[0].numFrames);
}

TEST(OptionsTests, CorrectSegmentOmittedSeconds)
{
    auto argv = std::array{"app_path", "url", "1s-2s"};
    auto options = Parse(argv);
    ASSERT_TRUE(options);
    ASSERT_EQ(1s, options->segments[0].from);
    ASSERT_EQ(2s, options->segments[0].to);
}

TEST(OptionsTests, CorrectSegmentOmittedMilliseconds)
{
    auto argv = std::array{"app_path", "url", "500ms-800ms"};
    auto options = Parse(argv);
    ASSERT_TRUE(options);
    ASSERT_EQ(500ms, options->segments[0].from);
    ASSERT_EQ(800ms, options->segments[0].to);
}

TEST(OptionsTests, CorrectMultipleSegments)
{
    auto argv = std::array{"app_path", "url", "500ms-800ms", "1s-2s"};
    auto options = Parse(argv);
    ASSERT_TRUE(options);
    ASSERT_EQ(2, options->segments.size());
    ASSERT_EQ(500ms, options->segments[0].from);
    ASSERT_EQ(800ms, options->segments[0].to);
    ASSERT_EQ(1s, options->segments[1].from);
    ASSERT_EQ(2s, options->segments[1].to);
}

}//unnamed namespace