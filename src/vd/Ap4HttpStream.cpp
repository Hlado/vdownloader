#include "Ap4HttpStream.h"

#include "Errors.h"
#include "Preprocessor.h"

namespace vd {

AP4_Result Ap4HttpByteStream::ReadPartial(void* buffer, AP4_Size bytes_to_read, AP4_Size& bytes_read)
{
    return AP4_Result();
}

AP4_Result Ap4HttpByteStream::WritePartial(const void* buffer, AP4_Size bytes_to_write, AP4_Size& bytes_written)
{
    return AP4_Result();
}

AP4_Result Ap4HttpByteStream::Seek(AP4_Position position)
{
    return AP4_Result();
}

AP4_Result Ap4HttpByteStream::Tell(AP4_Position& position)
{
    return AP4_Result();
}

AP4_Result Ap4HttpByteStream::GetSize(AP4_LargeSize& size)
{
    return AP4_Result();
}

/*
static void FreeContext(FormatContext::ImplType *ptr) noexcept {
    if(ptr != nullptr) {
        //It's unclear is there any drawbacks of closing stream that hasn't been opened.
        avformat_close_input(&ptr);
    }
}

// Only purpose of implementing this instead of leaving default is to guarantee that
// different not initialized objects are not equal
bool FormatContext::operator==(const FormatContext &other) const noexcept {
    return this == &other;
}

bool FormatContext::IsInitialized() const noexcept {
    return impl.get() != nullptr;
}

std::string FormatContext::GetName() const {
    AssertInitialized();

    if (impl->iformat == nullptr) {
        return "";
    }
    
    return impl->iformat->name != nullptr ? impl->iformat->name : "";
}

std::string FormatContext::GetLongName() const {
    AssertInitialized();

    if (impl->iformat == nullptr) {
        return "";
    }

    return impl->iformat->long_name != nullptr ? impl->iformat->long_name : "";
}

Duration FormatContext::GetDuration() const {
    AssertInitialized();

    return impl->duration <= 0 ? Duration::zero() : Duration(impl->duration);
}

const FormatContext::ImplType *FormatContext::UnsafeGetPtr() const {
    AssertInitialized();

    return impl.get();
}

FormatContext::ImplType *FormatContext::UnsafeGetPtr() {
    AssertInitialized();

    return impl.get();
}

void FormatContext::Open(const std::string &url) {
    FormatContext::ImplType *ctx = nullptr;
    
    int err = avformat_open_input(&ctx,url.c_str(),nullptr,nullptr);
    if(err != 0) {
        throw LibraryCallError{"avformat_open_input", err };
    }
    
    //Swap via tmp object to provide strong exception guarantee
    std::shared_ptr<ImplType> tmp{ctx,FreeContext};

    err = avformat_find_stream_info(tmp.get(), nullptr);
    //Yes, it's weird that for different functions successful return codes aren't the same,
    if (err < 0) {
        throw LibraryCallError{ "avformat_find_stream_info", err };
    }

    impl.swap(tmp);
}

void FormatContext::Close() noexcept {
    impl.reset();
}

void FormatContext::AssertInitialized() const {
    if(!IsInitialized()) {
        throw NotInitializedError{};
    }
}



FormatContext Open(const std::string &url) {
    auto ret = FormatContext{};
    ret.Open(url);
    return ret;
}
*/

} //namespace vd