#ifndef VDOWNLOADER_VD_DECODER_H_
#define VDOWNLOADER_VD_DECODER_H_

#include "Decoder.h"
#include "Ap4ByteStream.h"
#include "Ap4Helpers.h"
#include "LibavH264Decoder.h"
#include "Mp4Utils.h"
#include "Sources.h"
#include "Track.h"
#include "Utils.h"

#include <functional>
#include <optional>
#include <set>
#include <span>

namespace vd
{

struct Frame final
{
    ArgbImage image;
    std::chrono::nanoseconds timestamp;

    //No validation is performed
    static Frame Create(const ArgbImage &img, std::chrono::nanoseconds timestamp);
};



template <class T>
concept DecodedImageConcept =
    requires(const T cv)
{
    { cv.Image() } -> std::same_as<ArgbImage>;
};

template <class T>
concept H264DecoderConcept =
    requires(T v, std::span<const std::byte> nalUnit)
{
    DecodedImageConcept<typename T::DecodedImage>;
    { []<DecodedImageConcept T>(std::optional<T> ret) {} (v.Decode(nalUnit)) };
    { []<DecodedImageConcept T>(std::optional<T> ret) {} (v.Retrieve()) };
};



//There's an assumption that decoder returns frames in presentation order and it's coordinated with "trun" atom table,
//and also doesn't return non-displayable frames that have timestamps out of bounds, otherwise behavior may be weird
template <H264DecoderConcept DecoderImplT>
class DecoderBase final
{
public:
    DecoderBase(DecodingConfig config, Segment segment)
        requires std::is_default_constructible_v<DecoderImplT>;

    DecoderBase(DecodingConfig config, Segment segment, DecoderImplT decoderImpl)
        requires std::is_move_constructible_v<DecoderImplT> || std::is_copy_constructible_v<DecoderImplT>;

    //Returns timestamp of the next frame
    //Ideally, number of frames produced by decoder must match number of frames in mp4 description of segment.
    //But it can't be guaranteed, so timestamps and frames stream are kind of independent.
    //In ideal case once GetNext returns nothing, TimestampNext will throw from that moment, but
    //it's not impossible that TimestampNext may start throwing earlier or return value when frames stream is ended
    std::chrono::nanoseconds TimestampNext() const;
    bool HasMore() const noexcept;

    //Those functions are written as exception safe as possible, but it's impossible
    //to guarantee that they will work correctly after exception due to external dependencies, so it's safer
    //to throw away whole object when exception occurs, although skipping problematic frame may work
    std::optional<Frame> GetNext();
    void SkipNext();
    //Sets up decoder that subsequent call to GetNext() will return frame to be displayed at "to" moment.
    //Doesn't perform range check, so it's possible to request skipping to moment far beyond end,
    //in that case last frame will be returned on subsequent call to GetNext(), although it's illegal
    //to request skipping to already passed moment, so exception will be thrown in that case
    void SkipTo(std::chrono::nanoseconds to);
    
private:
    DecoderImplT mDecoder;
    DecodingConfig mConfig;
    Segment mSegment;
    std::size_t mPos;
    std::optional<typename DecoderImplT::DecodedImage> mPendingBuffer;
    std::vector<std::chrono::nanoseconds> mTimestamps;
    std::size_t mNumFramesProcessed{0};

