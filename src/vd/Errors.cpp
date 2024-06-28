#include "Errors.h"

#include <format>

namespace vd {

LibraryCallError::LibraryCallError(std::string name, int code)
    : Error(std::format(R"(call to "{}" failed with error({}))",name,code)),
      name(std::move(name)),
      code(code) {

}

const std::string &LibraryCallError::GetName() const noexcept {
    return name;
}

int LibraryCallError::GetCode() const noexcept {
    return code;
}

} //namespace vd