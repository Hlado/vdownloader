#include <vd/Decoder.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace vd;
using namespace vd::literals;
using namespace testing;
using namespace std::chrono_literals;

namespace
{

const size_t gNalUnitLengthSize = 4;
const AP4_UI32 gDefaultDuration = 5;
const std::byte gNalUnitData1[] = {0_b};
const std::byte gNalUnit1[] = {0_b, 0_b, 0_b, 1_b, 0_b};
const std::byte gNalUnitData2[] = {0_b, 1_b};
const std::byte gNalUnit2[] = {0_b, 0_b, 0_b, 2_b, 0_b, 1_b};
const std::byte gNalUnitData3[] = {0_b, 1_b, 2_b};
const std::byte gNalUnit3[] = {0_b, 0_b, 0_b, 3_b, 0_b, 1_b, 2_b};
const std::byte gNalUnitData4[] = {0_b, 1_b, 2_b, 3_b};
const std::byte gNalUnit4[] = {0_b, 0_b, 0_b, 4_b, 0_b, 1_b, 2_b, 3_b};

struct DecodedImage
{
    ArgbImage image;
    
    ArgbImage Image() const { return image; }
};

class MockH264Decoder
{
public:
    using DecodedImage = DecodedImage;
    
    MOCK_METHOD(std::optional<DecodedImage>, Retrieve, ());
    MOCK_METHOD(std::optional<DecodedImage>, Decode, (std::span<const std::byte> nalUnit));

    MockH264Decoder()
    {
        ON_CALL(*this, Retrieve)
            .WillByDefault(Return(std::optional<DecodedImage>{}));

        ON_CALL(*this, Decode)
            .WillByDefault(Return(std::optional<DecodedImage>{}));
    }
};

class MockH264DecoderWrapper
{
public:
    using DecodedImage = DecodedImage;

    std::optional<DecodedImage> Retrieve() const
    {
        return impl->Retrieve();
    }

    std::optional<DecodedImage> Decode(std::span<const std::byte> nalUnit)
    {
        return impl->Decode(nalUnit);
    }

