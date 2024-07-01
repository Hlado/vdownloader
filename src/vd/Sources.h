#ifndef VDOWNLOADER_VD_SOURCES_H_
#define VDOWNLOADER_VD_SOURCES_H_

//This file is included first because documentation states:
//"Include httplib.h before Windows.h or include Windows.h by
// defining WIN32_LEAN_AND_MEAN beforehand"
#include <httplib.h>

#include <concepts>
#include <span>
#include <string>
#include <vector>

namespace vd
{

template <class T>
concept SourceConcept =
    std::is_move_constructible<T>::value &&
    requires(T v, const T cv, std::size_t pos, std::span<std::byte> buf)
{
    { cv.GetContentLength() } -> std::same_as<std::size_t>;
    { v.Read(pos, buf) } -> std::same_as<std::size_t>;
};

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
    //Reading zero bytes performs no operation and returns instantly
    std::size_t Read(std::size_t pos, std::span<std::byte> buf);

private:
    //Only reason for it to be unique_ptr is the fact that Client doesn't
    //implement move assignment operator (likely by mistake, not by design)
    std::unique_ptr<httplib::Client> mHttpClient;
    std::string mRequestStr; 
    std::vector<std::byte> mCache;
    std::size_t mContentLength;
    bool mIsRangeSupported;

    void Cache();
    //Returns reference to result member
    const httplib::Response &AssertRequestSuccessful(
        const httplib::Result& result,
        httplib::StatusCode expectedCode = httplib::StatusCode::OK_200) const;
    static void AssertResponseLengthOk(std::size_t requested, std::size_t actual);
};

static_assert(std::is_nothrow_move_assignable_v<HttpSource>);
static_assert(std::is_nothrow_move_constructible_v<HttpSource>);

} //namespace vd

#endif //VDOWNLOADER_VD_SOURCES_H_