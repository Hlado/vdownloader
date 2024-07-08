#ifndef VDOWNLOADER_VD_UTILS_H_
#define VDOWNLOADER_VD_UTILS_H_

#include "Errors.h"

#include <bit>
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <utility>
#include <string>
#include <type_traits>

namespace vd
{

const std::string gEol{"\n"};



template <typename T>
constexpr std::underlying_type_t<T> ToUnderlying(const T &val)
{
    using UnderlyingType = std::underlying_type_t<T>;
    return static_cast<UnderlyingType>(val);
}

template <std::unsigned_integral T>
T StrToUint(const std::string &str)
{
    auto ret = std::stoull(str);
    if(ret > std::numeric_limits<T>::max())
    {
        throw RangeError{};
    }

    return static_cast<T>(ret);
}



template<std::unsigned_integral ToT, std::unsigned_integral FromT>
constexpr ToT UintCast(FromT val)
{
    if constexpr(std::numeric_limits<ToT>::max() >= std::numeric_limits<FromT>::max())
    {
        return val;
    }
    else
    {
        if(std::numeric_limits<ToT>::max() < static_cast<std::uintmax_t>(val))
        {
            throw RangeError{};
        }

        return static_cast<ToT>(val);
    }
}

template<std::unsigned_integral T>
constexpr bool UintOverflow(T val, T add)
{
    return std::numeric_limits<T>::max() - val < add;
}



//If return value is empty pointer then no modifications are made on base
template <class DerivedT, class BaseT>
    requires std::derived_from<DerivedT, BaseT>
std::unique_ptr<DerivedT> DynamicUniqueCast(std::unique_ptr<BaseT> &&base)
{
    auto pointer = dynamic_cast<DerivedT *>(base.get());

    std::unique_ptr<DerivedT> ret;
    if(pointer != nullptr)
    {
        ret.reset(pointer);
        base.release();
    }

    return ret;
}



namespace internal
{

//https://en.cppreference.com/w/cpp/numeric/byteswap
template<std::integral T>
constexpr T ByteSwap(T val) noexcept
{
    static_assert(std::has_unique_object_representations_v<T>,
                  "T may not have padding bits");

    auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(val);
    std::ranges::reverse(bytes);
    return std::bit_cast<T>(bytes);
}

} //namespace internal

template<std::integral T>
constexpr T ByteSwap(T val) noexcept
{
#ifdef __cpp_lib_byteswap
    return std::byteswap(val);
#else
    return internal::ByteSwap(val);
#endif
}

template<std::integral T>
constexpr void ByteSwapInplace(T &val) noexcept
{
    val = ByteSwap(val);
}



template <typename CharT,
          typename... Args,
          class Traits = std::char_traits<CharT>>
void PrintfTo(std::basic_ostream<CharT, Traits> &os,
              std::string_view fstr,
              Args &&... args)
{
    os << std::vformat(fstr, std::make_format_args(args...)) << std::flush;
}

template <typename... Args>
void Printf(const std::format_string<Args...> fstr,
            Args &&... args)
{
    PrintfTo(std::cout, fstr.get(), std::forward<Args>(args)...);
}

template <typename T>
void Println(const T &val)
{
    std::cout << val << std::endl;
}

template <typename... Args>
void Errorf(const std::format_string<Args...> fstr,
            Args &&... args)
{
    PrintfTo(std::cerr, fstr.get(), std::forward<Args>(args)...);
}

template <typename T>
void Errorln(const T &val)
{
    std::cerr << val << std::endl;
}



template <std::size_t Size, typename T>
std::array<T, Size> MakeArray(const T &val)
{
    std::array<T, Size> ret;
    std::fill(ret.begin(), ret.end(), val);
    return ret;
}



template <std::invocable T>
struct Defer final
{
    Defer(T &&invocable)
        : mInvocable(std::move(invocable))
    {
    
    }

    Defer(const Defer<T> &) = delete;
    Defer<T> &operator=(const Defer<T> &) = delete;
    Defer(Defer<T> &&) = delete;
    Defer<T> &operator=(Defer<T> &&) = delete;

    ~Defer()
    {
        try
        {
            std::invoke(mInvocable);
        }
        catch(...) {}
    }

private:
    T mInvocable;
};

}

#endif //VDOWNLOADER_VD_UTILS_H_