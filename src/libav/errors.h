#ifndef LIBAV_ERRORS_H_
#define LIBAV_ERRORS_H_

#include <stdexcept>
#include <string>

namespace libav {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string &reason) : std::runtime_error(reason) {}
};

class NotInitializedError : public Error {
public:
    NotInitializedError() : Error("entity is not initialized") {}
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

} //namespace libav


#endif //LIBAV_ERRORS_H_