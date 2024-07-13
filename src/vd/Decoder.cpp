#include "Decoder.h"
#include "Ap4ByteStream.h"
#include "Mp4Utils.h"
#include "Sources.h"
#include "Utils.h"

namespace vd
{

using namespace internal;

namespace
{

template <typename T>
void discard(const T &)
{

}

}


namespace
{

void ProcessParameterSetUnits(const std::vector<std::vector<std::byte>> &units)
{
    discard(units);
    /*for(const auto &unit : units)
    {
        //PopulateStream(stream, unit);

        if(auto err = stream.BroadwayDecode(); err != h264::STREAM_ENDED)
        {
            throw LibraryCallError{"h264::Stream::BroadwayDecode", err};
        }
    }*/
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

}//unnamed namespace

Decoder::Decoder(AvcDecConfigRecord config)
    : mAvcConfig(std::move(config))
{
    
}

void Decoder::Decode(const AvcDecConfigRecord &config,
                     const Segment &segment,
                     const std::function<void (const std::byte *, std::size_t)> &handler)
{
    ProcessParameterSetUnits(config.spsNalUnits);
    ProcessParameterSetUnits(config.ppsNalUnits);
    
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

    auto start = UintCast<std::size_t>(static_cast<std::uint32_t>(trunAtom->GetDataOffset()));
    auto data = std::span<const std::byte>(segment.data).subspan(start);
    while(data.size_bytes() > 0)
    {
        auto unit = ReadNalUnit(data, config.unitLengthSize);
        discard(unit);

        data = data.subspan(unit.size_bytes() + config.unitLengthSize);
    }

    handler(nullptr, 0);
}

void Decoder::Decode(const Segment &segment,
                     const std::function<void (const std::byte *, std::size_t)> &handler) const
{
    Decode(mAvcConfig, segment, handler);
}

} //namespace vd