#ifndef VDOWNLOADER_VD_A4_HTTP_STREAM_H_
#define VDOWNLOADER_VD_A4_HTTP_STREAM_H_

#include <Ap4FileByteStream.h>

namespace vd {

class Ap4HttpByteStream : public AP4_ByteStream
{
public:
    AP4_Result ReadPartial(void* buffer, AP4_Size bytes_to_read, AP4_Size &bytes_read) override;
    AP4_Result WritePartial(const void* buffer, AP4_Size bytes_to_write, AP4_Size &bytes_written) override;
    AP4_Result Seek(AP4_Position position) override;
    AP4_Result Tell(AP4_Position &position) override;
    AP4_Result GetSize(AP4_LargeSize &size) override;
};

/*
using Duration = std::chrono::duration<int64_t, std::ratio<1, AV_TIME_BASE>>;

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

    ///
    /// Not initialized objects aren't equal (except when it's the same objects),
    /// different objects are never equal
    ///
    bool operator==(const FormatContext &other) const noexcept;

    bool IsInitialized() const noexcept;
    std::string GetName() const;
    std::string GetLongName() const;
    Duration GetDuration() const;
    const ImplType *UnsafeGetPtr() const;
    ImplType *UnsafeGetPtr();

    ///
    /// avformat_open_input wrapper. In case of exception no changes are made.
    ///
    void Open(const std::string &url);

    ///
    /// avformat_close_input wrapper.
    ///
    void Close() noexcept;

private:
    std::shared_ptr<ImplType> impl;

    void AssertInitialized() const;
    //Every other Assert... function must do initialization check
};

static_assert(std::is_nothrow_move_assignable<FormatContext>::value);
static_assert(std::is_nothrow_move_constructible<FormatContext>::value);



///
/// avformat_open_input wrapper
/// 
FormatContext Open(const std::string &url);
*/

} //namespace vd

#endif //VDOWNLOADER_VD_A4_HTTP_STREAM_H_