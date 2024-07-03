#include <vd/Ap4ByteStream.h>  

using namespace vd;

std::shared_ptr<Ap4HttpByteStream> MakeStream(const std::string &url)
{
    return std::make_shared<Ap4HttpByteStream>(CachedSource{HttpSource{url},2});
}

int main()
{
    auto s = MakeStream(R"(https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_5MB.mp4)");
    AP4_File f{s};
    std::shared_ptr<AP4_ByteStream> ws;
    AP4_FileByteStream::Create("out.mp4", AP4_FileByteStream::STREAM_MODE_WRITE, ws);
    AP4_FileWriter::Write(f, *ws);
    
    return 0;
}