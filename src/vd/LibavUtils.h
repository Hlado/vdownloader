#ifndef VDOWNLOADER_VD__UTILS_H_
#define VDOWNLOADER_VD__UTILS_H_

#include "Libav.h"
#include "Sources.h"

#include <memory>

namespace vd::libav
{

class Reader final
{
public:
    //Behaviour when seeking with AVSEEK_SIZE flag is requested
    enum class SeekSizeMode
    {
        //Cache GetContentLength() result in constructor and always return
        //cached value
        Cache,
        
        //Use GetContentLength() method of source every time. Also used as
        //default when unknown mode is given
        Normal
    };

    explicit Reader(std::shared_ptr<SourceBase> source,
                    SeekSizeMode seekSizeMode = SeekSizeMode::Normal);
    
    Reader(const Reader &other) = delete;
    Reader &operator=(const Reader &other) = delete;
    Reader(Reader &&other) = delete;
    Reader &operator=(Reader &&other) = delete;

    int Read(std::span<std::byte> buf);
    std::int64_t Seek(std::int64_t offset, int whence);

    static int ReadPacket(void *opaque, std::uint8_t *buf, int size);
    static std::int64_t Seek(void *opaque, std::int64_t offset, int whence);

private:
    std::shared_ptr<SourceBase> mSrc;
    std::size_t mPos{0};
    std::size_t mSizeCache{0};
    SeekSizeMode mSeekSizeMode;

    std::size_t GetContentLength() const;
};



//All deleters take const pointers to be usable for
//unique_ptr<const ..., Deleter> scenario (it's not clear if it will ever be
//needed), shared_ptr seems to be fine with non-const deleters even for const
//template arguments
struct CodecContextDeleter
{
    void operator()(const AVCodecContext *p) const;
};

struct ParserContextDeleter
{
    void operator()(const AVCodecParserContext *p) const;
};

struct PacketDeleter
{
    void operator()(const AVPacket *p) const;
};

struct FormatContextDeleter
{
    void operator()(const AVFormatContext *p) const;
};

struct IoContextDeleter
{
    void operator()(const AVIOContext *p) const;
};

struct FrameDeleter
{
    void operator()(const AVFrame *p) const;
};

struct GenericDeleter
{
    void operator()(void *p) const;
};



std::chrono::nanoseconds ToNano(std::int64_t timestamp,
                                AVRational timeBase,
                                AVRounding rounding);
std::int64_t FromNano(std::chrono::nanoseconds timestamp,
                      AVRational timeBase,
                      AVRounding rounding);



template <typename T>
struct AvObjectTraits;

template <>
struct AvObjectTraits<AVCodecContext>
{
    using Deleter = CodecContextDeleter;
};

template <>
struct AvObjectTraits<const AVCodecContext>
{
    using Deleter = CodecContextDeleter;
};

template <>
struct AvObjectTraits<AVCodecParserContext>
{
    using Deleter = ParserContextDeleter;
};

template <>
struct AvObjectTraits<const AVCodecParserContext>
{
    using Deleter = ParserContextDeleter;
};

template <>
struct AvObjectTraits<AVFormatContext>
{
    using Deleter = FormatContextDeleter;
};

template <>
struct AvObjectTraits<const AVFormatContext>
{
    using Deleter = FormatContextDeleter;
};

template <>
struct AvObjectTraits<AVIOContext>
{
    using Deleter = IoContextDeleter;
};

template <>
struct AvObjectTraits<const AVIOContext>
{
    using Deleter = IoContextDeleter;
};

template <>
struct AvObjectTraits<AVPacket>
{
    using Deleter = PacketDeleter;
};

template <>
struct AvObjectTraits<const AVPacket>
{
    using Deleter = PacketDeleter;
};

template <>
struct AvObjectTraits<AVFrame>
{
    using Deleter = FrameDeleter;
};

template <>
struct AvObjectTraits<const AVFrame>
{
    using Deleter = FrameDeleter;
};



template <typename T>
using UniquePtr = std::unique_ptr<T, typename AvObjectTraits<T>::Deleter>;

using BufferPtr = std::unique_ptr<std::uint8_t, GenericDeleter>;



//Those function signatures are similiar to native libav ones, so arguments
//naming is preserved
UniquePtr<AVCodecContext> MakeCodecContext(const AVCodec *codec);

UniquePtr<AVCodecParserContext> MakeParserContext(AVCodecID codec_id);

UniquePtr<AVPacket> MakePacket();

UniquePtr<AVFormatContext> MakeFormatContext();

UniquePtr<AVIOContext> MakeIoContext(
    int buffer_size,
    int write_flag,
    void *opaque,
    int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
    int (*write_packet)(void *opaque, const uint8_t *buf, int buf_size),
    int64_t (*seek)(void *opaque, int64_t offset, int whence));

UniquePtr<AVFrame> MakeFrame();

BufferPtr MakeBuffer(std::size_t size);

}//namespace vd

#endif //VDOWNLOADER_VD__UTILS_H_