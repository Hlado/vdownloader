#include <vd/VideoUtils.h>
#include <vd/Errors.h>

#include <gtest/gtest.h>

using namespace vd;

namespace
{

TEST(Rgb32Image, ZeroDimensionThrows)
{
    ASSERT_THROW(Rgb32Image(0, 1), ArgumentError);
    ASSERT_THROW(Rgb32Image(1, 0), ArgumentError);

    auto img = Rgb32Image{1, 1};
    ASSERT_THROW(img.Resize(0, 1), ArgumentError);
    ASSERT_THROW(img.Resize(1, 0), ArgumentError);
}

TEST(Rgb32Image, DimensionsAndSizes)
{
    auto img = Rgb32Image{7, 9};
    ASSERT_EQ(7, img.Width());
    ASSERT_EQ(9, img.Height());
    ASSERT_EQ(28, img.RowSize());
    ASSERT_EQ(252, img.Data().size_bytes());

    img.Resize(10, 7);
    ASSERT_EQ(10, img.Width());
    ASSERT_EQ(7, img.Height());
    ASSERT_EQ(40, img.RowSize());
    ASSERT_EQ(280, img.Data().size_bytes());
}

TEST(Rgb32Image, OutOfBoundsAccess)
{
    auto img = Rgb32Image{5, 5};
    ASSERT_THROW(img.At(4, 5), RangeError);
    ASSERT_THROW(img.At(5, 4), RangeError);
}

TEST(Rgb32Image, PixelAccessAndEquality)
{
    using u8 = std::uint8_t;

    auto img1 = Rgb32Image{5, 5};
    auto img2 = Rgb32Image{5, 5};

    std::memset(img1.Data().data(), 123, img1.Data().size_bytes());
    std::memset(img2.Data().data(), 123, img2.Data().size_bytes());
    ASSERT_TRUE(img1 == img2);
    ASSERT_FALSE(img1 != img2);

    const auto bytePos = std::size_t{69};

    ASSERT_EQ(123, img1.Data()[bytePos]);
    auto pix1 = img1.At(2, 3);
    pix1[1] = 0;
    ASSERT_EQ(0, img1.Data()[bytePos]);
    ASSERT_FALSE(img1 == img2);
    ASSERT_TRUE(img1 != img2);

    ASSERT_EQ(123, img2.Data()[bytePos]);
    auto pix2 = img2.At(2, 3);
    pix2[1] = 0;
    ASSERT_EQ(0, img2.Data()[bytePos]);
    ASSERT_TRUE(img1 == img2);
    ASSERT_FALSE(img1 != img2);
}

}//unnamed namespace