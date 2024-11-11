#include "Mp4Container.h"
#include "Ap4Helpers.h"
#include "Utils.h"

#include <format>

namespace vd
{

using namespace internal;

Mp4Container::Mp4Container(std::shared_ptr<AP4_ByteStream> data)
{
    if(!data)
    {
        throw ArgumentError(R"("data" is null)");
    }

    //ftyp
    auto ftypAtom = GetNextAtom<AP4_FtypAtom>(data, AP4_ATOM_TYPE_FTYP);
    auto brand = ftypAtom->GetMajorBrand();
    if(brand != gBrandTypeDash)
    {
        throw NotSupportedError{std::format(R"(unsupported major brand "{}")", AcronymToStr(brand))};
    }

    //moov
    auto moovAtom = GetNextAtom<AP4_MoovAtom>(data, AP4_ATOM_TYPE_MOOV);

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
    auto sidxAtom = GetNextAtom<AP4_SidxAtom>(data, AP4_ATOM_TYPE_SIDX);
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