    std::set<std::chrono::nanoseconds>
        ParseTimestamps(AP4_TrunAtom &trunAtom, AP4_TfhdAtom &tfhdAtom);
    std::vector<std::chrono::nanoseconds>
        CalculateTimestamps(AP4_TrunAtom &trunAtom, AP4_TfhdAtom &tfhdAtom);
    std::chrono::nanoseconds TimestampNext(std::size_t lookahead) const;
    std::size_t NumFramesLeft() const;
    //Sets mPendingBuffer (possibly to empty state if no more frames available) or throws.
    void PrepareNext();
    std::optional<Frame> ReleaseBuffer();
    void DiscardBuffer();
};

using DecoderLibav = DecoderBase<LibavH264Decoder>;

namespace internal
{

std::size_t AssertDataOffset(const Segment &segment, AP4_TrunAtom &trunAtom);
std::span<const std::byte> ReadNalUnit(std::span<const std::byte> data, size_t sizeLen);

template <H264DecoderConcept DecoderT>
void ProcessParameterSetUnits(DecoderT &decoder, const std::vector<std::vector<std::byte>> &units)
{
    for(const auto &unit : units)
    {
        auto res = decoder.Decode(unit);
        if(res)
        {
            throw Error{"parameter set unit decoding returned value unexpectedly"};
        }
    }
}

}//namespace internal

template <H264DecoderConcept DecoderImplT>
DecoderBase<DecoderImplT>::DecoderBase(DecodingConfig config, Segment segment)
    requires std::is_default_constructible_v<DecoderImplT>
    : DecoderBase{std::move(config), std::move(segment), DecoderImplT{}}
{

}

template <H264DecoderConcept DecoderImplT>
DecoderBase<DecoderImplT>::DecoderBase(DecodingConfig config, Segment segment, DecoderImplT decoderImpl)
    requires std::is_move_constructible_v<DecoderImplT> || std::is_copy_constructible_v<DecoderImplT>
    : mDecoder{std::move(decoderImpl)},
      mConfig{std::move(config)},
      mSegment{std::move(segment)}
{
    using namespace internal;
    using namespace std::chrono_literals;

    ProcessParameterSetUnits(mDecoder, mConfig.spsNalUnits);
    ProcessParameterSetUnits(mDecoder, mConfig.ppsNalUnits);

    auto stream = std::make_shared<Ap4ByteStream<MemoryViewSource>>(MemoryViewSource{mSegment.data});

    auto moofAtom = GetNextAtom<AP4_ContainerAtom>(stream, gAtomTypeMoof);

    auto tfhdAtom = dynamic_cast<AP4_TfhdAtom *>(moofAtom->FindChild("traf/tfhd"));
    if(tfhdAtom == nullptr)
    {
        throw NotFoundError{R"("tfhd" atom is not found)"};
    }
    auto trunAtom = dynamic_cast<AP4_TrunAtom *>(moofAtom->FindChild("traf/trun"));
    if(trunAtom == nullptr)
    {
        throw NotFoundError{R"("trun" atom is not found)"};
    }
    
    mTimestamps = CalculateTimestamps(*trunAtom, *tfhdAtom);
    if(mTimestamps.empty())
    {
        throw Error{"segment doesn't contain frames"};
    }

    mPos = AssertDataOffset(mSegment, *trunAtom);
}

template <H264DecoderConcept DecoderImplT>
std::optional<Frame> DecoderBase<DecoderImplT>::GetNext()
{
    if(mPendingBuffer)
    {
        return ReleaseBuffer();
    }

    PrepareNext();

    if(mPendingBuffer)
    {
        return ReleaseBuffer();
    }

    return {};
}

template <H264DecoderConcept DecoderImplT>
void DecoderBase<DecoderImplT>::SkipNext()
{
    if(mPendingBuffer)
    {
        DiscardBuffer();
        return;
    }

    PrepareNext();

    if(mPendingBuffer)
    {
        DiscardBuffer();
    }
}

template <H264DecoderConcept DecoderImplT>
void DecoderBase<DecoderImplT>::SkipTo(std::chrono::nanoseconds to)
{
    if(!HasMore())
    {
        throw RangeError{"no more frames"};
    }

    if(to < TimestampNext())
    {
        throw RangeError{std::format("skipping to moment in the past ({}ns) requested", to.count())};
    }

    while(NumFramesLeft() > 1 && to >= TimestampNext(1))
    {
        SkipNext();
    }
}

template <H264DecoderConcept DecoderImplT>
std::chrono::nanoseconds DecoderBase<DecoderImplT>::TimestampNext() const
{
    return TimestampNext(0);
}

template <H264DecoderConcept DecoderImplT>
bool DecoderBase<DecoderImplT>::HasMore() const noexcept
{
    //This leads to exception if there is mismatch between container and data stream,
    //so we need to check another way
    //return NumFramesLeft() > 0;

    return mNumFramesProcessed < mTimestamps.size();
}

template <H264DecoderConcept DecoderImplT>
std::set<std::chrono::nanoseconds>
    DecoderBase<DecoderImplT>::ParseTimestamps(AP4_TrunAtom &trunAtom, AP4_TfhdAtom &tfhdAtom)
{
    using namespace internal;

    auto set = std::set<std::chrono::nanoseconds>{};

    auto defaultStart = std::chrono::nanoseconds{0};
    auto defaultDuration = DurationNano(tfhdAtom.GetDefaultSampleDuration(), mConfig.timescale);

    const auto &entries = trunAtom.UseEntries();
    for(const auto &entry : entries)
    {
        auto frameStart = defaultStart + DurationNano(entry.sample_composition_time_offset,mConfig.timescale);

        //We must increment time even if we skip frame
        auto frameDuration{defaultDuration};
        if(entry.sample_duration != 0)
        {
            frameDuration = DurationNano(entry.sample_duration, mConfig.timescale);
        }
        //Actually all that duration/offset/skippable frame/defaults mess is not obvious at all, so probably this calculation
        //will be incorrect for some files, but there is no point in trying to impliment all fantasies of mp4 format creators
        defaultStart += frameDuration;

        if(frameStart < std::chrono::nanoseconds{0} || frameStart >= mSegment.duration)
        {
            //Looks like there may be frames with composition time out of track(?) range (according to standard (8.6)),
            //it means they're not supposed to be displayed, so we just skip anything out of range of segment for simplicity.
            //And also we hope that decoder won't return those frames.
            continue;
        }
        if(set.contains(frameStart))
        {
            throw Error{std::format("found multiple frames with equal offset({}ns)", frameStart.count())};
        }

        set.insert(frameStart);
    }

    return set;
}

template <H264DecoderConcept DecoderImplT>
std::vector<std::chrono::nanoseconds>
    DecoderBase<DecoderImplT>::CalculateTimestamps(AP4_TrunAtom &trunAtom, AP4_TfhdAtom &tfhdAtom)
{
    auto set = ParseTimestamps(trunAtom, tfhdAtom);

    std::vector<std::chrono::nanoseconds> ret;
    ret.reserve(set.size());

    if(!set.empty())
    {
        //For some reason (edit: maybe to keep them all positive?) it's not required for offsets to start from zero,
        //so we fix it
        auto offsetMin = *set.begin();
        for(auto &ts : set)
        {
            ret.push_back(ts - offsetMin);
        }
    }

    return ret;
}

template <H264DecoderConcept DecoderImplT>
std::chrono::nanoseconds DecoderBase<DecoderImplT>::TimestampNext(std::size_t lookahead) const
{
    if(mTimestamps.size() == 0)
    {
        throw Error{"no frames found"};
    }

    auto ind = Add(mNumFramesProcessed, lookahead);    
    if(ind >= mTimestamps.size())
    {
        throw RangeError{"no more frames"};
    }

    return mTimestamps[ind] + mSegment.offset;
}
 
template <H264DecoderConcept DecoderImplT>
std::size_t DecoderBase<DecoderImplT>::NumFramesLeft() const
{
    return Sub(mTimestamps.size(), mNumFramesProcessed);
}

template <H264DecoderConcept DecoderImplT>
void DecoderBase<DecoderImplT>::PrepareNext()
{
    static_assert(std::is_nothrow_move_assignable_v<std::span<const std::byte>>);

    using namespace internal;

    //This shall never happen actually, but it won't hurt to check
    if(mPendingBuffer)
    {
        throw Error{"mPendingBuffer is not empty"};
    }

    while(mPos < mSegment.data.size())
    {
        auto remainder = std::span(std::next(mSegment.data.cbegin(), mPos), mSegment.data.cend());
        auto unit = ReadNalUnit(remainder, mConfig.unitLengthSize);
        mPendingBuffer = mDecoder.Decode(unit);
        mPos += (unit.size_bytes() + mConfig.unitLengthSize);

        if(mPendingBuffer)
        {
            return;
        }
    }

    mPendingBuffer = mDecoder.Retrieve();
}

namespace internal
{

namespace testing
{

extern thread_local int gDecoderBase_ReleaseBuffer_ThrowValue;

}//namespace testing

}//namespace internal

template <H264DecoderConcept DecoderImplT>
std::optional<Frame> DecoderBase<DecoderImplT>::ReleaseBuffer()
{
    static_assert(std::is_nothrow_move_assignable_v<Frame> &&
                  std::is_nothrow_move_constructible_v<Frame>);

    Frame ret = Frame::Create(mPendingBuffer.value().Image(), TimestampNext());

    //This is arguably not best approach, but to test exception safety we either need to introduce ugly mockable object,
    //or hide something like this to throw conditionally
    if(internal::testing::gDecoderBase_ReleaseBuffer_ThrowValue != 0)
    {
        throw internal::testing::gDecoderBase_ReleaseBuffer_ThrowValue;
    }

    mNumFramesProcessed = Add(mNumFramesProcessed, 1u);
    mPendingBuffer.reset();
    return ret;
}

template <H264DecoderConcept DecoderImplT>
void DecoderBase<DecoderImplT>::DiscardBuffer()
{
    mNumFramesProcessed = Add(mNumFramesProcessed, 1u);
    mPendingBuffer.reset();   
}



template <class T>
concept DecoderConcept =
    requires(T v, const T vconst, std::chrono::nanoseconds ts)
{
    { vconst.TimestampNext() } -> std::same_as<std::chrono::nanoseconds>;
    { vconst.HasMore() } noexcept -> std::same_as<bool>;
    { v.GetNext() } -> std::same_as<std::optional<Frame>>;
    { v.SkipNext() } -> std::same_as<void>;
    { v.SkipTo(ts) } -> std::same_as<void>;
};

//Wrapper for easy decoding sequential segments
template <DecoderConcept DecoderImplT>
class SerialDecoderBase final
{
public:
    SerialDecoderBase(std::vector<DecoderImplT> decoders);

