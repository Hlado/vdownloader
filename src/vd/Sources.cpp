#include "Sources.h"
#include "Errors.h"

#include <ada.h>

#include <cstring>
#include <functional>
#include <limits>

using namespace httplib;

namespace vd
{

namespace internal
{

void AssertRangeCorrect(
    std::size_t pos, std::size_t bufSize, std::size_t contentLength)
{
    auto to = Add<std::size_t>(pos, bufSize);
    if(to > contentLength)
    {
        throw RangeError{};
    }
}

std::string_view ExtractAddress(std::string_view url)
{
    auto parsed = ada::parse(std::string{url});

    //Although it's weird to have colon included in protocol, it's not ada's
    //unique behaviour and google know other cases, so we leave it as is for now
    if(parsed->get_protocol() != "http:" &&
       parsed->get_protocol() != "https:")
    {
        //Is term "HTTP resource" correct?
        throw ArgumentError{std::format(R"("{}" is not an HTTP(S) resource address)", url)};
    }

    return url.substr(0, parsed->get_components().pathname_start);
}

std::string_view ExtractPathAndQuery(std::string_view url)
{
    auto components = ada::parse(std::string{url})->get_components();
    
    if(components.pathname_start >= url.size())
    {
        return "/";
    }

    return url.substr(components.pathname_start);
}

}//namespace internal

using namespace internal;



HttpSource::HttpSource(const std::string &url)
{
    auto headers = EstablishConnection(url);
    auto lookupRes = headers.equal_range("Content-Length");
    if(lookupRes.first == headers.end())
    {
        throw NotFoundError{R"(response doesn't contain header "Content-Length")"};
    }

    mContentLength = StrToUint<decltype(mContentLength)>(lookupRes.first->second);

    mIsRangeSupported = false;
    lookupRes = headers.equal_range("Accept-Ranges");
    if(lookupRes.first != headers.end() &&
       lookupRes.first->second == "bytes")
    {
        mIsRangeSupported = true;
    }
}

std::size_t HttpSource::GetContentLength() const noexcept
{
    return mContentLength;
}

void HttpSource::Read(std::size_t pos, std::span<std::byte> buf)
{
    //This precondition is essential for next checks
    if(buf.size_bytes() == 0)
    {
        return;
    }

    AssertRangeCorrect(pos, buf.size_bytes(), GetContentLength());

    constexpr auto fromMax = std::numeric_limits<decltype(Range::first)>::max();
    constexpr auto toMax = std::numeric_limits<decltype(Range::second)>::max();
    auto from = pos;
    auto to = buf.size_bytes() - 1 + pos;
    if(std::cmp_greater(from, fromMax) || std::cmp_greater(to, toMax))
    {
        throw RangeError{};
    }

    if(mIsRangeSupported)
    {
        auto range = make_range_header({{from, to}});
        auto requestRes = mHttpClient->Get(mRequestStr, {range});
        const auto &response = AssertRequestSuccessful(requestRes, PartialContent_206);
        auto body = response.body;
        AssertResponseLengthCorrect(buf.size_bytes(), body.size());

        std::memcpy(buf.data(), body.data(), body.size());
        return;
    }
    else
    {
        if(mCache.empty())
        {
            Cache();
        }
        
        std::memcpy(buf.data(), std::next(mCache.data(), from), buf.size_bytes());
        return;
    }
}

httplib::Headers HttpSource::EstablishConnection(std::string url)
{
    //Not good if infinite redirection is possible
    while(true)
    {
        mRequestStr = ExtractPathAndQuery(url);
        mHttpClient = std::make_unique<Client>(std::string{ExtractAddress(url)});
        mHttpClient->set_keep_alive(true);
        mHttpClient->set_follow_location(true);
        auto requestRes = mHttpClient->Head(mRequestStr);
        const auto &response = AssertRequestSuccessful(requestRes);
    
        //Must be filled only when redirection happened
        if(!response.location.empty())
        {
            url = response.location;
        }
        else
        {
            return response.headers;
        }
    }
}

void HttpSource::Cache()
{
    auto requestRes = mHttpClient->Get(mRequestStr);
    const auto &response = AssertRequestSuccessful(requestRes);

    auto body = response.body;
    AssertResponseLengthCorrect(mContentLength, body.size());

    mCache.resize(body.size());
    std::memcpy(mCache.data(), body.data(), body.size());
}

const Response &HttpSource::AssertRequestSuccessful(
    const Result &result,
    StatusCode expectedCode) const
{
    if(result.error() != httplib::Error::Success)
    {
        throw HttplibError{result.error()};
    }
    const auto &response = result.value();
    if(response.status != expectedCode)
    {
        throw HttpError{response.status,
                        response.reason,
                        mHttpClient->host() + mRequestStr};
    }

    return response;
}

void HttpSource::AssertResponseLengthCorrect(std::size_t requested, std::size_t actual)
{
    if(requested != actual)
    {
        throw Error{std::format("requested content length ({}) is not equal to response body size ({})",
                                requested,
                                actual)};
    }
}



MemoryViewSource::MemoryViewSource(std::span<const std::byte> buf)
    : mBuf(buf)
{

}

std::size_t MemoryViewSource::GetContentLength() const noexcept
{
    return mBuf.size_bytes();
}

void MemoryViewSource::Read(std::size_t pos, std::span<std::byte> buf)
{
    AssertRangeCorrect(pos, buf.size_bytes(), GetContentLength());

    std::memcpy(buf.data(), mBuf.data() + pos, buf.size_bytes());
}



//It's not clear if all this trickery with tellg/ignore is really needed,
//but it looks like most correct way to deal with ifstream
FileSource::FileSource(const std::filesystem::path &path)
{
    mFile.open(path, std::ios_base::in | std::ios_base::binary);
    if(!mFile)
    {
        throw Error{Format(R"(failed to open file "{}")", path.string())};
    }

    try
    {
        mStart = mFile.tellg();
        mSize = IntCast<decltype(mSize)>(std::filesystem::file_size(path));
    }
    catch(std::exception &e)
    {
        throw Error{Format(R"(failed to read file "{}": "{}")", path.string(), std::string{e.what()})};
    }
}

std::size_t FileSource::GetContentLength() const noexcept
{
    return mSize;
}

void FileSource::Read(std::size_t pos, std::span<std::byte> buf)
{
    if(buf.size_bytes() == 0)
    {
        return;
    }

    internal::AssertRangeCorrect(pos, buf.size_bytes(), GetContentLength());

    try
    {
        mFile.seekg(mStart);
        mFile.ignore(IntCast<std::streamsize>(pos));
        mFile.read(reinterpret_cast<char *>(buf.data()), IntCast<std::streamsize>(buf.size_bytes()));
        if(!mFile)
        {
            throw std::exception{};
        }
    }
    catch(std::exception &e)
    {
        throw Error{Format(R"(failed to read file: "{}")", std::string{e.what()})};
    }
}



std::size_t SourceBase::GetContentLength() const
{
    return GetContentLengthOverride();
}

void SourceBase::Read(std::size_t pos, std::span<std::byte> buf)
{
    return ReadOverride(pos, buf);
}

} //namespace vd