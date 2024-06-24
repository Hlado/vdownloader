#include "gtest/gtest.h"

#include "libav/libav.h"

TEST(FormatContext, CreatedNotInitialized)
{
    libav::AvFormatContext ctx;

    ASSERT_TRUE(!ctx.IsInitialized());
}