    std::unique_ptr<MockH264Decoder> impl = std::make_unique<MockH264Decoder>();
};



struct TestData final
{
    std::unique_ptr<AP4_ContainerAtom> moofAtom;
    std::vector<std::byte> payload;
};

DecodingConfig DefaultConfig()
{
    DecodingConfig ret;
    ret.unitLengthSize = gNalUnitLengthSize;
    ret.timescale = 5;
    return ret;
}

std::unique_ptr<AP4_ContainerAtom> MakeMoofAtom(const std::vector<std::vector<std::byte>> &nalUnits, AP4_UI32 defaultDuration)
{
    auto trunAtom = std::make_unique<AP4_TrunAtom>(0x1 | 0x200 | 0x800,0,0);

    auto entries = AP4_Array<AP4_TrunAtom::Entry>{};
    auto entry = AP4_TrunAtom::Entry{};
    for(auto &unit : nalUnits)
    {
        entry.sample_size = IntCast<AP4_UI32>(unit.size());
        entries.Append(entry);
    }
    trunAtom->SetEntries(entries);

    auto &trunAtomRef = *trunAtom;
    
    auto trafAtom = std::make_unique<AP4_ContainerAtom>(AP4_ATOM_TYPE('t','r','a','f'));
    trafAtom->AddChild(new AP4_TfhdAtom{0x8,0,0,0,defaultDuration,0,0});
    trafAtom->AddChild(trunAtom.get());
    trunAtom.release();
    
    auto moofAtom = std::make_unique<AP4_ContainerAtom>(AP4_ATOM_TYPE_MOOF);
    moofAtom->AddChild(trafAtom.get());
    trafAtom.release();

    trunAtomRef.SetDataOffset(IntCast<AP4_SI32>(moofAtom->GetSize()));

    return moofAtom;
}

TestData MakeTestData(const std::vector<std::vector<std::byte>> &nalUnits, AP4_UI32 defaultDuration)
{
    std::vector<std::byte> payload;
    for(auto &unit : nalUnits)
    {
        payload.insert(payload.end(),std::cbegin(unit),std::cend(unit));
    }

    return TestData{.moofAtom = MakeMoofAtom(nalUnits, defaultDuration),
                    .payload = std::move(payload)};
}

std::unique_ptr<AP4_ContainerAtom> DefaultMoofAtom()
{
    return
        MakeMoofAtom(
            {MakeVector(gNalUnit1), MakeVector(gNalUnit2), MakeVector(gNalUnit3), MakeVector(gNalUnit4)},
            gDefaultDuration);
}

std::vector<std::byte> DefaultPayload()
{
    auto res = std::vector<std::byte>{};
    res.insert(res.end(),std::cbegin(gNalUnit1),std::cend(gNalUnit1));
    res.insert(res.end(),std::cbegin(gNalUnit2),std::cend(gNalUnit2));
    res.insert(res.end(),std::cbegin(gNalUnit3),std::cend(gNalUnit3));
    res.insert(res.end(),std::cbegin(gNalUnit4),std::cend(gNalUnit4));
    return res;
}

TestData DefaultTestData()
{
    return TestData{.moofAtom = DefaultMoofAtom(),
                    .payload = DefaultPayload()};
}

Segment MakeSegment(TestData &data, std::chrono::nanoseconds offset, std::chrono::nanoseconds duration)
{
    auto stream = std::make_shared<AP4_MemoryByteStream>();

    if(data.moofAtom != nullptr)
    {
        data.moofAtom->Write(*stream);
    }
    if(!data.payload.empty())
    {
        stream->Write(data.payload.data(), (AP4_Size)data.payload.size());
    }

    AP4_Position len;
    stream->Tell(len);
    stream->Seek(0);
    
    std::vector<std::byte> buf(len);
    stream->Read(buf.data(), (AP4_Size)len);

    return Segment{.offset = offset,
                   .duration = duration,
                   .data = std::move(buf)};
}

Segment MakeSegment(TestData &&data, std::chrono::nanoseconds offset, std::chrono::nanoseconds duration)
{
    return MakeSegment(data, offset, duration);
}

void RemoveChild(AP4_AtomParent &atom, const char *path)
{
    auto child = atom.FindChild(path);
    child->GetParent()->RemoveChild(child);
}



TEST(DecoderTests, ConstructionEmptySegment)
{
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),Segment{}}), NotFoundError);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),Segment{},MockH264DecoderWrapper{}}), NotFoundError);
}

TEST(DecoderTests, ConstructionCorrectData)
{
    auto data = DefaultTestData();
    auto segment = MakeSegment(data, 10s, 4s);
    ASSERT_NO_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}));
}

TEST(DecoderTests, ConstructionMissingAtoms)
{
    auto data = DefaultTestData();
    RemoveChild(*data.moofAtom,"traf/trun");
    auto segment = MakeSegment(data, 10s, 4s);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}), NotFoundError);

    data = DefaultTestData();
    RemoveChild(*data.moofAtom,"traf/tfhd");
    segment = MakeSegment(data, 10s, 4s);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}), NotFoundError);
}

TEST(DecoderTests, ConstructionEmptyTimestamps)
{
    auto data = DefaultTestData();
    dynamic_cast<AP4_TrunAtom *>(data.moofAtom->FindChild("traf/trun"))->SetEntries({});
    auto segment = MakeSegment(data, 10s, 4s);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}), Error);
}

TEST(DecoderTests, ConstructionDuplicatedTimestamps)
{
    auto data = DefaultTestData();
    auto trunAtom = dynamic_cast<AP4_TrunAtom *>(data.moofAtom->FindChild("traf/trun"));
    trunAtom->UseEntries()[0].sample_composition_time_offset += gDefaultDuration;
    auto segment = MakeSegment(data, 10s, 4s);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}), Error);
}

