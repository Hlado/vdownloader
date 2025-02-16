#ifndef VDOWNLOADER_VD_SOURCES_H_
#define VDOWNLOADER_VD_SOURCES_H_

//This file is included first because documentation states:
//"Include httplib.h before Windows.h or include Windows.h by
// defining WIN32_LEAN_AND_MEAN beforehand"
#include <httplib.h>

#include "Errors.h"
#include "Utils.h"

#include <concepts>
#include <list>
#include <mutex>
#include <fstream>
#include <span>
#include <string>
#include <filesystem>
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
    void Read(std::size_t pos, std::span<std::byte> buf);

private:
    std::span<const std::byte> mBuf;
};



//File size is cached in constructor, shared access to underlying file
//is not supposed to happen, there will be issues
class FileSource final
{
public:
    explicit FileSource(const std::filesystem::path &path);

    FileSource(const FileSource &) = delete;
    FileSource &operator=(const FileSource &) = delete;
    FileSource(FileSource &&) = default;
    FileSource &operator=(FileSource &&) = default;

    std::size_t GetContentLength() const noexcept;
    void Read(std::size_t pos, std::span<std::byte> buf);

private:
    std::ifstream mFile;
    std::size_t mSize;
    std::ifstream::pos_type mStart;
};



//Wrapper for buffering read operations on sources.
//It is thread safe natively also.
//MaxChunks parameter sets maximum amount of cached chunks (unlimited if 0)
template <SourceConcept SourceT>
class CachedSource final
{
public:
    static const std::size_t cDefaultChunkSize = 1 << 19;

    explicit CachedSource(SourceT source,
                          std::size_t maxChunks = 1,
                          std::size_t chunkSize = cDefaultChunkSize);
    CachedSource(const CachedSource &) = delete;
    CachedSource &operator=(const CachedSource &) = delete;
    CachedSource(CachedSource &&) = default;
    CachedSource &operator=(CachedSource &&) = default;
    ~CachedSource() = default;

    std::size_t NumCachedChunks() const;
    std::size_t GetContentLength() const;
    //Reading zero bytes performs no operation and returns immediately
    void Read(std::size_t pos, std::span<std::byte> buf);

private:
    using Chunk = std::vector<std::byte>;

    struct Entry
    {
        std::size_t id;
        std::shared_ptr<Chunk> chunk;
    };

    using Entries = std::list<Entry>;
    using Index = std::unordered_map<std::size_t, typename Entries::iterator>;
    
    SourceT mSrc;
    Index mIndex;
    Entries mEntries;
    mutable std::unique_ptr<std::mutex> mSrcMtx;
    mutable std::unique_ptr<std::mutex> mCacheMtx;
    const std::size_t mMaxChunks;
    const std::size_t mChunkSize;

    std::size_t GetChunkId(std::size_t pos) const noexcept;
    std::shared_ptr<Chunk> GetChunk(std::size_t id);
    //In case of exception oldest chunk may be discarded but new chunk won't be inserted into cache
    std::shared_ptr<Chunk> CacheChunk(std::size_t id);
    std::shared_ptr<Chunk> ReadChunk(std::size_t id);
    //Precondition: at least 1 item is in cache
    void DiscardOldestChunk() noexcept;
};

template <SourceConcept SourceT>
CachedSource<SourceT>::CachedSource(SourceT source,
                                    std::size_t maxChunks,
                                    std::size_t chunkSize)
    : mSrc{std::move(source)},
      mSrcMtx{std::make_unique<std::mutex>()},
      mCacheMtx{std::make_unique<std::mutex>()},
      mMaxChunks{maxChunks},
      mChunkSize{chunkSize}
{
    if(mChunkSize < 1)
    {
        throw ArgumentError{"chunk size must be greater than 0"};
    }
}

template <SourceConcept SourceT>
std::size_t CachedSource<SourceT>::NumCachedChunks() const
{
    std::lock_guard lock(*mCacheMtx);
    return mIndex.size();
}

template <SourceConcept SourceT>
std::size_t CachedSource<SourceT>::GetContentLength() const
{
    std::lock_guard lock(*mSrcMtx);
    return mSrc.GetContentLength();
}