    std::chrono::nanoseconds TimestampNext() const;
    bool HasMore() const noexcept;
    std::optional<Frame> GetNext();
    void SkipNext();
    void SkipTo(std::chrono::nanoseconds to);

private:
    std::vector<DecoderImplT> mDecoders;
    mutable std::size_t mCurrentIndex;

    void EnsureThereIsMoreOrEnd() const;
};

using SerialDecoderLibav = SerialDecoderBase<DecoderLibav>;

template <DecoderConcept DecoderImplT>
SerialDecoderBase<DecoderImplT>::SerialDecoderBase(std::vector<DecoderImplT> decoders)
    : mDecoders{std::move(decoders)},
      mCurrentIndex{0}
{
    if(mDecoders.empty())
    {
        throw ArgumentError{"at least one decoder is required"};
    }
}

template <DecoderConcept DecoderImplT>
std::optional<Frame> SerialDecoderBase<DecoderImplT>::GetNext()
{
    EnsureThereIsMoreOrEnd();

    if(mCurrentIndex == mDecoders.size())
    {
        return {};
    }

    return mDecoders[mCurrentIndex].GetNext();
}

template <DecoderConcept DecoderImplT>
void SerialDecoderBase<DecoderImplT>::SkipNext()
{
    EnsureThereIsMoreOrEnd();

    if(mCurrentIndex == mDecoders.size())
    {
        return;
    }

    return mDecoders[mCurrentIndex].SkipNext();
}

template <DecoderConcept DecoderImplT>
void SerialDecoderBase<DecoderImplT>::SkipTo(std::chrono::nanoseconds to)
{
    //really special case, handling it separately makes things much easier
    if(mCurrentIndex == mDecoders.size())
    {
        throw RangeError{"no more frames"};
    }

    auto index = mDecoders.size() - 1;
    while(mDecoders[index].HasMore() && index != mCurrentIndex && mDecoders[index].TimestampNext() > to)
    {
        index -= 1;
    }

    mCurrentIndex = index;
    mDecoders[mCurrentIndex].SkipTo(to);
}

template <DecoderConcept DecoderImplT>
std::chrono::nanoseconds SerialDecoderBase<DecoderImplT>::TimestampNext() const
{
    EnsureThereIsMoreOrEnd();

    if(mCurrentIndex == mDecoders.size())
    {
        throw RangeError{"no more frames"};
    }

    return mDecoders[mCurrentIndex].TimestampNext();
}

template <DecoderConcept DecoderImplT>
bool SerialDecoderBase<DecoderImplT>::HasMore() const noexcept
{
    EnsureThereIsMoreOrEnd();

    return mCurrentIndex != mDecoders.size();
}

template <DecoderConcept DecoderImplT>
void SerialDecoderBase<DecoderImplT>::EnsureThereIsMoreOrEnd() const
{
    while(mCurrentIndex != mDecoders.size() && !mDecoders[mCurrentIndex].HasMore())
    {
        mCurrentIndex += 1;
    }
}

} //namespace vd

#endif //VDOWNLOADER_VD_DECODER_H_