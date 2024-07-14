#include "Decoder.h"
#include "Ap4ByteStream.h"
#include "Mp4Utils.h"
#include "OpenH264Decoder.h"
#include "Sources.h"
#include "Utils.h"

namespace vd
{

using namespace internal;

namespace
{

void ProcessParameterSetUnits(OpenH264Decoder &decoder, const std::vector<std::vector<std::byte>> &units)
{
    for(const auto &unit : units)
    {
        decoder.Decode(unit);
    }
}

std::span<const std::byte> ReadNalUnit(std::span<const std::byte> data, size_t sizeLen)
{
    std::uint32_t unitLen;
    if(sizeLen != sizeof(unitLen))
    {
        throw NotSupportedError{std::format("NAL unit size length ({}) is not supported", sizeLen)};
    }
    if(data.size_bytes() < sizeLen)
    {
        throw Error{"unexpected end of data"};
    }

    unitLen = EndianCastFrom<std::endian::big>(ReadBuffer<decltype(unitLen)>(data));
    if(unitLen < 1)
    {
        throw Error{"zero length NAL unit"};
    }
    if(unitLen > data.size_bytes() - sizeLen)
    {
        throw Error{"unexpected end of data"};
    }

    return data.subspan(sizeLen, unitLen);
}

std::size_t AssertDataOffset(const Segment &segment)
{
    auto stream = std::make_shared<Ap4ByteStream<MemoryViewSource>>(MemoryViewSource{segment.data});

    auto moofAtom = GetNextAtom<AP4_ContainerAtom>(stream, "moof");
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
    if(trunAtom->GetDataOffset() < 0)
    {
        throw Error("negastive offset");
    }

    auto offset = UintCast<std::size_t>(static_cast<std::uint32_t>(trunAtom->GetDataOffset()));
    if(offset > segment.data.size())
    {
        throw Error("offset is too big");
    }

    return offset;
}

}//unnamed namespace



Decoder::Decoder(AvcDecConfigRecord config, Segment segment)
    : mAvcConfig(std::move(config)),
      mSegment(std::move(segment))
{

}

std::optional<RgbImage> Decoder::GetFrame()
{
    if(!mRemainder)
    {
        Initialize();
    }

    while(mRemainder->size_bytes() > 0)
    {
        auto unit = ReadNalUnit(*mRemainder, mAvcConfig.unitLengthSize);
        auto decoded = mDecoder.Decode(unit);
        mRemainder = mRemainder->subspan(unit.size_bytes() + mAvcConfig.unitLengthSize);

        if(decoded.iBufferStatus == 1)
        {
            return decoded; //TODO
        }
    }

    return std::nullopt;
}

void Decoder::Initialize()
{
    ProcessParameterSetUnits(mDecoder, mAvcConfig.spsNalUnits);
    ProcessParameterSetUnits(mDecoder, mAvcConfig.ppsNalUnits);

    auto offset = AssertDataOffset(mSegment);

    mRemainder = std::span<const std::byte>(mSegment.data).subspan(offset);
}

} //namespace vd