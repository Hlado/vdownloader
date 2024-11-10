#include "Track.h"
#include "Ap4Helpers.h"
#include "Errors.h"
#include "Utils.h"

#include <format>

namespace vd
{

namespace
{

std::vector<std::byte> CopyParams(const AP4_DataBuffer &buf)
{
    auto data = reinterpret_cast<const std::byte *>(buf.GetData());
    return std::vector<std::byte>(data, data + buf.GetDataSize());
}

} //unnamed namespace



DecodingConfig::DecodingConfig(AP4_AvccAtom &avccAtom, std::uint32_t timescale)
    : unitLengthSize{avccAtom.GetNaluLengthSize()},
      timescale(timescale)
{
    for(auto &param : avccAtom.GetSequenceParameters())
    {
        spsNalUnits.push_back(CopyParams(param));
    }
    for(auto &param : avccAtom.GetPictureParameters())
    {
        ppsNalUnits.push_back(CopyParams(param));
    }
}



std::chrono::nanoseconds Segment::Finish() const
{
    return offset + duration;
}



Track::Track(std::shared_ptr<AP4_ByteStream> containerData,
             AP4_TrakAtom &trakAtom,
             AP4_SidxAtom &sidxAtom,
             std::size_t anchorPoint)
    : mContainerData(containerData),
      mAnchorPoint(anchorPoint)
{
    if(trakAtom.UseTkhdAtom() == nullptr)
    {
        throw NotFoundError{R"("tkhd" atom is not found)"};
    }
    mId = trakAtom.GetId();

    auto avccAtom = dynamic_cast<AP4_AvccAtom *>(trakAtom.FindChild("mdia/minf/stbl/stsd/avc1/avcC"));
    if(avccAtom == nullptr)
    {
        throw NotFoundError{R"("avcC" atom is not found)"};
    }
    if(avccAtom->GetProfile() != 77)
    {
        throw NotSupportedError{std::format(R"("avcC" profile ({}) is not supported)", avccAtom->GetProfile())};
    }

    if(sidxAtom.GetReferenceId() != mId)
    {
        throw Error{std::format(R"("sidx" atom ({}) and track id ({}) mismatch)",
                                sidxAtom.GetReferenceId(),
                                mId)};
    }
    if(sidxAtom.GetReferences().ItemCount() == 0)
    {
        throw Error{R"("sidx" atom doesn't contain segments)"};
    }
    auto timescale = sidxAtom.GetTimeScale();
    if(timescale == 0)
    {
        throw ArgumentError{std::format(R"(timescale of track ({}) is zero)", mId)};
    }

    mDecodingConfig = DecodingConfig(*avccAtom, timescale);
    LoadDescriptors(sidxAtom);
}

std::uint32_t Track::GetId() const noexcept
{
    return mId;
}

const DecodingConfig &Track::GetDecodingConfig() const noexcept
{
    return mDecodingConfig;
}

bool Track::HasSegments() const noexcept
{
    return !mOrderedDescriptors.empty();
}

std::chrono::nanoseconds Track::GetStart() const
{
    AssertHasSegments();
    return mOrderedDescriptors.front().timeOffset;
}

std::chrono::nanoseconds Track::GetFinish() const
{
    AssertHasSegments();
    return mOrderedDescriptors.back().TimeEnd();
}

void Track::LoadDescriptors(AP4_SidxAtom &sidxAtom)
{
    std::uint32_t dataOffsetAcc{0};
    std::chrono::nanoseconds timeOffsetAcc{0};
    for(auto &ref : sidxAtom.GetReferences())
    {
        if(ref.m_ReferenceType == 1)
        {
            throw NotSupportedError(R"("sidx" atom format is not supported)");
        }
        if(ref.m_ReferencedSize == 0 || ref.m_SubsegmentDuration == 0)
        {
            throw ArgumentError("segment has zero size or duration");
        }

        auto duration = std::chrono::nanoseconds{
            ref.m_SubsegmentDuration * std::nano::den / mDecodingConfig.timescale};

        auto desc =
            SegmentDescriptor{.timeOffset = timeOffsetAcc,
                              .duration = duration,
                              .dataOffset = dataOffsetAcc,
                              .size = ref.m_ReferencedSize};

        mOrderedDescriptors.push_back(desc);

        dataOffsetAcc += desc.size;
        timeOffsetAcc += desc.duration;
    }
}

void Track::AssertHasSegments() const
{
    if(mOrderedDescriptors.empty())
    {
        throw Error(std::format("Track ({}) has no segments", mId));
    }
}

} //namespace vd