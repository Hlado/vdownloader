#include "Tga.h"
#include "Errors.h"
#include "Utils.h"

namespace vd
{

namespace
{

//No validation is performed
class TgaFileHeader final
{
public:
    std::uint8_t GetIdFieldLen() const
    {
        return mBuf[0];
    }

    void SetIdFieldLen(std::uint8_t val)
    {
        mBuf[0] = val;
    }

    std::uint8_t GetColorMapType() const
    {
        return mBuf[1];
    }

    void SetColorMapType(std::uint8_t val)
    {
        mBuf[1] = val;
    }

    std::uint8_t GetDataType() const
    {
        return mBuf[2];
    }

    void SetDataType(std::uint8_t val)
    {
        mBuf[2] = val;
    }

    std::uint16_t GetColorMapOg() const
    {
        return ReadUi16<3>();
    }

    void SetColorMapOg(std::uint16_t val)
    {
        WriteUi16<3>(val);
    }

    std::uint16_t GetColorMapLen() const
    {
        return ReadUi16<5>();
    }

    void SetColorMapLen(std::uint16_t val)
    {
        WriteUi16<5>(val);
    }

    std::uint8_t GetColorSize() const
    {
        return mBuf[7];
    }

    void SetColorSize(std::uint8_t val)
    {
        mBuf[7] = val;
    }

    std::uint16_t GetXOg() const
    {
        return ReadUi16<8>();
    }

    void SetXOg(std::uint16_t val)
    {
        WriteUi16<8>(val);
    }

    std::uint16_t GetYOg() const
    {
        return ReadUi16<10>();
    }

    void SetYOg(std::uint16_t val)
    {
        WriteUi16<10>(val);
    }

    std::uint16_t GetWidth() const
    {
        return ReadUi16<12>();
    }

    void SetWidth(std::uint16_t val)
    {
        WriteUi16<12>(val);
    }

    std::uint16_t GetHeight() const
    {
        return ReadUi16<14>();
    }

    void SetHeight(std::uint16_t val)
    {
        WriteUi16<14>(val);
    }

    std::uint8_t GetPixSize() const
    {
        return mBuf[16];
    }

    void SetPixSize(std::uint8_t val)
    {
        mBuf[16] = val;
    }

    std::uint8_t GetNumAttributeBits() const
    {
        return mBuf[17] & 0b1111;
    }

    void SetNumAttributeBits(std::uint8_t val)
    {
        mBuf[17] &= 0b11110000;
        mBuf[17] |= val & 0b1111;
    }

    bool GetRightToLeftFlag() const
    {
        return (mBuf[17] & 0b10000) != 0;
    }

    void SetRightToLeftFlag(bool val)
    {
        mBuf[17] &= 0b11101111;

        if(val)
        {
            mBuf[17] |= 0b10000;
        }
    }

    bool GetTopToBottomFlag() const
    {
        return (mBuf[17] & 0b100000) != 0;
    }

    void SetTopToBottomFlag(bool val)
    {
        mBuf[17] &= 0b11011111;

        if(val)
        {
            mBuf[17] |= 0b100000;
        }
    }

    std::ostream &Write(std::ostream &stream) const
    {
        return stream.write(reinterpret_cast<const char *>(mBuf), sizeof mBuf);
    }

private:
    std::uint8_t mBuf[18] = {};

    template <std::size_t N>
    std::uint16_t ReadUi16() const
    {
        static_assert(N < (sizeof mBuf) - 1);

        std::uint16_t ret;
        std::memcpy(&ret, &mBuf[N], sizeof ret);

        return EndianCastFrom<std::endian::little>(ret);
    }

    template <std::size_t N>
    void WriteUi16(std::uint16_t val)
    {
        static_assert(N < (sizeof mBuf) - 1);

        val = EndianCastTo<std::endian::little>(val);
        std::memcpy(&mBuf[N], &val, sizeof val);
    }
};

}//unnamed namespace



void WriteTga(std::ostream &stream, const std::vector<std::byte> &pixels, std::size_t width)
{
    if(width == 0)
    {
        throw ArgumentError{"width is zero"};
    }
    if(pixels.empty())
    {
        throw ArgumentError{"pixels vector is empty"};
    }
    if(pixels.size() % (width * 4) != 0)
    {
        throw ArgumentError{"wrong width or pixel format"};
    }

    TgaFileHeader header;
    header.SetDataType(2);
    header.SetWidth(UintCast<std::uint16_t>(width));
    header.SetHeight(UintCast<std::uint16_t>(pixels.size() / (width * 4)));
    header.SetPixSize(32);
    header.SetNumAttributeBits(8);
    //header.SetLeftToRightFlag(true);
    header.SetTopToBottomFlag(true);
    
    header.Write(stream);
    stream.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
    stream.flush();

    if(!stream)
    {
        throw Error{"failed writing to stream"};
    }
}

}//namespace vd