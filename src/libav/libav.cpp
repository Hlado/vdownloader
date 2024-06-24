#include "libav.h"

#include "errors.h"

#include <cassert>
#include <iostream>
#include <new>

namespace libav {

static void FreeContext(FormatContext::ImplType *ptr) noexcept {
    if(ptr != nullptr) {
        //It's unclear is there any drawbacks of closing stream that hasn't been opened.
        avformat_close_input(&ptr);
    }
}

bool FormatContext::IsInitialized() const noexcept {
    return impl.get() != nullptr;
}

const FormatContext::ImplType *FormatContext::UnsafeGetPtr() const {
    ThrowIfNotInitialized();

    return impl.get();
}

FormatContext::ImplType *FormatContext::UnsafeGetPtr() {
    ThrowIfNotInitialized();

    return impl.get();
}

void FormatContext::Open(const std::string &url) {
    FormatContext::ImplType *ctx = nullptr;
    
    auto ret = avformat_open_input(&ctx,url.c_str(),nullptr,nullptr);
    if(ret != 0) {
        throw LibraryCallError{"avformat_open_input", ret};
    }

    //Swap via tmp object due to strong exception guarantee
    std::shared_ptr<ImplType> tmp{ctx,FreeContext};
    impl.swap(tmp);
}

void FormatContext::ThrowIfNotInitialized() const {
    if(!IsInitialized()) {
        throw NotInitializedError{};
    }
}



FormatContext Open(const std::string &url) {
    FormatContext ret;
    ret.Open(url);
    return ret;
}

} //namespace libav