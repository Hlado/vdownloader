#include <vd/Utils.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>

using namespace vd;
//using namespace vd::internal;

TEST(UtilsTests, StrToUint)
{
    ASSERT_ANY_THROW(StrToUint<std::uint8_t>("-1"));

    ASSERT_EQ(255, StrToUint<std::uint8_t>("255"));
    ASSERT_THROW(StrToUint<std::uint8_t>("256"), RangeError);

    ASSERT_EQ(65535, StrToUint<std::uint16_t>("65535"));
    ASSERT_THROW(StrToUint<std::uint16_t>("65536"), RangeError);

    ASSERT_EQ(4294967295, StrToUint<std::uint32_t>("4294967295"));
    ASSERT_THROW(StrToUint<std::uint32_t>("4294967296"), RangeError);

    ASSERT_EQ(18446744073709551615, StrToUint<std::uint64_t>("18446744073709551615"));
    ASSERT_ANY_THROW(StrToUint<std::uint64_t>("18446744073709551616"));
}

TEST(UtilsTests, UintCast)
{
    ASSERT_EQ(10,UintCast<std::size_t>((std::uint8_t)10));
    ASSERT_EQ(255, UintCast<std::uint8_t>((std::size_t)255));
    ASSERT_THROW(UintCast<std::uint8_t>((std::size_t)256), RangeError);
}

TEST(UtilsTests, PrintfTo)
{
    std::ostringstream ss;
    PrintfTo(ss, "{1} test {0}", 1, 2);
    ASSERT_EQ("2 test 1", ss.str());
}