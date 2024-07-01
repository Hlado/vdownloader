#ifndef VDOWNLOADER_VD_UTILS_H_
#define VDOWNLOADER_VD_UTILS_H_

#include "Errors.h"

#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <utility>
#include <string>
#include <type_traits>

namespace vd
{

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

    return ret;
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

}

#endif //VDOWNLOADER_VD_UTILS_H_