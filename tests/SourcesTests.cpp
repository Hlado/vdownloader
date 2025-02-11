#include <vd/Sources.h>
#include <vd/Errors.h>
#include <vd/Preprocessor.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>
#include <regex>
#include <thread>


using namespace vd;
using namespace testing;
using namespace std::chrono_literals;

namespace
{

//It's not yet clear what port is better to use for testing purposes,
//but IANA reserved ones look promising because it must guarantee (almost)
//no collision for short lived apps like tests
//
//And, of course, something that works in windows won't work in ubuntu and vice-versa
#if defined(VDOWNLOADER_OS_WINDOWS)
    constexpr int gPort = 1027;
#else
    constexpr int gPort = 49151;
#endif

const std::string gContent = "This is content for testing";
const std::string gContentType = "text/plain";
const std::string gHostname = "localhost";
const std::string gAddress = "http://" + gHostname + ":" + std::to_string(gPort);
const std::string gContentPath = "/content";
const std::string gUrl = gAddress + gContentPath;
const std::string gRangesContentPath = "/content_ranges";
const std::string gUrlRanges = gAddress + gRangesContentPath;
auto gContentSpan = std::as_bytes(std::span<const char>{gContent.cbegin(), gContent.size()});
auto gDefaultSource = MemoryViewSource{gContentSpan};



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
                throw std::runtime_error{"HTTP server failed to start"};
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



class MockSource
{
public:
    MOCK_METHOD(void, Read, (std::size_t pos, std::span<std::byte> buf));
    MOCK_METHOD(std::size_t, GetContentLength, (), (const));

    MockSource()
    {
        ON_CALL(*this, Read)
            .WillByDefault(Return());
    }
};

class MockSourceWrapper
{
public:
    std::size_t GetContentLength() const
    {
        return impl->GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        return impl->Read(pos, buf);
    }

    std::unique_ptr<MockSource> impl = std::make_unique<MockSource>();
};

TEST(CachedSourceTests, ZeroChunkSizeThrows)
{
    ASSERT_THROW(CachedSource(MemoryViewSource{}, 1, 0), ArgumentError);
}

TEST(CachedSourceTests, ReadFullOneChunkIsEnough)
{
    auto buf = std::string(gContent.size(), '\0');
    auto span = std::span<char>{buf};
    auto source = CachedSource{gDefaultSource, 0, gContent.size()};
    source.Read(0, std::as_writable_bytes(span));
    ASSERT_EQ(gContent, buf);
    ASSERT_EQ(1, source.NumCachedChunks());
}

TEST(CachedSourceTests, ReadFullLittleChunksNoCache)
{
    auto buf = std::string(gContent.size(), '\0');
    auto span = std::span<char>{buf};
    auto source = CachedSource{gDefaultSource, 1, 1};
    source.Read(0, std::as_writable_bytes(span));
    ASSERT_EQ(gContent, buf);
    ASSERT_EQ(1, source.NumCachedChunks());
}

TEST(CachedSourceTests, ReadFullLittleChunksUnlimitedCache)
{
    auto buf = std::string(gContent.size(), '\0');
    auto span = std::span<char>{buf};
    auto source = CachedSource{gDefaultSource, 0, 1};
    source.Read(0, std::as_writable_bytes(span));
    ASSERT_EQ(gContent, buf);
    ASSERT_EQ(gContent.size(), source.NumCachedChunks());
}

TEST(CachedSourceTests, ReadPartialNeighborChunksRepeated)
{
    if(gContent.size() < 8)
    {
        FAIL();
    }

    auto half = gContent.size() / 2;
    auto quarter = gContent.size() / 4;
    auto eighth = gContent.size() / 8;

    auto buf = std::string(half, '\0');
    auto span = std::span<char>{buf};
    auto source = CachedSource{gDefaultSource, 0, half};

    source.Read(quarter, std::as_writable_bytes(span));
    ASSERT_EQ(gContent.substr(quarter, half), buf);
    ASSERT_EQ(2, source.NumCachedChunks());

    source.Read(eighth, std::as_writable_bytes(span));
    ASSERT_EQ(gContent.substr(eighth, half), buf);
    ASSERT_EQ(2, source.NumCachedChunks());
}

TEST(CachedSourceTests, DiscardsLeastUsed)
{
    using namespace vd::literals;

    auto wrapper = MockSourceWrapper{};
    auto mock = wrapper.impl.get();
    
    ON_CALL(*mock, GetContentLength)
            .WillByDefault(Return(30));
    EXPECT_CALL(*mock, Read(Eq(10),_)).
        Times(Exactly(2));
    EXPECT_CALL(*mock, Read(Eq(0),_)).
        Times(Exactly(1));
    EXPECT_CALL(*mock, Read(Eq(20),_)).
        Times(Exactly(1));

    auto source = CachedSource<MockSourceWrapper>{std::move(wrapper), 2, 10};

    auto arr = MakeArray<1>(1_b);
    auto buf = std::span<std::byte>(arr);
    source.Read(0, buf);
    ASSERT_EQ(1, source.NumCachedChunks());
    source.Read(10, buf);
    ASSERT_EQ(2, source.NumCachedChunks());
    source.Read(0, buf);
    ASSERT_EQ(2, source.NumCachedChunks());
    source.Read(20, buf);
    ASSERT_EQ(2, source.NumCachedChunks());
    source.Read(0, buf);
    ASSERT_EQ(2, source.NumCachedChunks());
    source.Read(10, buf);
    ASSERT_EQ(2, source.NumCachedChunks());
}



TEST(ThreadSafeSourceTests, RaceIfUnguarded)
{
    MockSource mock;
    volatile std::int64_t counter{0};

    auto unsafeIncrement =
        [&counter]()
        {
            auto buf = counter;
            std::this_thread::sleep_for(10ms);
            counter = ++buf;
        };

    ON_CALL(mock, Read)
        .WillByDefault(unsafeIncrement);
    ON_CALL(mock, GetContentLength)
        .WillByDefault(
            [unsafeIncrement]() -> std::size_t
            {
                unsafeIncrement();
                return 0;
            });
    EXPECT_CALL(mock, Read)
        .Times(AnyNumber());
    EXPECT_CALL(mock, GetContentLength)
        .Times(AnyNumber());

    std::thread t1(
        [&mock]()
        {
            for(size_t i = 0; i < 100; ++i)
            {
                mock.Read(0, std::span<std::byte>{});
            };
        });

    std::thread t2(
        [&mock]()
        {
            for(size_t i = 0; i < 100; ++i)
            {
                mock.GetContentLength();
            };
        });

    t1.join();
    t2.join();

    ASSERT_NE(200, counter);
}

TEST(ThreadSafeSourceTests, NoRaceIfGuarded)
{
    auto wrapper = MockSourceWrapper{};
    auto mock = wrapper.impl.get();
    volatile std::int64_t counter{0};

    auto unsafeIncrement =
        [&counter]()
        {
            auto buf = counter;
            std::this_thread::sleep_for(10ms);
            counter = ++buf;
        };

    ON_CALL(*mock, Read)
        .WillByDefault(unsafeIncrement);
    ON_CALL(*mock, GetContentLength)
        .WillByDefault(
            [unsafeIncrement]() -> std::size_t
            {
                unsafeIncrement();
                return 0;
            });
    EXPECT_CALL(*mock, Read)
        .Times(AnyNumber());
    EXPECT_CALL(*mock, GetContentLength)
        .Times(AnyNumber());

    ThreadSafeSource<MockSourceWrapper> source{std::move(wrapper)};

    std::thread t1(
        [&source]()
        {
            for(size_t i = 0; i < 100; ++i)
            {
                source.Read(0, std::span<std::byte>{});
            };
        });

    std::thread t2(
        [&source]()
        {
            for(size_t i = 0; i < 100; ++i)
            {
                source.GetContentLength();
            };
        });

    t1.join();
    t2.join();

    ASSERT_EQ(200, counter);
}



template <typename SourceT>
class SourceTestF : public SourceBaseTestF
{

};

struct HttpSourceWrapper
{
    HttpSource impl = HttpSource{gUrl};

