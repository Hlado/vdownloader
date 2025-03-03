#include "ImageFormats.h"
#include "Errors.h"
#include "Utils.h"

#include <format>

//Stb section
namespace
{

class StbiwError : public vd::Error
{
public:
    StbiwError(const char *condition)
        : Error{std::format(R"(stbi image writing library assertion "{}" failed)",
                            condition)}
    {
    
    }
};

void StbiwAssert(bool condition, const char *conditionStr)
{
    if(!condition)
    {
        throw StbiwError{conditionStr};
    }
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(condition) (StbiwAssert(condition, #condition))

#include <stb_image_write.h>

}//unnamed namespace



namespace vd
{

void WriteJpg(const std::filesystem::path &path,
              const Rgb32Image &rgbaImage,
              std::uint8_t quality)
{
    if(quality > 100)
    {
        throw ArgumentError{R"(parameter "quality" is greater than 100)"};
    }

    auto absolute = std::filesystem::absolute(path);
    auto err =
        stbi_write_jpg(
            absolute.string().c_str(),
            IntCast<int>(rgbaImage.Width()),
            IntCast<int>(rgbaImage.Height()),
            4,
            rgbaImage.Data().data(),
            quality);

    if(err == 0)
    {
        throw LibraryCallError{"stbi_write_jpg", err};
    }
}



void WritePng(const std::filesystem::path &path,
              const Rgb32Image &rgbaImage)
{
    auto absolute = std::filesystem::absolute(path);
    auto err =
        stbi_write_png(
            absolute.string().c_str(),
            IntCast<int>(rgbaImage.Width()),
            IntCast<int>(rgbaImage.Height()),
            4,
            rgbaImage.Data().data(),
            0); //stride will be calculated automatically if 0 is given

    if(err == 0)
    {
        throw LibraryCallError{"stbi_write_png", err};
    }
}



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



void WriteTga(std::ostream &stream, const Rgb32Image &bgraImage)
{

    TgaFileHeader header;
    header.SetDataType(2);
    header.SetWidth(UintCast<std::uint16_t>(bgraImage.Width()));
    header.SetHeight(UintCast<std::uint16_t>(bgraImage.Height()));
    header.SetPixSize(32);
    header.SetNumAttributeBits(8);
    //header.SetLeftToRightFlag(true);
    header.SetTopToBottomFlag(true);
    
    header.Write(stream);
    stream.write(reinterpret_cast<const char *>(bgraImage.Data().data()), bgraImage.Data().size_bytes());
    stream.flush();

    if(!stream)
    {
        throw Error{"failed writing to stream"};
    }
}

void WriteTga(const std::filesystem::path &path, const Rgb32Image &bgraImage)
{
    auto absolute = std::filesystem::absolute(path);
    std::filesystem::create_directories(absolute.parent_path());
    std::ofstream fs(absolute, std::ios::binary | std::ios_base::out);
    if(!fs)
    {
        throw Error{std::format(R"(failed to open file for writing image "{}")", absolute.string())};
    }

    WriteTga(fs, bgraImage);
}

}//namespace vd