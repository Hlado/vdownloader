#include <vd/Ap4ByteStream.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace vd;

const std::string gContent = "This is content for testing";



class MockSource
{
public:
    MOCK_METHOD(std::size_t, GetContentLength, (), (const));
    MOCK_METHOD(void, Read, (std::size_t pos, std::span<std::byte> buf));

    MockSource()
    {
        ON_CALL(*this, GetContentLength)
            .WillByDefault(Return(gContent.size()));

        ON_CALL(*this, Read)
            .WillByDefault(ReadDefault);
    }

    static void ReadDefault(std::size_t pos, std::span<std::byte> buf)
    {
        if(buf.size_bytes() < 1)
        {
            return;
        }

        vd::internal::AssertRangeCorrect(pos, buf, gContent.size());

        std::memcpy(buf.data(), (std::byte *)std::next(gContent.data(), pos), buf.size_bytes());
    }
};

class MockSourceWrapper
{
public:
    std::size_t GetContentLength() const
    {
        return impl->GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        impl->Read(pos, buf);
    }

    std::unique_ptr<MockSource> impl = std::make_unique<MockSource>();
};



class Ap4ByteStreamTestF : public testing::Test
{
protected:
    Ap4ByteStreamTestF()
        : mSrc(*mWrapper.impl),
          mStream(std::move(mWrapper))
    {
    
    }

    void SetUp() override
    {
        EXPECT_CALL(mSrc, GetContentLength)
            .Times(AnyNumber());
    }

//Some tricky initialization here, watch out
private:
    MockSourceWrapper mWrapper;

protected:
    MockSource &mSrc;
    Ap4ByteStream<MockSourceWrapper> mStream;
};

TEST_F(Ap4ByteStreamTestF, GetSizeReturnsCorrectSize)
{
    EXPECT_CALL(mSrc, GetContentLength)
        .Times(1);
    ASSERT_EQ(gContent.size(), mStream.GetSize());

    EXPECT_CALL(mSrc, GetContentLength)
        .Times(1);
    AP4_LargeSize size;
    auto ret = mStream.GetSize(size);
    ASSERT_EQ(AP4_SUCCESS, ret);
    ASSERT_EQ(gContent.size(), size);
}

TEST_F(Ap4ByteStreamTestF, NoexceptGetSizeFailsOnThrow)
{
    EXPECT_CALL(mSrc, GetContentLength)
        .WillOnce([]()->std::size_t { throw Error{}; });

    AP4_LargeSize size;
    ASSERT_TRUE(AP4_FAILED(mStream.GetSize(size)));
}

TEST_F(Ap4ByteStreamTestF, WritePartialIsNotSupported)
{
    AP4_Size numWritten;
    auto ret = mStream.WritePartial(nullptr, 0, numWritten);
    ASSERT_EQ(AP4_ERROR_NOT_SUPPORTED, ret);
}

TEST_F(Ap4ByteStreamTestF, SeekOutOfRangeFails)
{
    ASSERT_TRUE(AP4_FAILED(mStream.Seek(gContent.size())));
}

TEST_F(Ap4ByteStreamTestF, ReadFailsOnSourceException)
{
    EXPECT_CALL(mSrc, Read)
        .WillOnce([](auto, auto) { throw vd::Error{}; });

    auto arr = MakeArray<5>(std::byte{0});

    ASSERT_TRUE(AP4_FAILED(mStream.Read(arr.data(), (AP4_Size)arr.size())));
}

TEST_F(Ap4ByteStreamTestF, SequentialRead)
{
    EXPECT_CALL(mSrc, Read)
        .Times(2);

    auto buf = std::string(gContent.size(), '\0');
    auto half = buf.size() / 2;

    AP4_Size numRead;
    ASSERT_TRUE(AP4_SUCCEEDED(mStream.ReadPartial(buf.data(), (AP4_Size)half, numRead)));
    ASSERT_EQ(half, numRead);
    AP4_Position pos;
    mStream.Tell(pos);
    ASSERT_EQ(half, pos);

    auto err = mStream.ReadPartial(std::next(buf.data(), half), (AP4_Size)(buf.size() - half), numRead);
    ASSERT_TRUE(AP4_SUCCEEDED(err));
    ASSERT_EQ(buf.size() - half, numRead);
    ASSERT_EQ(gContent, buf);
    mStream.Tell(pos);
    ASSERT_EQ(buf.size(), pos);

    ASSERT_EQ(AP4_ERROR_EOS, mStream.ReadPartial(nullptr,std::numeric_limits<AP4_Size>::max(), numRead));
    ASSERT_EQ(0, numRead);
    mStream.Tell(pos);
    ASSERT_EQ(buf.size(), pos);
}

TEST_F(Ap4ByteStreamTestF, SeekAndRead)
{
    EXPECT_CALL(mSrc, Read)
        .Times(1);

    auto half = gContent.size() / 2;
    auto buf = std::string(half, '\0');
    
    ASSERT_TRUE(AP4_SUCCEEDED(mStream.Seek((AP4_Position)(gContent.size() - half))));
    AP4_Position pos;
    mStream.Tell(pos);
    ASSERT_EQ(gContent.size() - half, pos);
    AP4_Size numRead;
    ASSERT_TRUE(AP4_SUCCEEDED(mStream.ReadPartial(buf.data(), (AP4_Size)half, numRead)));
    ASSERT_EQ(half, numRead);
    mStream.Tell(pos);
    ASSERT_EQ(gContent.size(), pos);
    ASSERT_EQ(gContent.substr(gContent.size() - half), buf);

    ASSERT_EQ(AP4_ERROR_EOS, mStream.ReadPartial(nullptr, std::numeric_limits<AP4_Size>::max(), numRead));
    ASSERT_EQ(0, numRead);
    mStream.Tell(pos);
    ASSERT_EQ(gContent.size(), pos);
}