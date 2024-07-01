#ifndef VDOWNLOADER_VD_ERRORS_H_
#define VDOWNLOADER_VD_ERRORS_H_

#include <httplib.h>

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

namespace vd {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string &reason) : std::runtime_error{reason} {}
};

class NullPointerError : public Error {
public:
    NullPointerError() : Error{"null pointer error"} {}
    explicit NullPointerError(const std::string &reason) : Error{reason} {}
};

class NotSupportedError : public Error {
public:
    NotSupportedError() : Error{"not supported error"} {}
    explicit NotSupportedError(const std::string& reason) : Error{reason} {}
};


class RangeError : public Error {
public:
    RangeError() : Error{"out of range error"} {}
    explicit RangeError(const std::string& reason)
        : Error{reason} {}
    RangeError(std::string_view reason,
               std::string_view name)
        : Error{std::format(R"("{}" {})", name, reason)} {}
};

class ArgumentError : public Error {
public:
    ArgumentError() : Error{"argument error"} {}
    explicit ArgumentError(const std::string &reason)
        : Error{reason} {}
    ArgumentError(std::string_view reason,
                  std::string_view name)
        : Error{std::format(R"("{}" {})", name, reason)} {}
};

class NotInitializedError : public Error {
public:
    NotInitializedError() : Error{"entity is not initialized"} {}
    explicit NotInitializedError(std::string_view name)
        : Error{std::format(R"("{}" is not initialized)",name)} {}
};

class NotFoundError : public Error {
public:
    NotFoundError() : Error{"entity is not found"} {}
    explicit NotFoundError(std::string_view name)
        : Error{std::format(R"("{}" is not found)", name)} {}
};

class NotImplementedError : public Error {
public:
    NotImplementedError() : Error{"entity is not implemented"} {}
    explicit NotImplementedError(std::string_view name)
        : Error{std::format(R"("{}" is not implemented)",name)} {}
};

class HttpError : public Error {
public:
    HttpError(int status, std::string_view reason);
    HttpError(int status, std::string_view reason, std::string_view request);

    int GetStatus() const noexcept;

private:
    int status;
};

class HttplibError : public Error {
public:
    explicit HttplibError(httplib::Error code);

    httplib::Error GetCode() const noexcept;

private:
    httplib::Error code;
};


class LibraryCallError : public Error {
public:
    LibraryCallError(std::string name, int code);

    const std::string &GetName() const noexcept;
    int GetCode() const noexcept;

private:
    std::string name;
    int code;
};

} //namespace vd


#endif //VDOWNLOADER_VD_ERRORS_H_