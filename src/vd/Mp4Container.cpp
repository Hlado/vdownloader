#include "Mp4Container.h"
#include "Utils.h"

#include <format>

namespace vd
{

namespace
{

std::string AcronymToStr(AP4_UI32 val)
{
    if constexpr(std::endian::native != std::endian::big)
    {
        ByteSwapInplace(val);
    }

    return std::string(reinterpret_cast<const char *>(&val), sizeof(decltype(val)));;
}

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

std::unique_ptr<AP4_Atom> ReadNextAtom(std::shared_ptr<AP4_ByteStream> data)
{
    auto atomFactory = AP4_DefaultAtomFactory{};
    int err;
    AP4_Atom *atom;

    for(err = atomFactory.CreateAtomFromStream(data, atom); AP4_SUCCEEDED(err); )
    {
        auto ret = std::unique_ptr<AP4_Atom>{atom};
        if(ret->GetType() != AP4_ATOM_TYPE_FREE &&
           ret->GetType() != AP4_ATOM_TYPE('s', 'k', 'i', 'p'))
        {
            return ret;
        }
    }

    if(err != AP4_ERROR_EOS)
    {
        throw LibraryCallError{"AP4_DefaultAtomFactory::CreateAtomFromStream", err};
    }

    return nullptr;
}

template <typename ExpectedT>
    requires std::derived_from<ExpectedT, AP4_Atom>
std::unique_ptr<ExpectedT> GetNextAtom(std::shared_ptr<AP4_ByteStream> data, std::string_view atomName)
{
    auto atom = ReadNextAtom(data);
    if(atom == nullptr)
    {
        throw NotFoundError{std::format(R"(stream doesn't contain required atom "{}")", atomName)};
    }

    return AssertAtomType<ExpectedT>(std::move(atom));
}

} //unnamed namespace



Mp4Container::Mp4Container(std::shared_ptr<AP4_ByteStream> data)
{
    //ftyp
    auto ftypAtom = GetNextAtom<AP4_FtypAtom>(data, "ftyp");
    auto brand = ftypAtom->GetMajorBrand();
    if(brand != AP4_ATOM_TYPE('d', 'a', 's', 'h'))
    {
        throw NotSupportedError{std::format(R"(unsupported major brand "{}")", AcronymToStr(brand))};
    }

    //moov
    auto moovAtom = GetNextAtom<AP4_MoovAtom>(data, "moov");

    auto mvhdAtom = dynamic_cast<AP4_MvhdAtom *>(moovAtom->FindChild("mvhd"));
    if(mvhdAtom == nullptr)
    {
        throw NotFoundError{R"("mvhd" atom is not found)"};
    }
    if(mvhdAtom->GetRate() != 0x10000)
    {
        throw NotSupportedError{std::format(R"(rate ({}) is not supported)", mvhdAtom->GetRate())};
    }

    auto &tracks = moovAtom->GetTrakAtoms();
    if(tracks.ItemCount() == 0)
    {
        throw NotFoundError{R"("trak" atom is not found)"};
    }
    else if(tracks.ItemCount() > 1)
    {
        throw NotSupportedError{"multi-track container is not supported"};
    }
    auto &trakAtom = *tracks.FirstItem()->GetData();

    //sidx
    auto sidxAtom = GetNextAtom<AP4_SidxAtom>(data, "sidx");
    AP4_Position sidxEnd;
    if(auto err = data->Tell(sidxEnd); AP4_FAILED(err))
    {
        throw LibraryCallError{"AP4_ByteStream::Tell", err};
    }

    auto anchorPoint = UintCast<std::size_t>((sidxEnd - sidxAtom->GetSize64()));
    mTrack = Track{data, trakAtom, *sidxAtom, anchorPoint};
}

const Track &Mp4Container::GetTrack() const noexcept
{
    return *mTrack;
}

Track &Mp4Container::GetTrack() noexcept
{
    return *mTrack;
}

} //namespace vd