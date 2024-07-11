#include <vd/Sources.h>
#include <vd/Errors.h>

#include <gtest/gtest.h>

#include <future>
#include <regex>
#include <thread>

using namespace vd;



const std::string gContent = "This is content for testing";
const std::string gContentType = "text/plain";
const std::string gHostname = "localhost";
const std::string gAddress = "http://" + gHostname;
const std::string gContentPath = "/content";
const std::string gUrl = gAddress + gContentPath;
const std::string gRangesContentPath = "/content_ranges";
const std::string gUrlRanges = gAddress + gRangesContentPath;
constexpr int gPort = 80;



class SourceBaseTestF : public ::testing::Test
{
protected:
    httplib::Server mServer;
    std::future<void> mFuture;

    void SetUp() override
    {
        using namespace std::chrono_literals;

        mFuture = std::async(
            std::launch::async,
            [this]()
        {
            mServer.Get(
                gContentPath,
                [](const httplib::Request &, httplib::Response &res)
            {
                res.set_header("Accept-Ranges", "none");
                res.set_content(gContent, gContentType);
            });

            mServer.Get(
                gRangesContentPath,
                [](const httplib::Request &, httplib::Response &res)
            {
                res.set_header("Accept-Ranges", "bytes");
                //Server handles ranges on it's own
                res.set_content(gContent, gContentType);
            });

            if(!mServer.listen(gHostname, gPort))
            {
                throw std::exception{"HTTP server reported error"};
            }
        });


        while(!mServer.is_running())
        {
            if(mFuture.wait_for(10ms) == std::future_status::ready)
            {
                break;
            }
        }
    }

    void TearDown() override
    {
        mServer.stop();
        mFuture.get();
    }
};



class HttpSourceTestF : public SourceBaseTestF
{

};

TEST_F(HttpSourceTestF, NonHttpSchemeThrows)
{
    ASSERT_THROW(HttpSource{"ftp://address"}, ArgumentError);
    ASSERT_NO_THROW(HttpSource{gUrl});
    ASSERT_THROW(HttpSource{"https://address"}, HttplibError);
}

TEST_F(HttpSourceTestF, ReadFull)
{
    auto test = [](const std::string &url)
    {
        auto zero = std::string(gContent.size(), '\0');
        auto buf = zero;
        auto span = std::span<char>{buf};
        HttpSource{url}.Read(0, std::as_writable_bytes(span));
        ASSERT_EQ(gContent, buf);
    };

    test(gUrl);
    test(gUrlRanges);
}

TEST_F(HttpSourceTestF, ReadPartial)
{
    auto test = [](const std::string &url)
    {
        auto len = gContent.size() / 2;
        auto zero = std::string(gContent.size(), '\0');
        auto half = zero;
        half.replace(0, len, gContent, 0, len);

        auto buf = zero;
        auto span = std::span<char>{buf.data(), len};
        auto src = HttpSource{url};
        src.Read(0, std::as_writable_bytes(span));
        ASSERT_EQ(half, buf);
        span = std::span<char>{std::next(buf.data(), len), std::next(buf.data(), gContent.size())};
        src.Read(len, std::as_writable_bytes(span));
        ASSERT_EQ(gContent, buf);
    };

    test(gUrl);
    test(gUrlRanges);
}



class CachedSourceTestF : public SourceBaseTestF
{

};

struct FakeSource
{
    std::size_t GetContentLength() const { throw NotImplementedError{}; }
    void Read(std::size_t, std::span<std::byte>) { throw NotImplementedError{}; }
};

TEST_F(CachedSourceTestF, ZeroChunkSizeThrows)
{
    ASSERT_THROW(CachedSource(FakeSource{}, 1, 0), ArgumentError);
}

TEST_F(CachedSourceTestF, ReadFullOneChunkIsEnough)
{
    auto buf = std::string(gContent.size(), '\0');
    auto span = std::span<char>{buf};
    CachedSource{HttpSource{gUrl}, 0, gContent.size()}.Read(0, std::as_writable_bytes(span));
    ASSERT_EQ(gContent, buf);
}

