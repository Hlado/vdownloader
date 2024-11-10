#include "Decoder.h"

namespace vd
{

Frame Frame::Create(const ArgbImage &img, std::chrono::nanoseconds timestamp)
{
    return Frame{.image = img,
                 .timestamp = timestamp};
}



namespace internal
{

std::size_t AssertDataOffset(const Segment &segment, AP4_TrunAtom &trunAtom)
{
    if(trunAtom.GetDataOffset() < 0)
    {
        throw Error("negastive offset");
    }
    auto offset = IntCast<std::size_t>(trunAtom.GetDataOffset());
    if(offset >= segment.data.size())
    {
        throw Error("offset is too big");
    }

    return offset;
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

}//namespace internal

} //namespace vd