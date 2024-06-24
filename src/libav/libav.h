#ifndef LIBAV_LIBAV_H_
#define LIBAV_LIBAV_H_

extern "C" {

#include "libavformat/avformat.h"

}

#include <memory>
#include <string>

namespace libav {

class AvFormatContext final {
public:
    using ImplType = ::AVFormatContext;

    ///
    /// Creates uninitialized context. Any action on it except AvFormatContext::Open(),
    /// AvFormatContext::Close() or AvFormatContext::IsInitialized() const will throw
    /// NotInitializedError.
    ///
    AvFormatContext() = default;
    AvFormatContext(const AvFormatContext &other) = delete;
    AvFormatContext &operator=(const AvFormatContext &other) = delete;
    AvFormatContext(AvFormatContext &&other) noexcept = default;
    AvFormatContext &operator=(AvFormatContext &&other) noexcept = default;
    ~AvFormatContext() noexcept = default;

    bool IsInitialized() const noexcept;
    const ImplType *UnsafeGetPtr() const noexcept;
    ImplType *UnsafeGetPtr() noexcept;

    ///
    /// avformat_open_input wrapper. In case of exception no changes are made.
    ///
    void Open(const std::string &url);

private:
    std::shared_ptr<ImplType> impl;

    void ThrowIfNotInitialized() const;
};



///
/// avformat_open_input wrapper
/// 
AvFormatContext Open(const std::string &url);

} //namespace libav


#endif //LIBAV_LIBAV_H_