TEST_F(CachedSourceTestF, ReadFullLittleChunksNoCache)
{
    auto buf = std::string(gContent.size(), '\0');
    auto span = std::span<char>{buf};
    CachedSource{HttpSource{gUrl}, 1, 1}.Read(0, std::as_writable_bytes(span));
    ASSERT_EQ(gContent, buf);
}

TEST_F(CachedSourceTestF, ReadFullLittleChunksUnlimitedCache)
{
    auto buf = std::string(gContent.size(), '\0');
    auto span = std::span<char>{buf};
    CachedSource{HttpSource{gUrl}, 0, 1}.Read(0, std::as_writable_bytes(span));
    ASSERT_EQ(gContent, buf);
}

TEST_F(CachedSourceTestF, ReadPartialNeighborChunksRepeated)
{
    auto half = gContent.size() / 2;
    auto quarter = gContent.size() / 4;
    auto eighth = gContent.size() / 8;

    auto buf = std::string(half, '\0');
    auto span = std::span<char>{buf};
    auto src = CachedSource{HttpSource{gUrl}, 0, half};

    src.Read(quarter, std::as_writable_bytes(span));
    ASSERT_EQ(gContent.substr(quarter, half), buf);

    buf = std::string(half, '\0');
    src.Read(eighth, std::as_writable_bytes(span));
    ASSERT_EQ(gContent.substr(eighth, half), buf);
}



template <typename SourceT>
class SourceTestF : public SourceBaseTestF
{

};

struct HttpSourceWrapper
{
    HttpSource impl;

    explicit HttpSourceWrapper(const std::string &url)
        : impl{url}
    {
    
    }

    HttpSourceWrapper(HttpSourceWrapper &&) = default;
    HttpSourceWrapper &operator=(HttpSourceWrapper &&) = default;

    std::size_t GetContentLength() const noexcept
    {
        return impl.GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        return impl.Read(pos, buf);
    }
};

struct ChunkedSourceWrapper
{
    CachedSource<HttpSource> impl;

    explicit ChunkedSourceWrapper(const std::string &url)
        : impl{HttpSource{url}}
    {

    }

    ChunkedSourceWrapper(ChunkedSourceWrapper &&) = default;
    ChunkedSourceWrapper &operator=(ChunkedSourceWrapper &&) = default;

    std::size_t GetContentLength() const noexcept
    {
        return impl.GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        return impl.Read(pos, buf);
    }
};

using SourceTypes = ::testing::Types<HttpSourceWrapper, ChunkedSourceWrapper>;
TYPED_TEST_SUITE(SourceTestF, SourceTypes);

TYPED_TEST(SourceTestF, ContentLengthIsAvailableAfterConstruction)
{
    ASSERT_EQ(gContent.size(), TypeParam{gUrl}.GetContentLength());
}

TYPED_TEST(SourceTestF, ReadingZero)
{
    ASSERT_NO_THROW(TypeParam{gUrl}.Read(0, std::span<std::byte>{(std::byte *)nullptr, 0}));
}

TYPED_TEST(SourceTestF, ReadingNonsenseSpanThrows)
{
    auto src = TypeParam{gUrl};
    auto arrExtreme = std::array<std::byte, 2>{};
    auto bufExtreme = std::span(arrExtreme);

    ASSERT_THROW(src.Read(std::numeric_limits<std::size_t>::max(), bufExtreme), RangeError);
    ASSERT_THROW(src.Read(std::numeric_limits<httplib::Range::first_type>::max(), bufExtreme), RangeError);
    ASSERT_THROW(src.Read(std::numeric_limits<httplib::Range::second_type>::max(), bufExtreme), RangeError);

    auto arr = std::array<std::byte, 1>{};
    auto buf = std::span(arr);
    src.Read(gContent.size() - 1u, buf);
    ASSERT_EQ((std::byte)*gContent.crbegin(), *arr.cbegin());
    ASSERT_THROW(src.Read(gContent.size(),buf), RangeError);
}