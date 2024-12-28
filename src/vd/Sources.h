#ifndef VDOWNLOADER_VD_SOURCES_H_
#define VDOWNLOADER_VD_SOURCES_H_

//This file is included first because documentation states:
//"Include httplib.h before Windows.h or include Windows.h by
// defining WIN32_LEAN_AND_MEAN beforehand"
#include <httplib.h>

#include "Errors.h"
#include "Utils.h"

#include <concepts>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace vd
{

template <class T>
concept SourceConcept =
    std::is_move_constructible<T>::value &&
    requires(T v, const T cv, std::size_t pos, std::span<std::byte> buf)
    {
        { cv.GetContentLength() } -> std::same_as<std::size_t>;
        { v.Read(pos, buf) } -> std::same_as<void>;
    };



namespace internal
{

//buf size must be greater than 0
void AssertRangeCorrect(
    std::size_t pos, std::span<const std::byte> buf, std::size_t contentLength);

} //namespace internal

//Class to access web resource by HTTP(S) protocol. Establish keep-alive connection on creation.
//If server doesn't support range requests (or range unit isn't byte), content is downloaded
//and cached on first Read invocation.
class HttpSource final
{
public:
    explicit HttpSource(const std::string &url);
    HttpSource(const HttpSource &) = delete;
    HttpSource &operator=(const HttpSource &) = delete;
    HttpSource(HttpSource &&) = default;
    HttpSource &operator=(HttpSource &&) = default;

    std::size_t GetContentLength() const noexcept;
    //Reading zero bytes performs no operation and returns immediately
    void Read(std::size_t pos, std::span<std::byte> buf);

private:
    //Only reason for it to be unique_ptr is the fact that Client doesn't
    //implement move assignment operator (likely by mistake, not by design)
    std::unique_ptr<httplib::Client> mHttpClient;
    std::string mRequestStr; 
    std::vector<std::byte> mCache;
    std::size_t mContentLength;
    bool mIsRangeSupported;

    httplib::Headers EstablishConnection(std::string url);
    void Cache();
    //Returns reference to result member
    const httplib::Response &AssertRequestSuccessful(
        const httplib::Result& result,
        httplib::StatusCode expectedCode = httplib::StatusCode::OK_200) const;
    static void AssertResponseLengthCorrect(std::size_t requested, std::size_t actual);
};

static_assert(std::is_nothrow_move_assignable_v<HttpSource>);
static_assert(std::is_nothrow_move_constructible_v<HttpSource>);



class MemoryViewSource final
{
public:
    explicit MemoryViewSource(std::span<const std::byte> buf = {});

    std::size_t GetContentLength() const noexcept;
    //Reading zero bytes performs no operation and returns immediately
    void Read(std::size_t pos, std::span<std::byte> buf);

private:
    std::span<const std::byte> mBuf;
};



//Wrapper for buffering read operations on sources.
//MaxChunks parameter sets maximum amount of cached chunks (unlimited if 0)
template <SourceConcept SourceT>
class CachedSource final
{
public:
    static const std::size_t cDefaultChunkSize = 1 << 19;

    explicit CachedSource(SourceT source,
                          std::size_t maxChunks = 1,
                          std::size_t chunkSize = cDefaultChunkSize);
    CachedSource(const CachedSource &) = default;
    CachedSource &operator=(const CachedSource &) = default;
    CachedSource(CachedSource &&) = default;
    CachedSource &operator=(CachedSource &&) = default;
    ~CachedSource() = default;

    std::size_t GetContentLength() const noexcept(noexcept(mSrc.GetContentLength()));
    //Reading zero bytes performs no operation and returns immediately
    void Read(std::size_t pos, std::span<std::byte> buf);
private:
    using Chunk = std::vector<std::byte>;
    using Map = std::unordered_map<std::size_t, Chunk>;
    using History = std::vector<std::size_t>;
    
    SourceT mSrc;
    Map mCache;
    History mHistory;
    const std::size_t mMaxChunks;
    const std::size_t mChunkSize;

    std::size_t GetChunkId(std::size_t pos) const noexcept;
    //In case of exception oldest chunk may be discarded but new chunk won't be inserted into cache
    Map::iterator CacheChunk(std::size_t id);
    Chunk ReadChunk(std::size_t id);
    //Precondition: at least 1 item is in cache
    void DiscardOldestChunk() noexcept;
};

template <SourceConcept SourceT>
CachedSource<SourceT>::CachedSource(SourceT source,
                                    std::size_t maxChunks,
                                    std::size_t chunkSize)
    : mSrc{std::move(source)},
      mMaxChunks{maxChunks},
      mChunkSize{chunkSize}
{
    if(mChunkSize < 1)
    {
        throw ArgumentError("chunk size must be greater than 0");
    }
}

template <SourceConcept SourceT>
std::size_t CachedSource<SourceT>::GetContentLength() const noexcept(noexcept(mSrc.GetContentLength()))
{
    return mSrc.GetContentLength();
}

template <SourceConcept SourceT>
void CachedSource<SourceT>::Read(std::size_t pos, std::span<std::byte> buf)
{
    //This precondition is essential for next checks
    if(buf.size_bytes() == 0)
    {
        return;
    }

    internal::AssertRangeCorrect(pos, buf, GetContentLength());

    auto remainder = buf.size_bytes();
    auto outPtr = buf.data();
    auto chunkId = GetChunkId(pos);
    while(remainder > 0)
    {
        auto chunkIt = mCache.find(chunkId);
        if(chunkIt == mCache.end())
        {
            chunkIt = CacheChunk(chunkId);
        }
        const auto &chunk = chunkIt->second;

        auto offset = pos - (chunkId * mChunkSize);
        auto len = std::min(remainder, mChunkSize - offset);
        std::memcpy(outPtr, std::next(chunk.data(), offset), len);

        std::advance(outPtr, len);
        remainder -= len;
        pos += len;
        chunkId += 1;
    }
}

template <SourceConcept SourceT>
std::size_t CachedSource<SourceT>::GetChunkId(std::size_t pos) const noexcept
{
    return pos / mChunkSize;
}

template <SourceConcept SourceT>
CachedSource<SourceT>::Map::iterator CachedSource<SourceT>::CacheChunk(std::size_t id)
{
    auto chunk = ReadChunk(id);

    if(mMaxChunks != 0 && !(mHistory.size() < mMaxChunks))
    {
        DiscardOldestChunk();
    }
    
    mHistory.push_back(id);
    try
    {
        mCache.insert(std::make_pair(id, std::move(chunk))).first;
    }
    catch(...)
    {
        static_assert(noexcept(mHistory.pop_back()));
        mHistory.pop_back();
        throw;
    }

    return mCache.find(id);
}

template <SourceConcept SourceT>
CachedSource<SourceT>::Chunk CachedSource<SourceT>::ReadChunk(std::size_t id)
{
    Chunk ret(mChunkSize);
    auto offset = id * mChunkSize;
    auto len = std::min(mChunkSize, GetContentLength() - offset);
    auto span = std::span<std::byte>{ret.data(), len};
    mSrc.Read(offset, span);
    return ret;
}

template <SourceConcept SourceT>
void CachedSource<SourceT>::DiscardOldestChunk() noexcept
{
    static_assert(noexcept(*mHistory.cbegin()));
    static_assert(std::is_same_v<Map, std::unordered_map<std::size_t, Chunk>>);
    mCache.erase(*mHistory.cbegin());

    static_assert(std::is_same_v<History, std::vector<std::size_t>>);
    //It's not confirmed yet, but vector likely prefers move assignment when available
    static_assert(
        (std::is_move_assignable_v<History::value_type> && std::is_nothrow_move_assignable_v<History::value_type>)
            || (!std::is_move_assignable_v<History::value_type> && std::is_nothrow_assignable_v<History::value_type, History::value_type>));
    mHistory.erase(mHistory.cbegin());
}

} //namespace vd

#endif //VDOWNLOADER_VD_SOURCES_H_