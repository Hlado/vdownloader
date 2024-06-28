#ifndef VDOWNLOADER_VD_ERRORS_H_
#define VDOWNLOADER_VD_ERRORS_H_

#include <format>
#include <stdexcept>
#include <string>

namespace vd {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string &reason) : std::runtime_error(reason) {}
};

class NotInitializedError : public Error {
public:
    NotInitializedError() : Error("entity is not initialized") {}
};

class NullPointerError : public Error {
public:
    NullPointerError() : Error("prevented null pointer dereferencing") {}
    explicit NullPointerError(const std::string &reason) : Error(reason) {}
};

class NotImplementedError : public Error {
public:
    NotImplementedError() : Error("not implemented") {}
    explicit NotImplementedError(const std::string &name)
        : Error(std::format(R"("{}" is not implemented)",name)) {}
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