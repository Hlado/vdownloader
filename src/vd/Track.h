#ifndef VDOWNLOADER_VD_TRACK_H_
#define VDOWNLOADER_VD_TRACK_H_

#include "Ap4Headers.h"
#include "Errors.h"
#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace vd
{

struct DecodingConfig final
{
    using UnitType = std::vector<std::byte>;
    using UnitsType = std::vector<UnitType>;

    DecodingConfig() = default;
    DecodingConfig(AP4_AvccAtom &avccAtom, std::uint32_t timescale);

    UnitsType spsNalUnits;
    UnitsType ppsNalUnits;
    std::size_t unitLengthSize{4};
    std::uint32_t timescale{0};
};

struct Segment final
{
    std::chrono::nanoseconds offset;
    std::chrono::nanoseconds duration;
    std::vector<std::byte> data;

    std::chrono::nanoseconds Finish() const;
};



class Track final
{
public:
    Track(std::shared_ptr<AP4_ByteStream> containerData,
          AP4_TrakAtom &trakAtom,
          AP4_SidxAtom &sidxAtom,
          std::size_t anchorPoint);

    std::uint32_t GetId() const noexcept;
    const DecodingConfig  &GetDecodingConfig() const noexcept;
    bool HasSegments() const noexcept;
    //Throws if no segments are available
    std::chrono::nanoseconds GetStart() const;
    std::chrono::nanoseconds GetFinish() const;

    //Returns all segments for which [seg.timeOffset, seg.timeOffset + seg.duration)
    //and [from, to) has intersection. Throws if to <= from or to/from is out of bounds
    template <typename RepT, typename PeriodT>
    std::vector<Segment> Slice(std::chrono::duration<RepT, PeriodT> from,
                               std::chrono::duration<RepT, PeriodT> to) const;

private:
    struct SegmentDescriptor final
    {
        std::chrono::nanoseconds timeOffset;
        std::chrono::nanoseconds duration;
        std::uint32_t dataOffset;
        std::uint32_t size;

        std::chrono::nanoseconds TimeEnd() const noexcept
        {
            return timeOffset + duration;
        }
    };

    std::shared_ptr<AP4_ByteStream> mContainerData;
    std::vector<SegmentDescriptor> mOrderedDescriptors;
    DecodingConfig mDecodingConfig;
    std::size_t mAnchorPoint;
    std::uint32_t mId;

    void LoadDescriptors(AP4_SidxAtom &sidxAtom);
    void AssertHasSegments() const;
};

template <typename RepT, typename PeriodT>
std::vector<Segment> Track::Slice(std::chrono::duration<RepT, PeriodT> from,
                                  std::chrono::duration<RepT, PeriodT> to) const
{
    if(from >= to || from < GetStart() || to > GetFinish())
    {
        throw RangeError{};
    }

    auto first =
        std::ranges::upper_bound(
            mOrderedDescriptors, from, std::ranges::less{}, [](const auto &item) { return item.TimeEnd(); });
    auto last = 
        std::ranges::lower_bound(
            mOrderedDescriptors, to, std::ranges::less{}, &SegmentDescriptor::timeOffset);

    std::vector<Segment> ret;
    for(; first != last; ++first)
    {
        const auto &desc = *first;
        auto seg = Segment{.offset = desc.timeOffset,
                           .duration = desc.duration,
                           .data = std::vector<std::byte>(desc.size)};

        if(auto err = mContainerData->Seek(mAnchorPoint + desc.dataOffset); AP4_FAILED(err))
        {
            throw LibraryCallError{"AP4_ByteStream::Seek", err};
        }
        
        auto err = mContainerData->Read(seg.data.data(), UintCast<AP4_Size>(seg.data.size()));
        if(AP4_FAILED(err))
        {
            throw LibraryCallError{"AP4_ByteStream::Read", err};
        }
        ret.push_back(std::move(seg));
    }

    return ret;
}

} //namespace vd

#endif //VDOWNLOADER_VD_TRACK_H_