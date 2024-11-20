#include <vd/Ap4ByteStream.h>  
#include <vd/Mp4Container.h>
#include <vd/Decoder.h>
#include <vd/OpenH264Decoder.h>
#include <vd/Options.h>
#include <vd/Tga.h>

#include <libyuv.h>

using namespace vd;

namespace
{

std::shared_ptr<Ap4HttpByteStream> MakeStream(const std::string &url)
{
    return std::make_shared<Ap4HttpByteStream>(CachedSource{HttpSource{url},2,2000000});
}

std::shared_ptr<AP4_ByteStream> MakeFileStream(const std::string &path)
{
    std::shared_ptr<AP4_ByteStream> ret;
    AP4_FileByteStream::Create(path.c_str(), AP4_FileByteStream::STREAM_MODE_READ, ret);
    return ret;
}

}



int main(int argc, char *argv[])
{
    try
    {
        auto options = ParseOptions(argc, argv);
        if(!options)
        {
            return 0;
        }

        auto s = MakeFileStream(options->videoUrl);
        if(!s)
        {
            if(s = MakeStream(options->videoUrl); !s)
            {
                throw Error(std::format(R"(Unable to open video source "{}")", options->videoUrl));
            }
        }

        using namespace std::chrono_literals;

        Mp4Container c(s);
        auto t = c.GetTrack();
        auto segs = c.GetTrack().Slice(900s,910s);
        auto d = Decoder{t.GetDecodingConfig(), t.Slice(0s, 1s).front()};

        int n = 0;
        for(auto f = d.GetNext(); f.has_value(); f = d.GetNext())
        {
            {
                std::ofstream fs(std::format("out{}.tga", n), std::ios::binary | std::ios_base::out);
                WriteTga(fs, f->image.data, f->image.width);
            }

            std::cout << "frame " << n++ << std::endl;
            std::cout << d.TimestampLast() << "ns" << std::endl;
        }

        return 0;
    }
    catch(const std::exception &e)
    {
        Errorln(e.what());
        return 1;
    }
}