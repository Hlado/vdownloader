#include <vd/VideoStream.h>

#include <gtest/gtest.h>

using namespace std::chrono_literals;
using namespace vd;
using namespace vd::internal;
using namespace vd::libav;
using namespace vd::literals;

namespace
{

const auto gSquaresFilePath = "tests_data/squares.mp4";



Rgb32Image FilledRgbaImage(std::size_t width, std::size_t height, std::array<std::uint8_t, 4> val)
{
    Rgb32Image res{width, height};

    auto ptr = res.Data().data();
    for(std::size_t i = 0; i < width*height; ++i)
    {
        std::memcpy(ptr, val.data(), 4);
        ptr += 4;
    }

    return res;
}

UniquePtr<AVFrame> RgbaToFrame(const Rgb32Image &rgbaImg)
{
    auto res = MakeFrame();
    res->format = AV_PIX_FMT_RGBA;
    res->width = IntCast<int>(rgbaImg.Width());
    res->height = IntCast<int>(rgbaImg.Height());
    if(auto err = av_frame_get_buffer(res.get(), 0); err != 0)
    {
        throw LibraryCallError{"av_frame_get_buffer", err};
    }

    if(std::cmp_less(res->linesize[0], rgbaImg.RowSize()))
    {
        throw Error{"linesize miscalculation"};
    }

    av_image_copy_plane(res->data[0],
                        res->linesize[0],
                        rgbaImg.Data().data(),
                        IntCast<int>(rgbaImg.RowSize()),
                        IntCast<int>(rgbaImg.RowSize()),
                        IntCast<int>(rgbaImg.Height()));

    return res;
}

class ConvertFrameTestF :
    public testing::TestWithParam<AVPixelFormat>
{

};

TEST_P(ConvertFrameTestF, BackAndForth)
{
    auto frame = RgbaToFrame(FilledRgbaImage(25, 25, {123, 123, 123, 255}));

    auto forth = ConvertFrame(*frame, GetParam());
    auto back = ConvertFrame(*forth, AV_PIX_FMT_RGBA);

    ASSERT_EQ(frame->buf[0]->size, back->buf[0]->size);
    ASSERT_EQ(frame->linesize[0], back->linesize[0]);
    for(int i = 0; i < frame->height; ++i)
    {
        auto off = frame->linesize[0] * i;
        ASSERT_EQ(0, std::memcmp(frame->buf[0]->data + off,
                                 back->buf[0]->data + off,
                                 frame->width*4));
    }
}

INSTANTIATE_TEST_SUITE_P(Instance,
                         ConvertFrameTestF,
                         testing::Values(AV_PIX_FMT_ABGR, AV_PIX_FMT_YUV420P));



//For some not yet known reason RGB color in decoded images is different than
//in source files (155->154 and 55->54), attempts to reproduce issue with
//RGB->YUV->RGB conversion have failed
const auto gRgbaImg1 = FilledRgbaImage(100, 100, {255, 255, 255, 255});
const auto gRgbaImg2 = FilledRgbaImage(100, 100, {154, 154, 154, 255});
const auto gRgbaImg3 = FilledRgbaImage(100, 100, {54, 54, 54, 255});

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
                [&source]() { return std::make_unique<Reader>(source); });
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
        ASSERT_EQ(gRgbaImg1, image);
    }

    for(std::size_t i = 50; i < 99; ++i)
    {
        auto image = frames[i].RgbaImage();
        ASSERT_EQ(gRgbaImg2, image);
    }

    for(std::size_t i = 100; i < 150; ++i)
    {
        auto image = frames[i].RgbaImage();
        ASSERT_EQ(gRgbaImg3, image);
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
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());

    frame = *stream->NextFrame(5000ms);
    ASSERT_EQ(5000ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg2, frame.RgbaImage());
}

TEST_F(VideoStreamTestF, SeekBetweenFrames)
{
    auto frame = *stream->NextFrame(9850ms);
    ASSERT_EQ(9800ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg2, frame.RgbaImage());

    frame = *stream->NextFrame(9950ms);
    ASSERT_EQ(9900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg2, frame.RgbaImage());

    frame = *stream->NextFrame(10050ms);
    ASSERT_EQ(10000ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());
}

TEST_F(VideoStreamTestF, SeekToEndAndToBeginning)
{
    auto frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());

    frame = *stream->NextFrame(0s);
    ASSERT_EQ(0s, frame.Timestamp());
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());
}

TEST_F(VideoStreamTestF, RepeatedSeekToSameTimestamp)
{
    //Middle of stream
    auto frame = *stream->NextFrame(2s); 
    ASSERT_EQ(2s, frame.Timestamp());
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());
    frame = *stream->NextFrame(2s);
    ASSERT_EQ(2s, frame.Timestamp());
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());

    frame = *stream->NextFrame(2050ms);
    ASSERT_EQ(2000ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());
    frame = *stream->NextFrame(2050ms);
    ASSERT_EQ(2000ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());

    //End of stream
    frame = *stream->NextFrame(14900ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());
    frame = *stream->NextFrame(14900ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());

    frame = *stream->NextFrame(14950ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());
    frame = *stream->NextFrame(14950ms);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());

    frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());
    frame = *stream->NextFrame(15s);
    ASSERT_EQ(14900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg3, frame.RgbaImage());
}

TEST_F(VideoStreamTestF, SeekingAfterNormalReading)
{
    for(std::size_t i = 0; i < 10; ++i)
    {
        stream->NextFrame();
    }

    auto frame = *stream->NextFrame(5s);
    ASSERT_EQ(5s, frame.Timestamp());
    ASSERT_EQ(gRgbaImg2, frame.RgbaImage());
}

TEST_F(VideoStreamTestF, NormalReadingAfterSeeking)
{
    auto frame = *stream->NextFrame(4850ms);
    ASSERT_EQ(4800ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());

    frame = *stream->NextFrame();
    ASSERT_EQ(4900ms, frame.Timestamp());
    ASSERT_EQ(gRgbaImg1, frame.RgbaImage());

    frame = *stream->NextFrame();
    ASSERT_EQ(5s, frame.Timestamp());
    ASSERT_EQ(gRgbaImg2, frame.RgbaImage());
}

}//unnamed namespace