    std::size_t GetContentLength() const noexcept
    {
        return impl.GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        return impl.Read(pos, buf);
    }
};

struct MemoryViewSourceWrapper
{
    static_assert(std::is_same_v<MemoryViewSource, decltype(gDefaultSource)>);

    std::size_t GetContentLength() const noexcept
    {
        return gDefaultSource.GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        return gDefaultSource.Read(pos, buf);
    }
};

struct CachedSourceWrapper
{
    CachedSource<MemoryViewSource> impl{gDefaultSource};

    std::size_t GetContentLength() const noexcept
    {
        return impl.GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        return impl.Read(pos, buf);
    }
};

using SourceTypes = ::testing::Types<HttpSourceWrapper,
                                     MemoryViewSourceWrapper,
                                     CachedSourceWrapper>;
TYPED_TEST_SUITE(SourceTestF, SourceTypes);

TYPED_TEST(SourceTestF, ContentLengthIsAvailableAfterConstruction)
{
    ASSERT_EQ(gContent.size(), TypeParam{}.GetContentLength());
}

TYPED_TEST(SourceTestF, ReadingZero)
{
    ASSERT_NO_THROW(TypeParam{}.Read(0, std::span<std::byte>{(std::byte *)nullptr, 0}));
}

TYPED_TEST(SourceTestF, ReadingNonsenseSpanThrows)
{
    auto src = TypeParam{};
    auto arrExtreme = std::array<std::byte, 2>{};
    auto bufExtreme = std::span(arrExtreme);

    ASSERT_THROW(src.Read(std::numeric_limits<std::size_t>::max(), bufExtreme), RangeError);
    ASSERT_THROW(src.Read(std::numeric_limits<std::ptrdiff_t>::max(), bufExtreme), RangeError);
    ASSERT_THROW(src.Read(std::numeric_limits<std::ptrdiff_t>::max(), bufExtreme), RangeError);

    auto arr = std::array<std::byte, 1>{};
    auto buf = std::span(arr);
    src.Read(gContent.size() - 1u, buf);
    ASSERT_EQ((std::byte)*gContent.crbegin(), *arr.cbegin());
    ASSERT_THROW(src.Read(gContent.size(), buf), RangeError);
}

TYPED_TEST(SourceTestF, ReadingFull)
{
    auto buf = std::string(gContent.size(), '\0');
    TypeParam{}.Read(0, std::as_writable_bytes(std::span<char>{buf}));
    ASSERT_EQ(gContent, buf);
}

}//unnamed namespace