#ifndef VDOWNLOADER_VD_UTILS_H_
#define VDOWNLOADER_VD_UTILS_H_

#include "Errors.h"

#include <bit>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <string>
#include <type_traits>

namespace vd
{

const std::string gEol{"\n"};



template <typename T>
using Lateinit = std::optional<T>;

template <typename T>
concept StdDurationConcept = requires {
    []<class Rep, class Period>(std::type_identity<std::chrono::duration<Rep, Period>>){}(
        std::type_identity<T>{});
};



namespace literals
{

consteval std::byte operator ""_b(unsigned long long int val)
{
    if(val > 255) throw;

    return static_cast<std::byte>(val);
}

}//namespace literals



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

template <typename T>
    requires std::is_arithmetic_v<T> &&
             (!std::is_same_v<T,bool>)
T ReadBuffer(std::span<const std::byte> buf)
{
    if(buf.size_bytes() < sizeof(T))
    {
        throw RangeError{};
    }

    T ret;
    std::memcpy(&ret, buf.data(), sizeof ret);
    return ret;
}



template<std::unsigned_integral ToT, std::unsigned_integral FromT>
constexpr bool UintOverflow(FromT val)
{
    if constexpr(
        std::cmp_greater_equal(std::numeric_limits<ToT>::max(),
                               std::numeric_limits<FromT>::max()))
    {
        return false;
    }
    else
    {
        return std::cmp_greater(val, std::numeric_limits<ToT>::max());
    }
}



template<std::unsigned_integral RetT,
         std::unsigned_integral T,
         std::unsigned_integral U>
constexpr RetT Add(bool &overflow, T l, U r)
{
    using ExprT = std::common_type_t<RetT, std::make_unsigned_t<decltype(l + r)>>;
    auto ret = static_cast<ExprT>(l) + r;

    //ret is always unsigned, even in case of integer promotion of smaller types to signed int
    //but to be completely sure...
    static_assert(std::is_unsigned_v<decltype(ret)>);

    overflow = ret < l || UintOverflow<RetT>(ret);
    return static_cast<RetT>(ret);
}

template<std::unsigned_integral RetT,
         std::unsigned_integral T,
         std::unsigned_integral U>
constexpr RetT Sub(bool &overflow, T l, U r)
{
    using ExprT = std::common_type_t<RetT, std::make_unsigned_t<decltype(l - r)>>;
    auto ret = static_cast<ExprT>(l) - r;
    overflow = ret > l || UintOverflow<RetT>(ret);
    return static_cast<RetT>(ret);
}


template<std::unsigned_integral RetT,
         std::unsigned_integral T,
         std::unsigned_integral U>
constexpr RetT Mul(bool &overflow, T l, U r)
{
    using ExprT = std::common_type_t<RetT, std::make_unsigned_t<decltype(l * r)>>;
    constexpr auto max = std::numeric_limits<ExprT>::max();

    auto ret = static_cast<ExprT>(l) * r;
    overflow = max / r < l || UintOverflow<RetT>(ret);
    return static_cast<RetT>(ret);
}



namespace internal
{

struct AddFunctor
{
    template<std::unsigned_integral RetT,
             std::unsigned_integral T,
             std::unsigned_integral U>
    constexpr RetT operator()(bool &overflow, T l, U r) const;
};

template<std::unsigned_integral RetT,
         std::unsigned_integral T,
         std::unsigned_integral U>
constexpr RetT AddFunctor::operator()(bool &overflow, T l, U r) const
{
    return Add<RetT>(overflow, l, r);
}

struct SubFunctor
{
    template<std::unsigned_integral RetT,
             std::unsigned_integral T,
             std::unsigned_integral U>
    constexpr RetT operator()(bool &overflow, T l, U r) const;
};

template<std::unsigned_integral RetT,
         std::unsigned_integral T,
         std::unsigned_integral U>
constexpr RetT SubFunctor::operator()(bool &overflow, T l, U r) const
{
    return Sub<RetT>(overflow, l, r);
}

struct MulFunctor
{
    template<std::unsigned_integral RetT,
             std::unsigned_integral T,
             std::unsigned_integral U>
    constexpr RetT operator()(bool &overflow, T l, U r) const;
};

template<std::unsigned_integral RetT,
         std::unsigned_integral T,
         std::unsigned_integral U>
constexpr RetT MulFunctor::operator()(bool &overflow, T l, U r) const
{
    return Mul<RetT>(overflow, l, r);
}



template<std::unsigned_integral RetT,
         typename ArithmeticOpT,
         std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr RetT NoThrowArithmeticOp(bool &overflow, T l, U r, Args...  args)
{
    if constexpr(sizeof...(args))
    {
        bool tmp;
        auto ret = NoThrowArithmeticOp<RetT, ArithmeticOpT, RetT, Args...>(overflow, ArithmeticOpT{}.template operator()<RetT>(tmp, l, r), args...);
        overflow = overflow || tmp;
        return ret;
    }
    else
    {
        return ArithmeticOpT{}.template operator()<RetT>(overflow, l, r);
    }
}

template<std::unsigned_integral RetT,
         typename ArithmeticOpT,
         std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr RetT ThrowArithmeticOp(T l, U r, Args... args)
{
    bool overflow;
    auto ret = NoThrowArithmeticOp<RetT,ArithmeticOpT>(overflow, l, r, args...);
    if(overflow)
    {
        throw RangeError{};
    }

    return ret;
}

} //namespace internal

//Semantic of following functions is like (T)((T)(l <op> r) <op> args)...
//for exact details about type casts look into binary versions.
//It seems narrowing cast of every intermediate result doesn't affect final result at all
template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr T Add(bool &overflow, T l, U r, Args... args)
{
    return internal::NoThrowArithmeticOp<T, internal::AddFunctor>(overflow, l, r, args...);
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr T Add(T l, U r, Args... args)
{
    return internal::ThrowArithmeticOp<T, internal::AddFunctor>(l, r, args...);
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr T Sub(bool &overflow, T l, U r, Args... args)
{
    return internal::NoThrowArithmeticOp<T, internal::SubFunctor>(overflow, l, r, args...);
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr T Sub(T l, U r, Args... args)
{
    return internal::ThrowArithmeticOp<T, internal::SubFunctor>(l, r, args...);
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr T Mul(bool &overflow, T l, U r, Args... args)
{
    return internal::NoThrowArithmeticOp<T, internal::MulFunctor>(overflow, l, r, args...);
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr T Mul(T l, U r, Args... args)
{
    return internal::ThrowArithmeticOp<T, internal::MulFunctor>(l, r, args...);
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr bool WouldOverflowAdd(T l, U r, Args... args)
{
    bool ret;
    Add<T>(ret, l, r, args...);
    return ret;
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr bool WouldOverflowSub(T l, U r, Args... args)
{
    bool ret;
    Sub<T>(ret, l, r, args...);
    return ret;
}

template<std::unsigned_integral T,
         std::unsigned_integral U,
         std::unsigned_integral... Args>
constexpr bool WouldOverflowMul(T l, U r, Args... args)
{
    bool ret;
    Mul<T>(ret, l, r, args...);
    return ret;
}



template<std::integral ToT, std::unsigned_integral FromT>
constexpr ToT UintCast(FromT val)
{
    if constexpr(
        std::cmp_greater_equal(std::numeric_limits<ToT>::max(),
                               std::numeric_limits<FromT>::max()))
    {
        return val;
    }
    else
    {
        if(std::cmp_greater(val, std::numeric_limits<ToT>::max()))
        {
            throw RangeError{};
        }

        return static_cast<ToT>(val);
    }
}

template<std::integral ToT, std::signed_integral FromT>
constexpr ToT SintCast(FromT val)
{
    if constexpr(
        std::cmp_greater_equal(std::numeric_limits<ToT>::max(),
                               std::numeric_limits<FromT>::max()) &&
        std::cmp_less_equal(std::numeric_limits<ToT>::min(),
                            std::numeric_limits<FromT>::min()))
    {
        return val;
    }
    else
    {
        if(std::cmp_greater(val, std::numeric_limits<ToT>::max()) ||
           std::cmp_less(val, std::numeric_limits<ToT>::min()))
        {
            throw RangeError{};
        }

        return static_cast<ToT>(val);
    }
}

template<std::integral ToT, std::integral FromT>
    requires std::is_unsigned_v<FromT>
constexpr ToT IntCast(FromT val)
{
    return UintCast<ToT>(val);
}

template<std::integral ToT, std::integral FromT>
    requires std::is_signed_v<FromT>
constexpr ToT IntCast(FromT val)
{
    return SintCast<ToT>(val);
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

    auto bytes = std::bit_cast<std::array<std::byte, sizeof val>>(val);
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

template<std::endian From, std::endian To, std::integral T>
constexpr T EndianCast(T val)
{
    if constexpr(From == To)
    {
        return val;
    }
    else
    {
        return ByteSwap(val);
    }
}

template<std::endian To, std::integral T>
constexpr T EndianCastTo(T val)
{
    return EndianCast<std::endian::native, To, T>(val);
}

template<std::endian From, std::integral T>
constexpr T EndianCastFrom(T val)
{
    return EndianCast<From, std::endian::native, T>(val);
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



//To deal with warnings as errors (while prototyping only)
template <typename T>
void Discard(const T &)
{

}



//Returns 1 if failed to get actual number
unsigned int GetNumCores() noexcept;

}//namespace vd

#endif //VDOWNLOADER_VD_UTILS_H_