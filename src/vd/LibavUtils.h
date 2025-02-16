#ifndef VDOWNLOADER_VD_LIBAV_UTILS_H_
#define VDOWNLOADER_VD_LIBAV_UTILS_H_

#include "DecodingUtils.h"
#include "Sources.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <memory>

namespace vd
{

struct LibavCodecContextDeleter
{
    void operator()(const AVCodecContext *p) const;
};

struct LibavParserContextDeleter
{
    void operator()(const AVCodecParserContext *p) const;
};

struct LibavPacketDeleter
{
    void operator()(const AVPacket *p) const;
};

struct LibavFormatContextDeleter
{
    void operator()(const AVFormatContext *p) const;
};

struct LibavIoContextDeleter
{
    void operator()(const AVIOContext *p) const;
};

struct LibavFrameDeleter
{
    void operator()(const AVFrame *p) const;
};

struct LibavGenericDeleter
{
    void operator()(void *p) const;
};



class LibavReader final
{
public:
    //Behaviour when libav requests seeking with AVSEEK_SIZE flag
    enum class SeekSizeMode
    {
        //Cache GetContentLength() result in constructor and always return
        //cached value
        Cache,
        
        //Use GetContentLength() method of source every time. Also used as
        //default when unknown mode is given
        Normal
    };

    explicit LibavReader(std::shared_ptr<SourceBase> source,
                         SeekSizeMode seekSizeMode = SeekSizeMode::Normal);
    
    LibavReader(const LibavReader &other) = delete;
    LibavReader &operator=(const LibavReader &other) = delete;
    LibavReader(LibavReader &&other) = delete;
    LibavReader &operator=(LibavReader &&other) = delete;

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



I420Image ToI420Image(const AVFrame &frame);

std::chrono::nanoseconds ToNano(std::int64_t timestamp,
                                AVRational timeBase,
                                AVRounding rounding);
std::int64_t FromNano(std::chrono::nanoseconds timestamp,
                      AVRational timeBase,
                      AVRounding rounding);

}//namespace vd

#endif //VDOWNLOADER_VD_LIBAV_UTILS_H_