#include <vd/Sources.h>
#include <vd/Errors.h>

#include <gtest/gtest.h>

#include <future>
#include <regex>
#include <thread>

using namespace vd;

const std::string content = "This is content for testing";
const std::string contentType = "text/plain";
const std::string hostname = "localhost";
const std::string address = "http://" + hostname;
const std::string contentPath = "/content";
const std::string url = address + contentPath;
const std::string rangesContentPath = "/content_ranges";
const std::string urlRanges = address + rangesContentPath;
constexpr int port = 80;

class HttpSourceTestF : public testing::Test
{
protected:
    httplib::Server server;
    std::future<void> future;

    void SetUp()
    {
        using namespace std::chrono_literals;

        future = std::async(
            std::launch::async,
            [this]()
            {
                server.Get(
                    contentPath,
                    [](const httplib::Request &req, httplib::Response &res)
                    {
                        res.set_header("Accept-Ranges", "none");
                        res.set_content(content, contentType);
                    });

                server.Get(
                    rangesContentPath,
                    [](const httplib::Request &req, httplib::Response &res)
                    {
                        res.set_header("Accept-Ranges","bytes");
                        //Server handles ranges on it's own
                        res.set_content(content, contentType);
                    });

                if(!server.listen(hostname, port))
                {
                    throw std::exception{"HTTP server reported error"};
                }
            });


        while(!server.is_running())
        {
            if(future.wait_for(10ms) == std::future_status::ready)
            {
                break;
            }
        }
    }

    void TearDown()
    {
        server.stop();
        future.get();
    }
};



TEST(HttpSourceTests, NonHttpSchemeThrows)
{
    ASSERT_THROW(HttpSource{"ftp://address"}, ArgumentError);
    ASSERT_THROW(HttpSource{"http://address"}, HttplibError);
    ASSERT_THROW(HttpSource{"https://address"}, HttplibError);
}

TEST_F(HttpSourceTestF, ContentLengthIsAvailableAfterConstruction)
{
    ASSERT_EQ(content.size(), HttpSource{url}.GetContentLength());
}

TEST_F(HttpSourceTestF, ReadingZero)
{
    auto numRead = HttpSource{url}.Read(0, std::span<std::byte>{(std::byte *)nullptr, 0});
    ASSERT_EQ(0, numRead);
}

TEST_F(HttpSourceTestF, ReadingNonsenseSpanThrows)
{
    auto src = HttpSource{url};
    auto arr = std::array<std::byte, 1>{};
    auto buf = std::span(arr);

    ASSERT_EQ(buf.size(), src.Read(content.size() - 1, buf));
    ASSERT_EQ((std::byte)*content.crbegin(), *arr.cbegin());
    ASSERT_THROW(src.Read(content.size(),buf), RangeError);
}

TEST_F(HttpSourceTestF, ReadingFull)
{
    auto test = [](const std::string &url)
    {
        auto zero = std::string(content.size(), '\0');
        auto buf = zero;
        auto span = std::span<char>{buf};
        auto numRead = HttpSource{url}.Read(0, std::as_writable_bytes(span));
        ASSERT_EQ(content, buf);
        ASSERT_EQ(content.size(), numRead);
    };

    test(url);
    test(urlRanges);
}

TEST_F(HttpSourceTestF, ReadingPartial)
{
    auto test = [](const std::string &url)
    {
        static const auto len = content.size() / 2;
        auto zero = std::string(content.size(), '\0');
        auto half = zero;
        half.replace(0, len, content, 0, len);

        auto buf = zero;
        auto span = std::span<char>{buf.data(), len};
        auto src = HttpSource{url};
        src.Read(0, std::as_writable_bytes(span));
        ASSERT_EQ(half, buf);
        span = std::span<char>{std::next(buf.data(), len), std::next(buf.data(), content.size())};
        src.Read(len, std::as_writable_bytes(span));
        ASSERT_EQ(content, buf);
    };

    test(url);
    test(urlRanges);
}