#include "errors.h"

extern "C" {

#include "libavutil/error.h"

}

#include <format>

namespace libav {

static std::string CodeToStr(int code) {
    //128 characters looks reasonable, at first glance longest error str is around ~60 characters
    char buf[128];
    buf[0] = 0; //just in case

    if(av_strerror(code,buf,sizeof(buf)) != 0) {
        return "";        
    }

    return buf;
}

LibraryCallError::LibraryCallError(std::string name, int code)
    : Error(std::format(R"(call to "{}" failed with error({}) "{}")",name,code,CodeToStr(code))),
      name(std::move(name)),
      code(code) {

}

const std::string &LibraryCallError::GetName() const noexcept {
    return name;
}

int LibraryCallError::GetCode() const noexcept {
    return code;
}

} //namespace libav