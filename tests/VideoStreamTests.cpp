#include <vd/VideoStream.h>

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using namespace vd;
using namespace vd::literals;

namespace
{

const auto gSquaresFilePath = "tests_data/squares.mp4";



class VideoStreamTestF : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto source =
            std::shared_ptr<SourceBase>{
                new Source{FileSource{gSquaresFilePath}}};
        stream =
            OpenMediaSource(
                [&source]() { return std::make_unique<LibavReader>(source); });
    }

    std::optional<VideoStream> stream;    
};

TEST_F(VideoStreamTestF, FullReadFrameByFrame)
{
    std::vector<Frame> frames;
    auto frame = stream->NextFrame();
    while(frame)
    {
        frames.push_back(std::move(*frame));
        frame = stream->NextFrame();
    }

    ASSERT_EQ(150, frames.size());

    for(std::size_t i = 0; i < 50; ++i)
    {
        auto image = frames[i].Image();
        ASSERT_EQ(100, image.height);
        ASSERT_EQ(100, image.width);
        ASSERT_EQ(255_b, image.data[0]);
    }

    for(std::size_t i = 50; i < 99; ++i)
    {
        auto image = frames[i].Image();
        ASSERT_EQ(100, image.height);
        ASSERT_EQ(100, image.width);
        ASSERT_EQ(155_b, image.data[0]);
    }

    for(std::size_t i = 100; i < 150; ++i)
    {
        auto image = frames[i].Image();
        ASSERT_EQ(100, image.height);
        ASSERT_EQ(100, image.width);
        ASSERT_EQ(55_b, image.data[0]);
    }
}

TEST_F(VideoStreamTestF, SeekPastTheEndThrows)
{
    ASSERT_THROW(stream->NextFrame(15001ms), RangeError);
}

TEST_F(VideoStreamTestF, SeekExactlyToFrameStart)
{
    auto frame = *stream->NextFrame(4900ms);
    ASSERT_EQ(4900ms, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);

    frame = *stream->NextFrame(5000ms);
    ASSERT_EQ(5000ms, frame.Timestamp());
    ASSERT_EQ(155_b, frame.Image().data[0]);
}

TEST_F(VideoStreamTestF, SeekBetweenFrames)
{
    auto frame = *stream->NextFrame(9850ms);
    ASSERT_EQ(9800ms, frame.Timestamp());
    ASSERT_EQ(155_b, frame.Image().data[0]);

    frame = *stream->NextFrame(9950ms);
    ASSERT_EQ(9900ms, frame.Timestamp());
    ASSERT_EQ(155_b, frame.Image().data[0]);

    frame = *stream->NextFrame(10050ms);
    ASSERT_EQ(10000ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);
}

TEST_F(VideoStreamTestF, SeekToEndAndToBeginning)
{
    auto frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);

    frame = *stream->NextFrame(0s);
    ASSERT_EQ(0s, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);
}

TEST_F(VideoStreamTestF, RepeatedSeekToSameTimestamp)
{
    //Middle of stream
    auto frame = *stream->NextFrame(2s); 
    ASSERT_EQ(2s, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);
    frame = *stream->NextFrame(2s);
    ASSERT_EQ(2s, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);

    frame = *stream->NextFrame(2050ms);
    ASSERT_EQ(2000ms, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);
    frame = *stream->NextFrame(2050ms);
    ASSERT_EQ(2000ms, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);

    //End of stream
    frame = *stream->NextFrame(14900ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);
    frame = *stream->NextFrame(14900ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);

    frame = *stream->NextFrame(14950ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);
    frame = *stream->NextFrame(14950ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);

    frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);
    frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(55_b, frame.Image().data[0]);
}

TEST_F(VideoStreamTestF, SeekingAfterNormalReading)
{
    for(std::size_t i = 0; i < 10; ++i)
    {
        stream->NextFrame();
    }

    auto frame = *stream->NextFrame(5s);
    ASSERT_EQ(5s, frame.Timestamp());
    ASSERT_EQ(155_b, frame.Image().data[0]);
}

TEST_F(VideoStreamTestF, NormalReadingAfterSeeking)
{
    auto frame = *stream->NextFrame(4850ms);
    ASSERT_EQ(4800ms, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);

    frame = *stream->NextFrame();
    ASSERT_EQ(4900ms, frame.Timestamp());
    ASSERT_EQ(255_b, frame.Image().data[0]);

    frame = *stream->NextFrame();
    ASSERT_EQ(5s, frame.Timestamp());
    ASSERT_EQ(155_b, frame.Image().data[0]);
}

}//unnamed namespace