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

    ASSERT_EQ(18446744073709551615u, StrToUint<std::uint64_t>("18446744073709551615"));
    ASSERT_ANY_THROW(StrToUint<std::uint64_t>("18446744073709551616"));
}

TEST(UtilsTests, UintCast)
{
    ASSERT_EQ(10,UintCast<std::size_t>((std::uint8_t)10));
    ASSERT_EQ(255, UintCast<std::uint8_t>((std::size_t)255));
    ASSERT_THROW(UintCast<std::uint8_t>((std::size_t)256), RangeError);
}

TEST(UtilsTests, UintOverflow)
{
    ASSERT_FALSE(UintOverflow<std::uint8_t>(0, 255));
    ASSERT_TRUE(UintOverflow<std::uint8_t>(1, 255));
    ASSERT_FALSE(UintOverflow<std::uint8_t>(255, 0));
    ASSERT_TRUE(UintOverflow<std::uint8_t>(255, 1));
}

TEST(UtilsTests, PrintfTo)
{
    std::ostringstream ss;
    PrintfTo(ss, "{1} test {0}", 1, 2);
    ASSERT_EQ("2 test 1", ss.str());
}

TEST(UtilsTests, MakeArray)
{
    ASSERT_EQ(0, (MakeArray<0>(0).size()));

    auto arr = MakeArray<5>(123);

    for(auto v : arr)
    {
        ASSERT_EQ(123, v);
    }
}

TEST(UtilsTests, Defer)
{
    auto x = 12345;

    {
        Defer d(
            [&x]()
            {
                x = 54321;
                throw Error{};
            });

        ASSERT_EQ(12345, x);
    }

    ASSERT_EQ(54321, x);
}

TEST(UtilsTests, ByteSwap)
{
    std::int32_t val = 0xAABBCCDD;

    ASSERT_EQ(0xDDCCBBAA, ByteSwap(val));
    ByteSwapInplace(val);
    ASSERT_EQ(0xDDCCBBAA, val);
}



struct B
{
    bool &mFlag;

    B(bool &flag)
        : mFlag(flag)
    {
    
    }

    virtual ~B()
    {
        mFlag = !mFlag;
    }
};

struct D : public B
{
    bool &mFlag;

    D(bool &flagBase, bool &flag)
        : B(flagBase),
          mFlag(flag)
    {
    
    }

    ~D() override
    {
        mFlag = !mFlag;
    }
};

TEST(DynamicUniqueCast, EmptyBase)
{
    auto d = DynamicUniqueCast<D>(std::unique_ptr<B>{});
    ASSERT_EQ(nullptr,d);
}

TEST(DynamicUniqueCast, DerivedToDerived)
{
    bool flagBase = false;
    bool flagDerived = false;

    {
        auto b = std::make_unique<D>(flagBase, flagDerived);
        auto d = DynamicUniqueCast<D>(std::move(b));

        ASSERT_FALSE(flagBase);
        ASSERT_FALSE(flagDerived);
        ASSERT_EQ(nullptr, b);
        ASSERT_NE(nullptr, d);
    }

    ASSERT_TRUE(flagBase);
    ASSERT_TRUE(flagDerived);
}

TEST(DynamicUniqueCast, BaseToDerived)
{
    bool flagBase = false;

    {
        auto b = std::make_unique<B>(flagBase);
        auto d = DynamicUniqueCast<D>(std::move(b));

        ASSERT_FALSE(flagBase);
        ASSERT_EQ(nullptr, d);
        ASSERT_NE(nullptr, b);
    }

    ASSERT_TRUE(flagBase);
}