TEST(DecoderTests, ConstructionBadDataOffset)
{
    auto data = DefaultTestData();
    auto trunAtom = dynamic_cast<AP4_TrunAtom *>(data.moofAtom->FindChild("traf/trun"));

    trunAtom->SetDataOffset(0);
    auto segment = MakeSegment(data, 10s, 4s);
    ASSERT_NO_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}));
    trunAtom->SetDataOffset(-1);
    segment = MakeSegment(data, 10s, 4s);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}), Error);

    trunAtom->SetDataOffset((AP4_SI32)(data.moofAtom->GetSize32()+data.payload.size()-1));
    segment = MakeSegment(data, 10s, 4s);
    ASSERT_NO_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}));
    trunAtom->SetDataOffset((AP4_SI32)(data.moofAtom->GetSize32()+data.payload.size()));
    segment = MakeSegment(data, 10s, 4s);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}), Error);
}

TEST(DecoderTests, ConstructionParameterSetUnitsProcessing)
{
    auto unit = std::vector<std::byte>{{0xD_b,0xE_b,0xA_b,0xD_b,0xB_b,0xE_b,0xE_b,0xF_b}};
    auto segment = MakeSegment(DefaultTestData(), 10s, 4s);
    
    auto decoderWrapper = MockH264DecoderWrapper{};
    auto decoder = decoderWrapper.impl.get();
    auto config = DefaultConfig();
    config.spsNalUnits.push_back(unit);
    //std::span lacks == operator, so we have to use more complex matcher here
    EXPECT_CALL(*decoder, Decode(ElementsAreArray(unit))).WillOnce(Return(std::optional<DecodedImage>{DecodedImage{}}));
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(decoderWrapper)}), Error);

    decoderWrapper = MockH264DecoderWrapper{};
    decoder = decoderWrapper.impl.get();
    config = DefaultConfig();
    config.ppsNalUnits.push_back(unit);
    //std::span lacks == operator, so we have to use more complex matcher here
    EXPECT_CALL(*decoder, Decode(ElementsAreArray(unit))).WillOnce(Return(std::optional<DecodedImage>{DecodedImage{}}));
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(decoderWrapper)}), Error);
}

TEST(DecoderTests, ConstructionEmptyH264Stream)
{
    auto data = DefaultTestData();
    data.payload = std::vector<std::byte>{};
    auto segment = MakeSegment(data, 10s, 4s);
    ASSERT_THROW((DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment}), Error);
}

TEST(DecoderTests, DecodingCorruptedH264Stream)
{
    auto data = DefaultTestData();
    data.payload = std::vector<std::byte>{0_b,9_b};
    auto segment = MakeSegment(data, 10s, 4s);
    auto decoder = DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment};
    ASSERT_THROW(decoder.GetNext(), Error);
    
    data.payload = std::vector<std::byte>{0_b,0_b,0_b,2_b,0_b};
    segment = MakeSegment(data, 10s, 4s);
    decoder = DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment};
    ASSERT_THROW(decoder.GetNext(), Error);

    data.payload = std::vector<std::byte>{0_b,0_b,0_b,0_b};
    segment = MakeSegment(data, 10s, 4s);
    decoder = DecoderBase<MockH264DecoderWrapper>{DefaultConfig(),segment};
    ASSERT_THROW(decoder.GetNext(), Error);
}

namespace
{

template <std::ranges::range Range>
DecodedImage MakeImage(Range &&r)
{
    return
        DecodedImage{
            ArgbImage{ .data = std::vector<std::byte>(std::ranges::begin(r),std::ranges::end(r)),
                       .width = 0,
                       .height = 0 }};
}

}//unnamed namespace

TEST(DecoderTests, DecodingCorrectH264Stream)
{
    auto segment = MakeSegment(DefaultTestData(), 10s, 4s);
    auto h264DecoderWrapper = MockH264DecoderWrapper{};
    auto h264Decoder = h264DecoderWrapper.impl.get();
    auto config = DefaultConfig();
    auto decoder = DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(h264DecoderWrapper)};

    EXPECT_CALL(*h264Decoder,Decode)
        .WillRepeatedly([](auto const &data){ return MakeImage(data); });

    EXPECT_CALL(*h264Decoder,Retrieve)
        .WillOnce([](){ return std::optional<DecodedImage>{}; });

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(10s, decoder.TimestampNext());
    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(10s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(11s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData2,frame->image.data));
    ASSERT_EQ(11s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(12s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData3,frame->image.data));
    ASSERT_EQ(12s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(13s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData4,frame->image.data));
    ASSERT_EQ(13s,frame->timestamp);

    ASSERT_FALSE(decoder.HasMore());
    ASSERT_THROW(decoder.TimestampNext(), RangeError);
    ASSERT_FALSE(decoder.GetNext());
}

