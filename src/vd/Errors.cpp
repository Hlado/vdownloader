#include "Errors.h"
#include "Utils.h"

#include <format>

namespace vd
{

HttpError::HttpError(int status, std::string_view reason)
    : Error{std::format(R"(http request failed with error "{}" (status {}))",
                        reason,
                        status)},
      status{status}
{

}

HttpError::HttpError(int status, std::string_view reason, std::string_view request)
    : Error{std::format(R"(http request "{}" failed with error "{}" (status {}))",
                        request,
                        reason,
                        status)},
    status{status}
{

}

int HttpError::GetStatus() const noexcept
{
    return status;
}

HttplibError::HttplibError(httplib::Error code)
    : Error{std::format(R"(httplib call failed with error({}))",
                        ToUnderlying(code))},
      code{code}
{

}

httplib::Error HttplibError::GetCode() const noexcept
{
    return code;
}

LibraryCallError::LibraryCallError(std::string name, int code)
    : Error{std::format(R"(call to "{}" failed with error({}))",name,code)},
      name{std::move(name)},
      code{code} {

}

const std::string &LibraryCallError::GetName() const noexcept {
    return name;
}

int LibraryCallError::GetCode() const noexcept {
    return code;
}

} //namespace vd