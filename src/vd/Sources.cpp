#include "Sources.h"

#include "Errors.h"
#include "Utils.h"

#include <Poco/URI.h>

#include <cstring>
#include <limits>

using namespace httplib;

namespace vd
{

static std::string ExtractAddress(std::string_view url)
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
        throw NotFoundError(R"(header "Content-Length")");
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

std::size_t HttpSource::Read(std::size_t pos, std::span<std::byte> buf)
{
    //This precondition is essential for next checks
    if(buf.size_bytes() == 0)
    {
        return 0;
    }

    constexpr auto posMax = std::numeric_limits<decltype(pos)>::max();
    if(posMax - pos < buf.size_bytes() - 1)
    {
        throw RangeError{};
    }

    constexpr auto fromMax = std::numeric_limits<decltype(Range::first)>::max();
    constexpr auto toMax = std::numeric_limits<decltype(Range::second)>::max();
    auto from = pos;
    auto to = pos + buf.size_bytes() - 1;
    if(std::cmp_greater(from, fromMax) || std::cmp_greater(to, toMax) || to >= mContentLength)
    {
        throw RangeError{};
    }

    if(mIsRangeSupported)
    {
        auto range = make_range_header({{from, to}});
        auto requestRes = mHttpClient->Get(mRequestStr, {range});
        const auto &response = AssertRequestSuccessful(requestRes, PartialContent_206);
        auto body = response.body;
        AssertResponseLengthOk(buf.size_bytes(), body.size());

        auto ret = body.size();
        std::memcpy(buf.data(), body.data(), ret);
        return ret;
    }
    else
    {
        if(mCache.empty())
        {
            Cache();
        }
       
        std::memcpy(buf.data(), std::next(mCache.data(), from), buf.size_bytes());
        return buf.size_bytes();
    }
}

void HttpSource::Cache()
{
    auto requestRes = mHttpClient->Get(mRequestStr);
    const auto &response = AssertRequestSuccessful(requestRes);

    auto body = response.body;
    AssertResponseLengthOk(mContentLength, body.size());

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

void HttpSource::AssertResponseLengthOk(std::size_t requested, std::size_t actual)
{
    if(requested != actual)
    {
        throw Error(std::format("requested content length ({}) is not equal to response body size ({})",
                                requested,
                                actual));
    }
}

} //namespace vd