#include <vd/LibavUtils.h>

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using namespace vd;
using namespace vd::libav;
using namespace vd::literals;

namespace
{

const auto gData = std::vector<std::uint8_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
const auto gDataSpan = std::as_bytes(std::span<const std::uint8_t>(gData));



TEST(ReaderTests, EmptySource)
{
    auto reader = Reader{std::shared_ptr<SourceBase>{new Source{MemoryViewSource{}}}};

    ASSERT_EQ(0, Reader::Seek(&reader, std::numeric_limits<int>::max(), AVSEEK_SIZE | 1234));
    ASSERT_EQ(AVERROR_EOF, Reader::ReadPacket(&reader, nullptr, std::numeric_limits<int>::max()));
}

TEST(ReaderTests, SeekSet)
{
    auto reader = Reader{std::shared_ptr<SourceBase>{new Source{MemoryViewSource{gDataSpan}}}};

    ASSERT_LT(Reader::Seek(&reader, -1, SEEK_SET), 0);
    ASSERT_LT(Reader::Seek(&reader, 11, SEEK_SET), 0);

    std::uint8_t b{255};

    ASSERT_EQ(9, Reader::Seek(&reader, 9, SEEK_SET));
    ASSERT_EQ(1, Reader::ReadPacket(&reader, &b, 1));
    ASSERT_EQ(gData[9], b);

    ASSERT_EQ(0, Reader::Seek(&reader, 0, SEEK_SET));
    ASSERT_EQ(1, Reader::ReadPacket(&reader, &b, 1));
    ASSERT_EQ(gData[0], b);

    ASSERT_EQ(10, Reader::Seek(&reader, 10, SEEK_SET));
    ASSERT_EQ(AVERROR_EOF, Reader::ReadPacket(&reader, &b, 1));
}

TEST(ReaderTests, SeekCur)
{
    auto reader = Reader{std::shared_ptr<SourceBase>{new Source{MemoryViewSource{gDataSpan}}}};

    std::uint8_t b{255};

    ASSERT_EQ(5, Reader::Seek(&reader, 5, SEEK_CUR));
    ASSERT_EQ(1, Reader::ReadPacket(&reader, &b, 1));
    ASSERT_EQ(gData[5], b);

    ASSERT_EQ(6, Reader::Seek(&reader, 0, SEEK_CUR));
    ASSERT_EQ(3, Reader::Seek(&reader, -3, SEEK_CUR));
    ASSERT_EQ(1, Reader::ReadPacket(&reader, &b, 1));
    ASSERT_EQ(gData[3], b);

    ASSERT_EQ(4, Reader::Seek(&reader, 0, SEEK_CUR));
    ASSERT_EQ(10, Reader::Seek(&reader, 6, SEEK_CUR));
    ASSERT_EQ(AVERROR_EOF, Reader::ReadPacket(&reader, &b, 1));

    ASSERT_EQ(10, Reader::Seek(&reader, 0, SEEK_CUR));
    ASSERT_LT(Reader::Seek(&reader, -11, SEEK_CUR), 0);
    ASSERT_LT(Reader::Seek(&reader, 1, SEEK_CUR), 0);
}

TEST(ReaderTests, SeekEnd)
{
    auto reader = Reader{std::shared_ptr<SourceBase>{new Source{MemoryViewSource{gDataSpan}}}};

    std::uint8_t b{255};

    ASSERT_LT(Reader::Seek(&reader, -11, SEEK_END), 0);
    ASSERT_LT(Reader::Seek(&reader, 1, SEEK_END), 0);

    ASSERT_EQ(9, Reader::Seek(&reader, -1, SEEK_END));
    ASSERT_EQ(1, Reader::ReadPacket(&reader, &b, 1));
    ASSERT_EQ(gData[9], b);

    ASSERT_EQ(0, Reader::Seek(&reader, -10, SEEK_END));
    ASSERT_EQ(1, Reader::ReadPacket(&reader, &b, 1));
    ASSERT_EQ(gData[0], b);

    ASSERT_EQ(10, Reader::Seek(&reader, 0, SEEK_END));
    ASSERT_EQ(AVERROR_EOF, Reader::ReadPacket(&reader, &b, 1));
}

TEST(ReaderTests, SequentialRead)
{
    auto reader = Reader{std::shared_ptr<SourceBase>{new Source{MemoryViewSource{gDataSpan}}}};

    std::vector<std::uint8_t> vec(4, 0);

    ASSERT_EQ(4, Reader::ReadPacket(&reader, vec.data(), 4));
    auto slice = std::vector(gData.begin(), std::next(gData.begin(), 4));
    ASSERT_EQ(slice, vec);

    ASSERT_EQ(4, Reader::ReadPacket(&reader, vec.data(), 4));
    slice = std::vector(std::next(gData.begin(), 4), std::next(gData.begin(), 8));
    ASSERT_EQ(slice, vec);

    std::fill(vec.begin(), vec.end(), 0);
    ASSERT_EQ(2, Reader::ReadPacket(&reader, vec.data(), 4));
    ASSERT_EQ(gData[8], vec[0]);
    ASSERT_EQ(gData[9], vec[1]);
    ASSERT_EQ(0, vec[2]);
    ASSERT_EQ(0, vec[3]);

    ASSERT_EQ(AVERROR_EOF, Reader::ReadPacket(&reader, vec.data(), 4));
}

TEST(TimestampConversionTests, ToNanoValid)
{
    ASSERT_EQ(0s, ToNano(0, AVRational{.num = 10, .den = 100}, AV_ROUND_INF));
    
    ASSERT_EQ(2ns, ToNano(5, AVRational{.num = 1, .den = 2000000000}, AV_ROUND_ZERO));
    ASSERT_EQ(3ns, ToNano(5, AVRational{.num = 1, .den = 2000000000}, AV_ROUND_INF));

    ASSERT_EQ(5s, ToNano(1, AVRational{.num = 25, .den = 5}, AV_ROUND_INF));
}

TEST(TimestampConversionTests, ToNanoInvalid)
{
    ASSERT_THROW(
        ToNano(std::numeric_limits<std::int64_t>::max() / 2,
               AVRational{.num = 3, .den = 1},
               AV_ROUND_INF),
        RangeError);
}

TEST(TimestampConversionTests, FromNanoValid)
{
    ASSERT_EQ(0, FromNano(0ns, AVRational{.num = 10, .den = 100}, AV_ROUND_INF));
    
    ASSERT_EQ(4, FromNano(9ns, AVRational{.num = 4, .den = 2000000000}, AV_ROUND_ZERO));
    ASSERT_EQ(5, FromNano(9ns, AVRational{.num = 4, .den = 2000000000}, AV_ROUND_INF));

    ASSERT_EQ(2, FromNano(10s, AVRational{.num = 25, .den = 5}, AV_ROUND_INF));
}

TEST(TimestampConversionTests, FromNanoInvalid)
{
    auto val = static_cast<std::int64_t>((std::numeric_limits<std::int64_t>::max() * 0.7));
    ASSERT_THROW(
        FromNano(std::chrono::nanoseconds(val),
                 AVRational{.num = 1, .den = 2000000000},
                 AV_ROUND_INF),
        RangeError);
}

}//unnamed namespace