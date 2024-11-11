#include "TestContainer.h"

#include <vd/Mp4Container.h>
#include <vd/Utils.h>

#include <gtest/gtest.h>

using namespace vd;
using namespace std::chrono_literals;

namespace vd
{

static bool operator==(const Segment &l, const Segment &r) noexcept
{
    return l.offset == r.offset &&
        l.duration == r.duration &&
        l.data == r.data;
}

}//namespace vd



namespace
{

template <std::size_t Len>
std::vector<std::vector<std::byte>> ToNalUnits(const std::byte (&arr)[Len])
{
    return std::vector<std::vector<std::byte>>{std::vector<std::byte>(arr, arr + Len)};
}



TEST(TrackTests, Accessors)
{
    auto container = Mp4Container{Serialize(GetTestData())};
    auto &track = container.GetTrack();

    ASSERT_EQ(111, track.GetId());
    ASSERT_TRUE(track.HasSegments());
    ASSERT_EQ(0s, track.GetStart());
    ASSERT_EQ(60s, track.GetFinish());
    ASSERT_EQ(ToNalUnits(gSeqParams), track.GetDecodingConfig().spsNalUnits);
    ASSERT_EQ(ToNalUnits(gPicParams), track.GetDecodingConfig().ppsNalUnits);
    ASSERT_EQ(2, track.GetDecodingConfig().unitLengthSize);
    ASSERT_EQ(1, track.GetDecodingConfig().timescale);
}

//Can't test that, because constructing track without segments is not allowed for now
//
/*TEST(TrackTests, NoSegments)
{
    auto data = GetTestData();
    data.sidxAtom->SetReferenceCount(0);
    auto container = Mp4Container{Serialize(data)};
    auto &track = container.GetTrack();

    ASSERT_FALSE(track.HasSegments());
    ASSERT_THROW(track.GetStart(), Error);
    ASSERT_THROW(track.GetFinish(), Error);
}*/

TEST(TrackTests, SliceBadRangeThrows)
{
    auto container = Mp4Container{Serialize(GetTestData())};
    auto &track = container.GetTrack();

    ASSERT_THROW(track.Slice(-1s, 10s), RangeError);
    ASSERT_THROW(track.Slice(50s, 61s), RangeError);
    ASSERT_THROW(track.Slice(30s, 29s), RangeError);
    ASSERT_THROW(track.Slice(30s, 30s), RangeError);
}

TEST(TrackTests, SliceCorrectRanges)
{
    auto container = Mp4Container{Serialize(GetTestData())};
    auto &track = container.GetTrack();

    const auto seg1 = Segment{.offset = 0s,
                              .duration = 10s,
                              .data = std::vector<std::byte>(1, 0xAB_b)};
    const auto seg2 = Segment{.offset = 10s,
                              .duration = 20s,
                              .data = std::vector<std::byte>(2, 0xAB_b)};
    const auto seg3 = Segment{.offset = 30s,
                              .duration = 30s,
                              .data = std::vector<std::byte>(3, 0xAB_b)};

    ASSERT_EQ((std::vector<Segment>{seg1, seg2, seg3}), track.Slice(0s, 60s));
    ASSERT_EQ((std::vector<Segment>{seg1}), track.Slice(0s, 10s));
    ASSERT_EQ((std::vector<Segment>{seg2}), track.Slice(10s, 20s));
    ASSERT_EQ((std::vector<Segment>{seg3}), track.Slice(30s, 60s));
    ASSERT_EQ((std::vector<Segment>{seg1, seg2}), track.Slice(5s, 30s));
    ASSERT_EQ((std::vector<Segment>{seg2, seg3}), track.Slice(15s, 60s));
}

}//unnamed namespace