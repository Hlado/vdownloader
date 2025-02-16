#include <vd/Utils.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>

using namespace vd;

namespace
{

consteval std::uint8_t operator ""_u8(unsigned long long int val)
{
    return IntCast<std::uint8_t>(val);
}

consteval std::uint16_t operator ""_u16(unsigned long long int val)
{
    return IntCast<std::uint16_t>(val);
}

consteval std::uint32_t operator ""_u32(unsigned long long int val)
{
    return IntCast<std::uint32_t>(val);
}



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



template <typename T>
std::span<const std::byte> AsBytes(const T *val)
{
    return std::as_bytes(std::span<const T>(val, sizeof(T)));
}



TEST(UtilsTests, ReadBuffer)
{
    int i = 12345;
    double d = 123.45;

    ASSERT_EQ(i, ReadBuffer<int>(AsBytes(&i)));
    ASSERT_EQ(d, ReadBuffer<double>(AsBytes(&d)));
}

TEST(UtilsTests, UintOverflow)
{
    ASSERT_FALSE(UintOverflow<std::uint8_t>(0u));
    ASSERT_FALSE(UintOverflow<std::uint8_t>(255u));
    ASSERT_TRUE(UintOverflow<std::uint8_t>(256u));
    ASSERT_FALSE(UintOverflow<std::uint16_t>(255_u8));
}

TEST(UtilsTests, Add)
{
    ASSERT_NO_THROW(Add(255_u8, 0_u8));
    ASSERT_THROW(Add(255_u8, 1_u8), RangeError);

    ASSERT_NO_THROW(Add(65535_u16, 0_u8));
    ASSERT_THROW(Add(65535_u16, 1_u8), RangeError);

    ASSERT_NO_THROW(Add(65535_u16, 0_u32));
    ASSERT_THROW(Add(65535_u16, 1_u32), RangeError);
    ASSERT_THROW(Add(65535_u16, 65536_u32), RangeError);

    ASSERT_NO_THROW(Add(0xFFFFFFF0_u32, 1_u8, 0x0E_u16));
    ASSERT_THROW(Add(0xFFFFFFF1_u32, 0x0E_u16, 1_u8), RangeError);
}


TEST(UtilsTests, Sub)
{
    ASSERT_NO_THROW(Sub(1_u8, 1_u8));
    ASSERT_THROW(Sub(0_u8, 1_u8), RangeError);

    ASSERT_NO_THROW(Sub(1_u16, 1_u8));
    ASSERT_THROW(Sub(0_u16, 1_u8), RangeError);

    ASSERT_NO_THROW(Sub(65535_u16, 65535_u32));
    ASSERT_THROW(Sub(65535_u16, 65536_u32), RangeError);
    ASSERT_THROW(Sub(0_u16, 65536_u32), RangeError);

    ASSERT_NO_THROW(Sub(0xFFFF_u32, 0x0F_u8, 0xFFF0_u16));
    ASSERT_THROW(Sub(0xFFFF_u32, 0xFFF0_u16, 0x10_u8), RangeError);
}

TEST(UtilsTests, Mul)
{
    ASSERT_NO_THROW(Mul(51_u8, 5_u8));
    ASSERT_THROW(Mul(128_u8, 2_u8), RangeError);

    ASSERT_NO_THROW(Mul(0x101_u16, 0xFF_u8));
    ASSERT_THROW(Mul(0x102_u16, 0xFF_u8), RangeError);

    ASSERT_NO_THROW(Mul(0x101_u16, 0xFF_u32));
    ASSERT_THROW(Mul(0x102_u16, 0xFF_u32), RangeError);
    ASSERT_THROW(Mul(0x101_u16, 0xFFFF_u32), RangeError);

    ASSERT_NO_THROW(Mul(0x10001_u32, 0xFF_u8, 0x101_u16));
    ASSERT_THROW(Mul(0x10002_u32, 0x101_u16, 0xFF_u8), RangeError);
}

TEST(UtilsTests, UintCast)
{
    ASSERT_EQ(10,IntCast<std::uint64_t>((std::uint8_t)10));
    ASSERT_EQ(255, IntCast<std::uint8_t>((std::uint64_t)255));
    ASSERT_THROW(IntCast<std::uint8_t>((std::uint64_t)256), RangeError);

    ASSERT_EQ(10,IntCast<std::int64_t>((std::uint8_t)10));
    ASSERT_EQ(127, IntCast<std::int8_t>((std::uint64_t)127));
    ASSERT_THROW(IntCast<std::int8_t>((std::uint64_t)128), RangeError);
}

TEST(UtilsTests, SintCast)
{
    ASSERT_EQ(10,IntCast<std::uint64_t>((std::int8_t)10));
    ASSERT_EQ(255, IntCast<std::uint8_t>((std::int64_t)255));
    ASSERT_THROW(IntCast<std::uint8_t>((std::int64_t)256), RangeError);
    ASSERT_THROW(IntCast<std::uint64_t>((std::int8_t)-1), RangeError);

    ASSERT_EQ(10,IntCast<std::int64_t>((std::int8_t)10));
    ASSERT_EQ(-10,IntCast<std::int64_t>((std::int8_t)-10));
    ASSERT_EQ(127, IntCast<std::int8_t>((std::int64_t)127));
    ASSERT_EQ(-128, IntCast<std::int8_t>((std::int64_t)-128));
    ASSERT_THROW(IntCast<std::int8_t>((std::int64_t)128), RangeError);
    ASSERT_THROW(IntCast<std::int8_t>((std::int64_t)-129), RangeError);
}

TEST(UtilsTests, Format)
{
    ASSERT_EQ("2 test 1", Format("{1} test {0}", 1, 2));
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

TEST(UtilsTests, MakeVector)
{
    auto vec = MakeVector({3u, 2u, 1u});
    ASSERT_TRUE((std::is_same_v<std::vector<unsigned int>, decltype(vec)>));
    ASSERT_EQ(3, vec.size());
    ASSERT_EQ(3, vec[0]);
    ASSERT_EQ(2, vec[1]);
    ASSERT_EQ(1, vec[2]);
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

TEST(UtilsTests, EndianCast)
{
    int i = 0x01020304;
    auto be = EndianCastTo<std::endian::big>(i);
    auto le = EndianCastTo<std::endian::little>(i);

    ASSERT_NE(be, le);
    ASSERT_EQ(i, EndianCastFrom<std::endian::big>(be));
    ASSERT_EQ(i, EndianCastFrom<std::endian::little>(le));
    ASSERT_EQ(le, (EndianCast<std::endian::big, std::endian::little>(be)));
    ASSERT_EQ(be, (EndianCast<std::endian::little, std::endian::big>(le)));
    ASSERT_EQ(i, (EndianCast<std::endian::native, std::endian::native>(i)));
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

TEST(StdDurationConcept, Test)
{
    ASSERT_TRUE(StdDurationConcept<std::chrono::seconds>);
    ASSERT_FALSE(StdDurationConcept<std::string>);
}

TEST(DiscardingQueueTests, CapacityCheck)
{
    auto queue = DiscardingQueue<int>{};
    ASSERT_EQ(0, queue.Capacity());

    for(int i = 0; i < 1000; ++i)
    {
        queue.Push(i);
    }
    ASSERT_EQ(1000, queue.Size());

    queue = DiscardingQueue<int>{2};
    ASSERT_EQ(2, queue.Capacity());

    for(int i = 0; i < 10; ++i)
    {
        queue.Push(i);
    }
    ASSERT_EQ(2, queue.Size());
}

TEST(DiscardingQueueTests, EmptyAccess)
{
    auto queue = DiscardingQueue<int>{};

    ASSERT_EQ(0, queue.Size());
    ASSERT_TRUE(queue.Empty());
    ASSERT_NO_THROW(queue.Pop());
    ASSERT_THROW(queue.First(), NotFoundError);
}

TEST(DiscardingQueueTests, Clearing)
{
    auto queue = DiscardingQueue<int>{};

    for(int i = 0; i < 10; ++i)
    {
        queue.Push(i);
    }
    ASSERT_EQ(10, queue.Size());

    queue.Clear();
    ASSERT_EQ(0, queue.Size());
}

TEST(DiscardingQueueTests, NormalUsage)
{
    auto queue = DiscardingQueue<int>{2};

    queue.Push(0);
    ASSERT_FALSE(queue.Empty());
    ASSERT_EQ(1, queue.Size());
    ASSERT_EQ(0, queue.First());

    queue.Push(1);
    ASSERT_FALSE(queue.Empty());
    ASSERT_EQ(2, queue.Size());
    ASSERT_EQ(0, queue.First());

    queue.Push(2);
    ASSERT_FALSE(queue.Empty());
    ASSERT_EQ(2, queue.Size());
    ASSERT_EQ(1, queue.First());

    queue.Pop();
    ASSERT_FALSE(queue.Empty());
    ASSERT_EQ(1, queue.Size());
    ASSERT_EQ(2, queue.First());

    queue.Pop();
    ASSERT_TRUE(queue.Empty());
    ASSERT_EQ(0, queue.Size());
}

}//unnamed namespace