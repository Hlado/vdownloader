#include <vd/Sources.h>
#include <vd/Errors.h>
#include <vd/Preprocessor.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <future>
#include <regex>
#include <thread>


using namespace vd;
using namespace vd::internal;
using namespace vd::literals;
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



TEST(UrlParsingTests, AddressExtraction)
{
    ASSERT_EQ(std::string("http://random.site.com:1234"), ExtractAddress("http://random.site.com:1234"));
    ASSERT_EQ(std::string("http://random.site.com:1234"), ExtractAddress("http://random.site.com:1234/"));
    ASSERT_EQ(std::string("http://random.site.com:1234"), ExtractAddress("http://random.site.com:1234/path"));
}

TEST(UrlParsingTests, PathAndQueryExtraction)
{
    ASSERT_EQ(std::string("/"), ExtractPathAndQuery("http://random.site.com:1234"));
    ASSERT_EQ(std::string("/"), ExtractPathAndQuery("http://random.site.com:1234/"));
    ASSERT_EQ(std::string("/path"), ExtractPathAndQuery("http://random.site.com:1234/path"));
    ASSERT_EQ(std::string("/path?query#hash"), ExtractPathAndQuery("http://random.site.com:1234/path?query#hash"));
}



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



TEST(FileSourceTests, ConstructorThrowsIfFileDoesntExist)
{
    ASSERT_ANY_THROW(FileSource("absurd path to non-existent file"));
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

//Test case based on issue accidentally found by other generic test
TEST(CachedSourceTests, ReadingPastEndOfAlreadyCachedChunk)
{
    auto src = CachedSource{MemoryViewSource{gContentSpan}, 1, gContent.size() + 1};
    std::byte b;
    auto buf = std::span(&b, 1);

    ASSERT_THROW(src.Read(gContent.size(), buf), RangeError);
    src.Read(0, buf);
    ASSERT_THROW(src.Read(gContent.size(), buf), RangeError);
}

TEST(CachedSourceThreadSafetyTests, SimultaneousCachingSingleChunk)
{
    //Basic scenario is that we start two threads, ensure priority for first
    //thread and then check that read have taken place only once and there's
    //only one cached chunk

    auto wrapper = MockSourceWrapper{};
    auto mock = wrapper.impl.get();
    auto src = CachedSource{std::move(wrapper), 0, 1};

    auto mtx = std::mutex{};
    auto cv = std::condition_variable{};
    bool thread1Reading = false;

    auto read =
        [&mtx, &cv, &thread1Reading](auto, auto buf)
        {
            //If we are here, it means that thread 1 has obtained
            //read lock and we can allow thread 2 to read too   
            {
                std::lock_guard lock(mtx);
                thread1Reading = true;
            }
            cv.notify_one();

            //Now best we can do is some reasonable waiting, because
            //there is no way to know that thread 2 has started reading
            std::this_thread::sleep_for(250ms);

            buf[0] = 1_b;
        };

    EXPECT_CALL(*mock, Read)
        .WillOnce(read);
    EXPECT_CALL(*mock, GetContentLength)
        .WillRepeatedly([]() { return gContent.size(); });

    auto t1Byte = std::byte{};
    auto t1Buf = std::span(&t1Byte, 1);
    auto t2Byte = std::byte{};
    auto t2Buf = std::span(&t2Byte, 1);

    std::thread t1(
        [&mtx, &cv, &thread1Reading, &src, t1Buf]()
        {
            src.Read(0, t1Buf);
        });

    std::thread t2(
        [&mtx, &cv, &thread1Reading, &src, t2Buf]()
        {
            {
                std::unique_lock lock(mtx);
                cv.wait(lock, [&thread1Reading]{ return thread1Reading; });
            }

            src.Read(0, t2Buf);
        });

    t1.join();
    t2.join();

    ASSERT_EQ(1_b, t1Byte);
    ASSERT_EQ(1_b, t2Byte);
    ASSERT_EQ(1, src.NumCachedChunks());

    auto controlByte = std::byte{0};
    src.Read(0, std::span(&controlByte, 1));
    ASSERT_EQ(1_b, controlByte);
}

TEST(CachedSourceThreadSafetyTests, CachedReadsDontWait)
{
    auto wrapper = MockSourceWrapper{};
    auto mock = wrapper.impl.get();
    auto src = CachedSource{std::move(wrapper), 0, 1};

    EXPECT_CALL(*mock, Read)
        .WillOnce([](auto, auto buf) { buf[0] = 123_b; });
    EXPECT_CALL(*mock, GetContentLength)
        .WillRepeatedly([]() { return gContent.size(); });

    auto controlByte = std::byte{0};
    src.Read(0, std::span(&controlByte, 1));
    ASSERT_EQ(123_b, controlByte);

    //Plan is to initiate reading other chunk and sleep inside,
    //because it must be protected by global (among all kind of
    //source access) lock and then read few times already cached
    //first chunk
   
    EXPECT_CALL(*mock, Read)
        .WillOnce(
            [](auto, auto buf)
            {
                //We don't want deadlock in case of bugs,
                //so just good old delay
                std::this_thread::sleep_for(250ms);

                buf[0] = 234_b;
            });

    ASSERT_EQ(1, src.NumCachedChunks());

    std::thread t(
        [&src]()
        {
            auto discard = std::byte{0};
            src.Read(1, std::span(&discard, 1));
        });

    //Few cached reads
    for(std::size_t i = 0; i < 10; ++i)
    {
        src.Read(0, std::span(&controlByte, 1));
    }

    //If thread t isn't finished yet, there must be only one cached chunk
    ASSERT_EQ(1, src.NumCachedChunks());

    t.join();

    ASSERT_EQ(2, src.NumCachedChunks());
    src.Read(1, std::span(&controlByte, 1));
    ASSERT_EQ(234_b, controlByte);
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

struct FileSourceWrapper
{
    std::unique_ptr<FileSource> impl;
    std::filesystem::path tmpFilePath;

    FileSourceWrapper()
    {
        //There is some hint about tread safety on cppreference, but
        //tests are running in a single threaded manner, so doesn't matter
        auto nameBuf = std::tmpnam(nullptr);
        if(nameBuf == nullptr)
        {
            throw Error{"failed to generate temporary file name"};
        }
        std::string fileName = nameBuf;
        tmpFilePath = std::filesystem::temp_directory_path() / fileName;

        auto of = std::ofstream(tmpFilePath, std::ios_base::binary | std::ios_base::out);
        if(!of)
        {
            throw Error{"failed to open temporary file for writing"};
        }

        of.write(gContent.data(), gContent.size());
        if(!of)
        {
            throw Error{"failed to write to temporary file"};
        }

        of.close();
        impl.reset(new FileSource(tmpFilePath));
    }

    ~FileSourceWrapper()
    {
        try
        {
            std::filesystem::remove(tmpFilePath);
        } catch(...) {}
    }

    std::size_t GetContentLength() const noexcept
    {
        return impl->GetContentLength();
    }

    void Read(std::size_t pos, std::span<std::byte> buf)
    {
        return impl->Read(pos, buf);
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
                                     FileSourceWrapper,
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

TYPED_TEST(SourceTestF, RandomAccessReads)
{
    if(gContent.size() < 8)
    {
        FAIL();
    }

    auto half = gContent.size() / 2;
    auto quarter = gContent.size() / 4;
    auto eighth = gContent.size() / 8;

    auto buf = std::string(half, '\0');
    auto span = std::as_writable_bytes(std::span<char>{buf});
    auto source = CachedSource{gDefaultSource, 0, half};

    source.Read(quarter, span);
    ASSERT_EQ(gContent.substr(quarter, half), buf);

    source.Read(half, span);
    ASSERT_EQ(gContent.substr(half, half), buf);

    source.Read(eighth, span);
    ASSERT_EQ(gContent.substr(eighth, half), buf);
}

}//unnamed namespace