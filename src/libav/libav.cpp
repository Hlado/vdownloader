#include "libav.h"

#include "errors.h"

#include <cassert>
#include <iostream>
#include <new>

namespace libav {

static void FreeContext(AvFormatContext::ImplType *ptr) noexcept {
    if(ptr != nullptr) {
        //It's unclear is there any drawbacks of closing stream that hasn't been opened.
        avformat_close_input(&ptr);
    }
}

bool AvFormatContext::IsInitialized() const noexcept {
    return impl.get() != nullptr;
}

const AvFormatContext::ImplType *AvFormatContext::UnsafeGetPtr() const noexcept {
    ThrowIfNotInitialized();

    return impl.get();
}

AvFormatContext::ImplType *AvFormatContext::UnsafeGetPtr() noexcept {
    ThrowIfNotInitialized();

    return impl.get();
}

void AvFormatContext::Open(const std::string &url) {
    AvFormatContext::ImplType *ctx = nullptr;
    
    auto ret = avformat_open_input(&ctx,url.c_str(),nullptr,nullptr);
    if(ret != 0) {
        throw LibraryCallError{"avformat_open_input", ret};
    }

    //Swap via tmp object due to strong exception guarantee
    std::shared_ptr<ImplType> tmp{ctx,FreeContext};
    impl.swap(tmp);
}

void AvFormatContext::ThrowIfNotInitialized() const {
    if(!IsInitialized()) {
        throw NotInitializedError{};
    }
}



AvFormatContext Open(const std::string &url) {
    AvFormatContext ret;
    ret.Open(url);
    return ret;
}

} //namespace libav