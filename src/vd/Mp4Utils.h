#ifndef VDOWNLOADER_VD_MP4_UTILS_H_
#define VDOWNLOADER_VD_MP4_UTILS_H_

#include "Ap4Headers.h"
#include "Errors.h"
#include "Utils.h"

#include <chrono>
#include <concepts>
#include <memory>
#include <string_view>

namespace vd
{

namespace internal
{

std::string AcronymToStr(AP4_UI32 val);
std::unique_ptr<AP4_Atom> ReadNextAtom(std::shared_ptr<AP4_ByteStream> data);
std::chrono::nanoseconds DurationNano(std::uint64_t dur, std::uint32_t timescale);

template <class DerivedT, class BaseT>
    requires std::derived_from<DerivedT, BaseT> &&
             std::derived_from<BaseT, AP4_Atom>
std::unique_ptr<DerivedT> AssertAtomType(std::unique_ptr<BaseT> &&base)
{
    if(base == nullptr)
    {
        throw NullPointerError{R"("base" is null)"};
    }

    auto ret = DynamicUniqueCast<DerivedT>(std::move(base));

    if(ret == nullptr)
    {
        throw ArgumentError(std::format(R"(unexpected atom "{}")", AcronymToStr(base->GetType())));
    }

    return ret;
}

template <typename ExpectedT>
    requires std::derived_from<ExpectedT, AP4_Atom>
std::unique_ptr<ExpectedT> GetNextAtom(std::shared_ptr<AP4_ByteStream> data, AP4_Atom::Type atomType)
{
    auto atom = ReadNextAtom(data);
    if(atom == nullptr)
    {
        throw NotFoundError{std::format(R"(stream doesn't contain required atom "{}")", AcronymToStr(atomType))};
    }
    else if(atom->GetType() != atomType)
    {
        throw Error(std::format(R"(unexpected atom "{}" found instead of "{}")", AcronymToStr(atom->GetType()), AcronymToStr(atomType)));
    }

    return AssertAtomType<ExpectedT>(std::move(atom));
}

}//namespace internal

} //namespace vd

#endif //VDOWNLOADER_VD_MP4_UTILS_H_