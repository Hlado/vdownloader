#include <vd/Ap4ByteStream.h>  
#include <vd/Mp4Container.h>

using namespace vd;

std::shared_ptr<Ap4HttpByteStream> MakeStream(const std::string &url)
{
    return std::make_shared<Ap4HttpByteStream>(CachedSource{HttpSource{url},2,1 << 14});
}

std::shared_ptr<AP4_ByteStream> MakeFileStream(const std::string &path)
{
    std::shared_ptr<AP4_ByteStream> ret;
    AP4_FileByteStream::Create(path.c_str(), AP4_FileByteStream::STREAM_MODE_READ, ret);
    return ret;
}

template <typename T>
void discard(const T &)
{

}

int main(int argc, char *argv[])
{
    try
    {
        if(argc != 2)
        {
            Println("Usage: vdownloader <path-to-mp4>");
        }

        auto s = MakeFileStream(argv[1]);

        using namespace std::chrono_literals;

        Mp4Container c(s);
        auto t = c.GetTrack();
        auto segs = c.GetTrack().Slice(t.GetStart(),t.GetFinish());
    }
    catch(const std::exception &e)
    {
        Println(e.what());
    }

    return 0;
}