TEST(DecoderTests, DecodingCorrectH264StreamDelayedDecoding)
{
    auto segment = MakeSegment(DefaultTestData(), 10s, 4s);
    auto h264DecoderWrapper = MockH264DecoderWrapper{};
    auto h264Decoder = h264DecoderWrapper.impl.get();
    auto config = DefaultConfig();
    auto decoder = DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(h264DecoderWrapper)};

    EXPECT_CALL(*h264Decoder,Decode)
        .WillOnce([](auto const &){ return MakeImage(gNalUnitData1); })
        .WillOnce([](auto const &){ return std::optional<DecodedImage>{}; })
        .WillOnce([](auto const &){ return MakeImage(gNalUnitData2); })
        .WillOnce([](auto const &){ return std::optional<DecodedImage>{}; });

    EXPECT_CALL(*h264Decoder,Retrieve)
        .WillOnce([](){ return MakeImage(gNalUnitData3); })
        .WillOnce([](){ return MakeImage(gNalUnitData4); })
        .WillOnce([](){ return std::optional<DecodedImage>{}; });

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(10s, decoder.TimestampNext());
    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(10s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(11s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData2,frame->image.data));
    ASSERT_EQ(11s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(12s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData3,frame->image.data));
    ASSERT_EQ(12s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(13s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData4,frame->image.data));
    ASSERT_EQ(13s,frame->timestamp);

    ASSERT_FALSE(decoder.HasMore());
    ASSERT_THROW(decoder.TimestampNext(), RangeError);
    ASSERT_FALSE(decoder.GetNext());
}

TEST(DecoderTests, DecodingCorrectH264NonDisplayableFrame)
{
    auto data = DefaultTestData();
    auto &entries = dynamic_cast<AP4_TrunAtom *>(data.moofAtom->FindChild("traf/trun"))->UseEntries();
    entries[2].sample_composition_time_offset = 100500;
    auto segment = MakeSegment(data, 10s, 4s);
    auto h264DecoderWrapper = MockH264DecoderWrapper{};
    auto h264Decoder = h264DecoderWrapper.impl.get();
    auto config = DefaultConfig();
    auto decoder = DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(h264DecoderWrapper)};

    EXPECT_CALL(*h264Decoder,Decode)
        .WillOnce([](auto const &){ return MakeImage(gNalUnitData1); })
        .WillOnce([](auto const &){ return MakeImage(gNalUnitData2); })
        .WillOnce([](auto const &){ return std::optional<DecodedImage>{}; })
        .WillOnce([](auto const &){ return MakeImage(gNalUnitData4); });

    EXPECT_CALL(*h264Decoder,Retrieve)
        .WillOnce([](){ return std::optional<DecodedImage>{}; });

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(10s, decoder.TimestampNext());
    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(10s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(11s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData2,frame->image.data));
    ASSERT_EQ(11s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(13s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData4,frame->image.data));
    ASSERT_EQ(13s,frame->timestamp);

    ASSERT_FALSE(decoder.HasMore());
    ASSERT_THROW(decoder.TimestampNext(), RangeError);
    ASSERT_FALSE(decoder.GetNext());
}

