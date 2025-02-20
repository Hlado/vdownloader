#include <vd/VideoStream.h>

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using namespace vd;
using namespace vd::internal;
using namespace vd::literals;

namespace
{

const auto gSquaresFilePath = "tests_data/squares.mp4";



class ConvertFrameTestF :
    public testing::TestWithParam<AVPixelFormat>
{

};

TEST_P(ConvertFrameTestF, BackAndForth)
{
    auto frame = std::unique_ptr<AVFrame, LibavFrameDeleter>{av_frame_alloc()};
    ASSERT_TRUE(frame);

    frame->format = AV_PIX_FMT_RGBA;
    frame->width = 1;
    frame->height = 1;
    ASSERT_EQ(0, av_frame_get_buffer(frame.get(), 0));

    frame->data[0][0] = 11;
    frame->data[0][1] = 22;
    frame->data[0][2] = 33;
    frame->data[0][3] = 44;

    auto forth = ConvertFrame(*frame, GetParam());
    auto back = ConvertFrame(*frame, AV_PIX_FMT_RGBA);
    ASSERT_EQ(frame->data[0][0], back->data[0][0]);
    ASSERT_EQ(frame->data[0][1], back->data[0][1]);
    ASSERT_EQ(frame->data[0][2], back->data[0][2]);
    ASSERT_EQ(frame->data[0][3], back->data[0][3]);
}

INSTANTIATE_TEST_SUITE_P(Instance,
                         ConvertFrameTestF,
                         testing::Values(AV_PIX_FMT_ABGR, AV_PIX_FMT_YUV420P));



//For some not yet known reason RGB color in decoded images is different than
// in source files (155->154 and 55->54), attempts to reproduce issue with
//RGB->YUV->RGB conversion have failed
auto gPix1 = std::array{ 255_b, 255_b, 255_b, 255_b };
auto gPix2 = std::array{ 154_b, 154_b, 154_b, 255_b };
auto gPix3 = std::array{ 54_b, 54_b, 54_b, 255_b };

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
        auto image = frames[i].RgbaImage();
        ASSERT_EQ(100, image.height);
        ASSERT_EQ(100, image.width);
        ASSERT_EQ(gPix1[0], image.data[0]);
    }

    for(std::size_t i = 50; i < 99; ++i)
    {
        auto image = frames[i].RgbaImage();
        ASSERT_EQ(100, image.height);
        ASSERT_EQ(100, image.width);
        ASSERT_EQ(gPix2[0], image.data[0]);
    }

    for(std::size_t i = 100; i < 150; ++i)
    {
        auto image = frames[i].RgbaImage();
        ASSERT_EQ(100, image.height);
        ASSERT_EQ(100, image.width);
        ASSERT_EQ(gPix3[0], image.data[0]);
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
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame(5000ms);
    ASSERT_EQ(5000ms, frame.Timestamp());
    ASSERT_EQ(gPix2[0], frame.RgbaImage().data[0]);
}

TEST_F(VideoStreamTestF, SeekBetweenFrames)
{
    auto frame = *stream->NextFrame(9850ms);
    ASSERT_EQ(9800ms, frame.Timestamp());
    ASSERT_EQ(gPix2[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame(9950ms);
    ASSERT_EQ(9900ms, frame.Timestamp());
    ASSERT_EQ(gPix2[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame(10050ms);
    ASSERT_EQ(10000ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);
}

TEST_F(VideoStreamTestF, SeekToEndAndToBeginning)
{
    auto frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame(0s);
    ASSERT_EQ(0s, frame.Timestamp());
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);
}

TEST_F(VideoStreamTestF, RepeatedSeekToSameTimestamp)
{
    //Middle of stream
    auto frame = *stream->NextFrame(2s); 
    ASSERT_EQ(2s, frame.Timestamp());
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);
    frame = *stream->NextFrame(2s);
    ASSERT_EQ(2s, frame.Timestamp());
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame(2050ms);
    ASSERT_EQ(2000ms, frame.Timestamp());
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);
    frame = *stream->NextFrame(2050ms);
    ASSERT_EQ(2000ms, frame.Timestamp());
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);

    //End of stream
    frame = *stream->NextFrame(14900ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);
    frame = *stream->NextFrame(14900ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame(14950ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);
    frame = *stream->NextFrame(14950ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);
    frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gPix3[0], frame.RgbaImage().data[0]);
}

TEST_F(VideoStreamTestF, SeekingAfterNormalReading)
{
    for(std::size_t i = 0; i < 10; ++i)
    {
        stream->NextFrame();
    }

    auto frame = *stream->NextFrame(5s);
    ASSERT_EQ(5s, frame.Timestamp());
    ASSERT_EQ(gPix2[0], frame.RgbaImage().data[0]);
}

TEST_F(VideoStreamTestF, NormalReadingAfterSeeking)
{
    auto frame = *stream->NextFrame(4850ms);
    ASSERT_EQ(4800ms, frame.Timestamp());
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame();
    ASSERT_EQ(4900ms, frame.Timestamp());
    ASSERT_EQ(gPix1[0], frame.RgbaImage().data[0]);

    frame = *stream->NextFrame();
    ASSERT_EQ(5s, frame.Timestamp());
    ASSERT_EQ(gPix2[0], frame.RgbaImage().data[0]);
}

}//unnamed namespace