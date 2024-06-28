#include "libav/errors.h"
#include "libav/libav.h"

#include "gtest/gtest.h"

#include <chrono>

const char *NON_EXISTENT_FILE = "no.file";
const char *DEFAULT_TEST_FILE = "sample__240__libx265__aac__30s__video.mkv";

TEST(FormatContext, CreatedNotInitialized)
{
    auto ctx = libav::FormatContext{};

    ASSERT_FALSE(ctx.IsInitialized());
}

TEST(FormatContext, InitializationRelatedThrows)
{
    auto ctx = libav::FormatContext{};

    ASSERT_THROW(ctx.UnsafeGetPtr(), libav::NotInitializedError);
    ASSERT_THROW((const_cast<const libav::FormatContext&>(ctx).UnsafeGetPtr()), libav::NotInitializedError);

    ASSERT_NO_THROW(ctx.IsInitialized());
    ASSERT_NO_THROW(ctx.Close());
    ASSERT_NO_THROW(ctx.Open(DEFAULT_TEST_FILE));
}

TEST(FormatContext, FailedOpening)
{
    auto ctx = libav::Open(DEFAULT_TEST_FILE);
    
    ASSERT_ANY_THROW(libav::Open(NON_EXISTENT_FILE));
    ASSERT_ANY_THROW(ctx.Open(NON_EXISTENT_FILE));
    ASSERT_TRUE(ctx.IsInitialized());
}

TEST(FormatContext, InitializedWhenOpened)
{
    auto ctx = libav::FormatContext{};
    ctx.Open(DEFAULT_TEST_FILE);

    ASSERT_TRUE(ctx.IsInitialized());

    ctx = libav::Open(DEFAULT_TEST_FILE);

    ASSERT_TRUE(ctx.IsInitialized());
}

TEST(FormatContext, NotInitializedAfterMove)
{
    auto ctx = libav::Open(DEFAULT_TEST_FILE);

    libav::FormatContext movedTo = std::move(ctx);

    ASSERT_FALSE(ctx.IsInitialized());
    ASSERT_TRUE(movedTo.IsInitialized());

    ctx = std::move(movedTo);

    ASSERT_TRUE(ctx.IsInitialized());
    ASSERT_FALSE(movedTo.IsInitialized());
}

TEST(FormatContext, EqualityComparison)
{
    auto ctx1 = libav::FormatContext{};
    auto ctx2 = libav::FormatContext{};

    ASSERT_NE(ctx1, ctx2);
    ASSERT_EQ(ctx1, ctx1);

    ctx1.Open(DEFAULT_TEST_FILE);
    ctx2.Open(DEFAULT_TEST_FILE);

    ASSERT_NE(ctx1, ctx2);
}

TEST(FormatContext, NotInitializedAfterClosing)
{
    auto ctx = libav::Open(DEFAULT_TEST_FILE);

    ctx.Close();

    ASSERT_FALSE(ctx.IsInitialized());
}

TEST(FormatContext, ReadingAttributes)
{
    using namespace std::chrono_literals;

    auto ctx = libav::Open(DEFAULT_TEST_FILE);

    ASSERT_EQ("matroska,webm",ctx.GetName());
    ASSERT_EQ("Matroska / WebM", ctx.GetLongName());
    ASSERT_EQ(30021ms, ctx.GetDuration());
}