TEST(DecoderTests, DecodingCorrectH264SkippingFrames)
{
    auto segment = MakeSegment(DefaultTestData(), 10s, 4s);
    auto h264DecoderWrapper = MockH264DecoderWrapper{};
    auto h264Decoder = h264DecoderWrapper.impl.get();
    auto config = DefaultConfig();
    auto decoder = DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(h264DecoderWrapper)};

    EXPECT_CALL(*h264Decoder,Decode)
        .WillRepeatedly([](auto const &data){ return MakeImage(data); });

    EXPECT_CALL(*h264Decoder,Retrieve)
        .WillOnce([](){ return std::optional<DecodedImage>{}; });

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(10s, decoder.TimestampNext());
    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(10s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(11s, decoder.TimestampNext());
    decoder.SkipNext();

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(12s, decoder.TimestampNext());
    decoder.SkipNext();

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(13s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData4,frame->image.data));
    ASSERT_EQ(13s,frame->timestamp);

    ASSERT_FALSE(decoder.HasMore());
    ASSERT_THROW(decoder.TimestampNext(), RangeError);
    ASSERT_FALSE(decoder.GetNext());
}

TEST(DecoderTests, DecodingCorrectH264MissingTrunEntry)
{
    auto data = DefaultTestData();
    auto trunAtom = dynamic_cast<AP4_TrunAtom *>(data.moofAtom->FindChild("traf/trun"));
    auto entries = trunAtom->GetEntries();
    entries.RemoveLast();
    trunAtom->SetEntries(entries);
    trunAtom->SetDataOffset(IntCast<AP4_SI32>(data.moofAtom->GetSize()));

    auto segment = MakeSegment(data, 10s, 4s);
    auto h264DecoderWrapper = MockH264DecoderWrapper{};
    auto h264Decoder = h264DecoderWrapper.impl.get();
    auto config = DefaultConfig();
    auto decoder = DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(h264DecoderWrapper)};

    EXPECT_CALL(*h264Decoder,Decode)
        .WillRepeatedly([](auto const &data){ return MakeImage(data); });

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(10s, decoder.TimestampNext());
    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(10s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(11s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData2,frame->image.data));
    ASSERT_EQ(11s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(12s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData3,frame->image.data));
    ASSERT_EQ(12s,frame->timestamp);

    //This part looks weird but it's because timestamps and h264 stream may be not concordant, so it is as it is
    ASSERT_FALSE(decoder.HasMore());
    ASSERT_THROW(decoder.TimestampNext(), RangeError);
    ASSERT_THROW(decoder.GetNext(), RangeError);

    ASSERT_FALSE(decoder.HasMore());
    ASSERT_THROW(decoder.TimestampNext(), RangeError);
    decoder.SkipNext();

    ASSERT_FALSE(decoder.HasMore());
    ASSERT_THROW(decoder.TimestampNext(), RangeError);
    ASSERT_FALSE(decoder.GetNext());
}

TEST(DecoderTests, DecodingCorrectH264MissingH264Frame)
{
    auto data = DefaultTestData();
    data.payload.clear();
    data.payload.insert(data.payload.end(),std::cbegin(gNalUnit1),std::cend(gNalUnit1));
    data.payload.insert(data.payload.end(),std::cbegin(gNalUnit2),std::cend(gNalUnit2));
    data.payload.insert(data.payload.end(),std::cbegin(gNalUnit3),std::cend(gNalUnit3));
    auto segment = MakeSegment(data, 10s, 4s);
    auto h264DecoderWrapper = MockH264DecoderWrapper{};
    auto h264Decoder = h264DecoderWrapper.impl.get();
    auto config = DefaultConfig();
    auto decoder = DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(h264DecoderWrapper)};

    EXPECT_CALL(*h264Decoder,Decode)
        .WillRepeatedly([](auto const &data){ return MakeImage(data); });

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(10s, decoder.TimestampNext());
    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(10s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(11s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData2,frame->image.data));
    ASSERT_EQ(11s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(12s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData3,frame->image.data));
    ASSERT_EQ(12s,frame->timestamp);

    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(13s,decoder.TimestampNext());
    ASSERT_FALSE(decoder.GetNext());
}