template <SourceConcept SourceT>
void CachedSource<SourceT>::Read(std::size_t pos, std::span<std::byte> buf)
{
    //We can't afford this, it will kill performance because of lock on source,
    //underlying source must also check range so it's not a problem
    //internal::AssertRangeCorrect(pos, buf, GetContentLength());

    auto remainder = buf.size_bytes();
    auto outPtr = buf.data();
    auto chunkId = GetChunkId(pos);
    while(remainder > 0)
    {
        auto chunk = GetChunk(chunkId);

        auto offset = pos - (chunkId * mChunkSize);
        auto len = std::min(remainder, mChunkSize - offset);

        //This check is needed because we disabled initial check but
        //last chunk may be shorter than others and need special care
        internal::AssertRangeCorrect(offset, std::span{(std::byte *)0, len}, chunk->size());

        std::memcpy(outPtr, std::next(chunk->data(), offset), len);

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
std::shared_ptr<typename CachedSource<SourceT>::Chunk>
    CachedSource<SourceT>::GetChunk(std::size_t id)
{
    {
        std::lock_guard lock(*mCacheMtx);

        auto indexIt = mIndex.find(id);
        if(indexIt != mIndex.end())
        {
            mEntries.splice(mEntries.end(), mEntries, indexIt->second);
            return indexIt->second->chunk;
        }
    }

    return CacheChunk(id);
}

template <SourceConcept SourceT>
std::shared_ptr<typename CachedSource<SourceT>::Chunk>
    CachedSource<SourceT>::CacheChunk(std::size_t id)
{
    //double caching
    auto chunk = ReadChunk(id);

    {
        std::lock_guard lock(*mCacheMtx);

        //Someone else could put chunk into cache while we were reading,
        //so we have to check again
        auto indexIt = mIndex.find(id);
        if(indexIt != mIndex.end())
        {
            return chunk;
        }

        if(mMaxChunks != 0 && !(mIndex.size() < mMaxChunks))
        {
            DiscardOldestChunk();
        }

        mEntries.push_back(Entry{.id = id, .chunk = chunk});
        try
        {
            mIndex.insert(std::make_pair(id, std::prev(mEntries.end())));
        }
        catch(...)
        {
            static_assert(noexcept(mEntries.pop_back()));
            mEntries.pop_back();
            throw;
        }
    }

    return chunk;
}

template <SourceConcept SourceT>
std::shared_ptr<typename CachedSource<SourceT>::Chunk>
    CachedSource<SourceT>::ReadChunk(std::size_t id)
{
    auto offset = id * mChunkSize;
    
    //GetContentLength() is thread safe already, no lock is needed
    auto len = std::min(mChunkSize, GetContentLength() - offset);
    
    //It's essential that last chunk has exact size and not just mChunkSize
    //because range check depends on it
    auto ret = std::make_shared<Chunk>(len);
    auto span = std::span<std::byte>{ret->data(), len};

    {
        std::lock_guard lock(*mSrcMtx);
        mSrc.Read(offset, span);
    }

    return ret;
}

template <SourceConcept SourceT>
void CachedSource<SourceT>::DiscardOldestChunk() noexcept
{
    mIndex.erase(mEntries.front().id);
    mEntries.pop_front();
}



//Wrapper to allow multithreaded operations on sources.
template <SourceConcept SourceT>
class ThreadSafeSource final
{
public:
    explicit ThreadSafeSource(SourceT source);

    ThreadSafeSource(const ThreadSafeSource &) = delete;
    ThreadSafeSource &operator=(const ThreadSafeSource &) = delete;
    ThreadSafeSource(ThreadSafeSource &&) = default;
    ThreadSafeSource &operator=(ThreadSafeSource &&) = default;
    ~ThreadSafeSource() = default;

    std::size_t GetContentLength() const;
    void Read(std::size_t pos, std::span<std::byte> buf);

private:
    SourceT mSrc;
    mutable std::unique_ptr<std::mutex> mMutex;
};

template <SourceConcept SourceT>
ThreadSafeSource<SourceT>::ThreadSafeSource(SourceT source)
    : mSrc{std::move(source)},
      mMutex{new std::mutex()}
{

}

template <SourceConcept SourceT>
std::size_t ThreadSafeSource<SourceT>::GetContentLength() const
{
    auto lock = std::lock_guard{*mMutex};
    return mSrc.GetContentLength();
}

template <SourceConcept SourceT>
void ThreadSafeSource<SourceT>::Read(std::size_t pos, std::span<std::byte> buf)
{
    auto lock = std::lock_guard{*mMutex};
    return mSrc.Read(pos, buf);
}



class SourceBase
{
public:
    virtual ~SourceBase() = default;

    std::size_t GetContentLength() const;
    void Read(std::size_t pos, std::span<std::byte> buf);

protected:
    virtual std::size_t GetContentLengthOverride() const = 0;
    virtual void ReadOverride(std::size_t pos, std::span<std::byte> buf) = 0;
};

template <SourceConcept SourceT>
class Source final : public SourceBase
{
public:
    explicit Source(SourceT source);

    Source(const Source &) = delete;
    Source &operator=(const Source &) = delete;
    Source(Source &&) = default;
    Source &operator=(Source &&) = default;

    ~Source() override = default;

protected:
    virtual std::size_t GetContentLengthOverride() const override;
    virtual void ReadOverride(std::size_t pos, std::span<std::byte> buf) override;

private:
    SourceT mSrc;
};

template <SourceConcept SourceT>
Source<SourceT>::Source(SourceT source)
    : mSrc{std::move(source)}
{

}

template <SourceConcept SourceT>
std::size_t Source<SourceT>::GetContentLengthOverride() const
{
    return mSrc.GetContentLength();
}

template <SourceConcept SourceT>
void Source<SourceT>::ReadOverride(std::size_t pos, std::span<std::byte> buf)
{
    mSrc.Read(pos, buf);
}

} //namespace vd

#endif //VDOWNLOADER_VD_SOURCES_H_