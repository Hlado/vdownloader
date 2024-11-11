#include "Sources.h"
#include "Errors.h"

#include <Poco/URI.h>

#include <cstring>
#include <functional>
#include <limits>

using namespace httplib;

namespace vd
{

namespace
{

std::string ExtractAddress(std::string_view url)
{
    auto parsed = Poco::URI{std::string{url}};

    if(parsed.getScheme() != "http" &&
       parsed.getScheme() != "https")
    {
        //Is term "HTTP resource" correct?
        throw ArgumentError{std::format(R"("{}" is not an HTTP(S) resource address)", url)};
    }

    return parsed.getScheme() + "://" + parsed.getHost();
}

} //unnamed namespace

namespace internal
{

void AssertRangeCorrect(
    std::size_t pos, std::span<const std::byte> buf, std::size_t contentLength)
{
    if(buf.size_bytes() == 0)
    {
        throw ArgumentError{R"("buf" size must be greater than 0)"};
    }

    auto to = Add<std::size_t>(pos, buf.size_bytes() - 1);
    if(to >= contentLength)
    {
        throw RangeError{};
    }
}

}//namespace internal

using namespace internal;



HttpSource::HttpSource(const std::string &url)
    : mHttpClient{std::make_unique<Client>(ExtractAddress(url))},
      mRequestStr{Poco::URI{url}.getPathAndQuery()}
{
    mHttpClient->set_keep_alive(true);

    auto requestRes = mHttpClient->Head(mRequestStr);
    const auto &response = AssertRequestSuccessful(requestRes);

    auto headers = response.headers;
    auto lookupRes = headers.equal_range("Content-Length");
    if(lookupRes.first == headers.end())
    {
        throw NotFoundError(R"(response doesn't contain header "Content-Length")");
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

    AssertRangeCorrect(pos, buf, GetContentLength());

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
        throw Error(std::format("requested content length ({}) is not equal to response body size ({})",
                                requested,
                                actual));
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
    //This precondition is essential for next checks
    if(buf.size_bytes() == 0)
    {
        return;
    }

    internal::AssertRangeCorrect(pos, buf, GetContentLength());

    std::memcpy(buf.data(), mBuf.data() + pos, buf.size_bytes());
}

} //namespace vd