TEST(DecoderTests, ExceptionSafetyGetNext)
{
    auto segment = MakeSegment(DefaultTestData(), 10s, 4s);
    auto h264DecoderWrapper = MockH264DecoderWrapper{};
    auto h264Decoder = h264DecoderWrapper.impl.get();
    auto config = DefaultConfig();
    auto decoder = DecoderBase<MockH264DecoderWrapper>{config,segment,std::move(h264DecoderWrapper)};

    EXPECT_CALL(*h264Decoder,Decode)
        .WillRepeatedly([](auto const &data){ return MakeImage(data); });

    vd::internal::testing::gDecoderBase_ReleaseBuffer_ThrowValue = 1;
    ASSERT_THROW(decoder.GetNext(),int);
    ASSERT_EQ(10s, decoder.TimestampNext());

    vd::internal::testing::gDecoderBase_ReleaseBuffer_ThrowValue = 0;
    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(10s,frame->timestamp);
    ASSERT_TRUE(decoder.HasMore());
    ASSERT_EQ(11s, decoder.TimestampNext());
}



class MockDecoder
{
public:
    MOCK_METHOD(std::chrono::nanoseconds, TimestampNext, (), (const));
    MOCK_METHOD(bool, HasMore, (), (const, noexcept));
    MOCK_METHOD(std::optional<Frame>, GetNext, (), (const));
    MOCK_METHOD(void, SkipNext, ());
    MOCK_METHOD(void, SkipTo, (std::chrono::nanoseconds to));
};

class MockDecoderWrapper
{
public:
    std::chrono::nanoseconds TimestampNext() const
    {
        return impl->TimestampNext();
    }

    bool HasMore() const noexcept
    {
        return impl->HasMore();
    }

    std::optional<Frame> GetNext()
    {
        return impl->GetNext();
    }

    void SkipNext()
    {
        impl->SkipNext();
    }

    void SkipTo(std::chrono::nanoseconds to)
    {
        impl->SkipTo(to);
    }

    std::unique_ptr<MockDecoder> impl = std::make_unique<MockDecoder>();
};

TEST(SerialDecoderTests, ConstructionEmpty)
{
    ASSERT_THROW(SerialDecoderBase<MockDecoderWrapper>{{}}, ArgumentError);
}

TEST(SerialDecoderTests, DecodingTwoSegments)
{
    auto decoders = std::vector<MockDecoderWrapper>{};
    decoders.emplace_back();
    decoders.emplace_back();
    auto decoder1 = decoders[0].impl.get();
    auto decoder2 = decoders[1].impl.get();
    SerialDecoderBase serialDecoder{std::move(decoders)};

    auto counter1 = std::chrono::nanoseconds{0};
    EXPECT_CALL(*decoder1, GetNext)
        .WillOnce([&counter1]()->std::optional<Frame>{ return Frame{.image = ArgbImage{}, .timestamp = ++counter1}; })
        .WillRepeatedly(Return(std::optional<Frame>{}));
    ON_CALL(*decoder1, TimestampNext)
        .WillByDefault([&counter1]()->std::chrono::nanoseconds{ return counter1.count() != 0 ? throw RangeError{} : counter1 + 1ns; });
    ON_CALL(*decoder1, HasMore)
        .WillByDefault([&counter1](){ return counter1.count() == 0 ; });

    auto counter2 = std::chrono::nanoseconds{1};
    EXPECT_CALL(*decoder2, SkipNext).WillOnce([&counter2]() { ++counter2; });
    EXPECT_CALL(*decoder2, GetNext)
        .WillOnce([&counter2]()->std::optional<Frame>{ return Frame{.image = ArgbImage{}, .timestamp = ++counter2}; })
        .WillRepeatedly(Return(std::optional<Frame>{}));
    ON_CALL(*decoder2, TimestampNext)
        .WillByDefault([&counter2]()->std::chrono::nanoseconds{ return counter2.count() > 2 ? throw RangeError{} : counter2 + 1ns; });
    ON_CALL(*decoder2, HasMore)
        .WillByDefault([&counter2](){ return counter2.count() < 3; });

    ASSERT_TRUE(serialDecoder.HasMore());
    ASSERT_EQ(1ns, serialDecoder.TimestampNext());
    auto frame = serialDecoder.GetNext();
    ASSERT_EQ(1ns,frame->timestamp);

    ASSERT_TRUE(serialDecoder.HasMore());
    ASSERT_EQ(2ns, serialDecoder.TimestampNext());
    serialDecoder.SkipNext();

    ASSERT_TRUE(serialDecoder.HasMore());
    ASSERT_EQ(3ns, serialDecoder.TimestampNext());
    frame = serialDecoder.GetNext();
    ASSERT_EQ(3ns,frame->timestamp);

    ASSERT_FALSE(serialDecoder.HasMore());
    ASSERT_THROW(serialDecoder.TimestampNext(), RangeError);
    ASSERT_FALSE(serialDecoder.GetNext());
}



