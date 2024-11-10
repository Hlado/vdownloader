#ifndef VDOWNLOADER_VD_AP4_BYTE_STREAM_H_
#define VDOWNLOADER_VD_AP4_BYTE_STREAM_H_

//This file is included first because documentation states:
//"Include httplib.h before Windows.h or include Windows.h by
// defining WIN32_LEAN_AND_MEAN beforehand"
#include <httplib.h>

#include "Ap4Headers.h"
#include "Errors.h"
#include "Sources.h"
#include "Utils.h"

#include <concepts>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace vd
{

template <SourceConcept SourceT>
class Ap4ByteStream : public AP4_ByteStream
{
public:
    explicit Ap4ByteStream(SourceT source);
    Ap4ByteStream(const Ap4ByteStream &) = delete;
    Ap4ByteStream &operator=(const Ap4ByteStream &) = delete;
    Ap4ByteStream(Ap4ByteStream &&) = default;
    Ap4ByteStream &operator=(Ap4ByteStream &&) = default;
    ~Ap4ByteStream() override = default;

    std::size_t GetSize() const noexcept(noexcept(mSrc.GetContentLength()));

    AP4_Result ReadPartial(void *buf, AP4_Size len, AP4_Size &numRead) override;
    AP4_Result WritePartial(const void *buf, AP4_Size len, AP4_Size &numWritten) override;
    AP4_Result Seek(AP4_Position pos) noexcept override;
    AP4_Result Tell(AP4_Position &pos) noexcept override;
    AP4_Result GetSize(AP4_LargeSize &size) noexcept override;

private:
    SourceT mSrc;
    AP4_Position mPos = 0;
};

template <SourceConcept SourceT>
Ap4ByteStream<SourceT>::Ap4ByteStream(SourceT source)
    : mSrc(std::move(source))
{

}

template <SourceConcept SourceT>
std::size_t Ap4ByteStream<SourceT>::GetSize() const noexcept(noexcept(mSrc.GetContentLength()))
{
    return mSrc.GetContentLength();
}

//It's unclear how safe is it to throw exceptions here,
//it seems Bento4 is more error-code oriented,
//so probably for stability it's better not to throw inside those functions
template <SourceConcept SourceT>
AP4_Result Ap4ByteStream<SourceT>::ReadPartial(
    void *buf, AP4_Size len, AP4_Size &numRead)
{
    numRead = 0;

    try
    {
        if(mPos >= GetSize())
        {
            return AP4_ERROR_EOS;
        }

        auto span = std::span{static_cast<std::byte *>(buf), UintCast<std::size_t>(len)};
        mSrc.Read(mPos, span);
        numRead = UintCast<AP4_Size>(span.size_bytes());
        mPos += span.size_bytes();
    }
    catch(const std::exception &e)
    {
        Errorln(e.what());
        return AP4_FAILURE;
    }

    return AP4_SUCCESS;
}

template <SourceConcept SourceT>
AP4_Result Ap4ByteStream<SourceT>::WritePartial(
    const void *, AP4_Size , AP4_Size &)
{
    return AP4_ERROR_NOT_SUPPORTED;
}

template <SourceConcept SourceT>
AP4_Result Ap4ByteStream<SourceT>::Seek(AP4_Position pos) noexcept
{
    if(std::cmp_greater(pos, GetSize()))
    {
        return AP4_ERROR_OUT_OF_RANGE;
    }

    mPos = pos;
    return AP4_SUCCESS;
}

template <SourceConcept SourceT>
AP4_Result Ap4ByteStream<SourceT>::Tell(AP4_Position &pos) noexcept
{
    pos = mPos;
    return AP4_SUCCESS;
}

template <SourceConcept SourceT>
AP4_Result Ap4ByteStream<SourceT>::GetSize(AP4_LargeSize &size) noexcept
{
    try
    {
        size = UintCast<AP4_LargeSize>(GetSize());
    }
    catch(const std::exception &e)
    {
        Errorln(e.what());
        return AP4_FAILURE;
    }

    return AP4_SUCCESS;
}

using Ap4HttpByteStream = Ap4ByteStream<CachedSource<HttpSource>>;

} //namespace vd

#endif //VDOWNLOADER_VD_AP4_BYTE_STREAM_H_