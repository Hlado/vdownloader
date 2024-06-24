#include "gtest/gtest.h"

#include <libav/errors.h>
#include "libav/libav.h"

const char *NON_EXISTENT_FILE = "no.file";
const char *DEFAULT_TEST_FILE = "sample__240__libx265__aac__30s__video.mkv";

TEST(FormatContext, CreatedNotInitialized)
{
    libav::FormatContext ctx;

    ASSERT_TRUE(!ctx.IsInitialized());
}

TEST(FormatContext, ThrowsIfNotInitialized)
{
    libav::FormatContext ctx;

    ASSERT_THROW(ctx.UnsafeGetPtr(), libav::NotInitializedError);
    ASSERT_THROW((const_cast<const libav::FormatContext&>(ctx).UnsafeGetPtr()), libav::NotInitializedError);
}

TEST(FormatContext, ThrowsWhenOpeningFails)
{
    libav::FormatContext ctx;
    
    ASSERT_ANY_THROW(ctx.Open(NON_EXISTENT_FILE));
    ASSERT_ANY_THROW(libav::Open(NON_EXISTENT_FILE));
}

TEST(FormatContext, InitializedWhenOpened)
{
    libav::FormatContext ctx;
    ctx.Open(DEFAULT_TEST_FILE);

    ASSERT_TRUE(ctx.IsInitialized());

    ctx = libav::Open(DEFAULT_TEST_FILE);

    ASSERT_TRUE(ctx.IsInitialized());
}

TEST(FormatContext, NotInitializedAfterMove)
{
    auto ctx = libav::Open(DEFAULT_TEST_FILE);

    //move construction
    libav::FormatContext movedTo = std::move(ctx);

    ASSERT_TRUE(!ctx.IsInitialized());
    ASSERT_TRUE(movedTo.IsInitialized());

    //move assignment
    ctx = std::move(movedTo);

    ASSERT_TRUE(ctx.IsInitialized());
    ASSERT_TRUE(!movedTo.IsInitialized());
}