#ifndef LIBAV_LIBAV_H_
#define LIBAV_LIBAV_H_

extern "C" {

#include "libavformat/avformat.h"

}

#include <memory>
#include <string>
#include <type_traits>

namespace libav {

class FormatContext final {
public:
    using ImplType = ::AVFormatContext;

    ///
    /// Creates uninitialized context. Any action on it except FormatContext::Open(),
    /// FormatContext::Close() or FormatContext::IsInitialized() const will throw
    /// NotInitializedError.
    ///
    FormatContext() = default;
    FormatContext(const FormatContext &other) = delete;
    FormatContext &operator=(const FormatContext &other) = delete;
    FormatContext(FormatContext &&other) = default;
    FormatContext &operator=(FormatContext &&other) = default;
    ~FormatContext() noexcept = default;

    bool IsInitialized() const noexcept;
    const ImplType *UnsafeGetPtr() const;
    ImplType *UnsafeGetPtr();

    ///
    /// avformat_open_input wrapper. In case of exception no changes are made.
    ///
    void Open(const std::string &url);

private:
    std::shared_ptr<ImplType> impl;

    void ThrowIfNotInitialized() const;
};

static_assert(std::is_nothrow_move_assignable<FormatContext>::value);
static_assert(std::is_nothrow_move_constructible<FormatContext>::value);



///
/// avformat_open_input wrapper
/// 
FormatContext Open(const std::string &url);

} //namespace libav


#endif //LIBAV_LIBAV_H_