class FakeH264Decoder
{
public:
    using DecodedImage = DecodedImage;
    
    std::optional<DecodedImage> Decode(std::span<const std::byte> nalUnit, bool = false)
    {
        return MakeImage(nalUnit);
    }

    std::optional<DecodedImage> Retrieve(bool = false)
    {
        return {};
    }
};

template <class T>
concept DecoderSeekingTestFactoryConcept =
    requires()
{
    { T::Create() } -> DecoderConcept;
};

class DecoderFactory
{
public:
    static DecoderBase<FakeH264Decoder> Create(const DecodingConfig::UnitsType &nalUnits,
                                               std::chrono::nanoseconds offset,
                                               std::chrono::nanoseconds duration)
    {
        return DecoderBase<FakeH264Decoder>{DefaultConfig(),
                                            MakeSegment(MakeTestData(nalUnits, gDefaultDuration), offset, duration)};
    }

    static DecoderBase<FakeH264Decoder> Create()
    {
        return Create({MakeVector(gNalUnit1), MakeVector(gNalUnit2), MakeVector(gNalUnit3), MakeVector(gNalUnit4)}, 10s, 4s);
    }
};

class SerialDecoderFactory
{
public:
    static SerialDecoderBase<DecoderBase<FakeH264Decoder>> Create()
    {
        auto d1 = DecoderFactory::Create({MakeVector(gNalUnit1)}, 10s, 1s);
        auto d2 = DecoderFactory::Create({MakeVector(gNalUnit2), MakeVector(gNalUnit3)}, 11s, 2s);
        auto d3 = DecoderFactory::Create({MakeVector(gNalUnit4)}, 13s, 1s);
        return
            SerialDecoderBase<DecoderBase<FakeH264Decoder>>{
                {DecoderFactory::Create({MakeVector(gNalUnit1)}, 10s, 1s),
                 DecoderFactory::Create({MakeVector(gNalUnit2), MakeVector(gNalUnit3)}, 11s, 2s),
                 DecoderFactory::Create({MakeVector(gNalUnit4)}, 13s, 1s)}
            };
    }
};
 
template <DecoderSeekingTestFactoryConcept FactoryT>
class DecoderSeekingTestF : public ::testing::Test
{

};

using DecoderFactoryTypes = ::testing::Types<DecoderFactory, SerialDecoderFactory>;
TYPED_TEST_SUITE(DecoderSeekingTestF, DecoderFactoryTypes);

TYPED_TEST(DecoderSeekingTestF, Seeking)
{
    auto decoder = TypeParam::Create();

    ASSERT_ANY_THROW(decoder.SkipTo(9s));
    
    ASSERT_NO_THROW(decoder.SkipTo(10s));   
    ASSERT_EQ(10s, decoder.TimestampNext());

    auto frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData1,frame->image.data));
    ASSERT_EQ(11s, decoder.TimestampNext());

    ASSERT_NO_THROW(decoder.SkipTo(11500ms));   
    ASSERT_EQ(11s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData2,frame->image.data));
    ASSERT_EQ(12s, decoder.TimestampNext());

    ASSERT_NO_THROW(decoder.SkipTo(15s));   
    ASSERT_EQ(13s, decoder.TimestampNext());
    frame = decoder.GetNext();
    ASSERT_TRUE(std::ranges::equal(gNalUnitData4,frame->image.data));
    ASSERT_ANY_THROW(decoder.TimestampNext());
}

}//unnamed namespace