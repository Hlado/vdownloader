#include "Mp4Utils.h"
#include "Utils.h"

namespace vd
{

namespace internal
{

std::string AcronymToStr(AP4_UI32 val)
{
    val = EndianCastFrom<std::endian::big>(val);

    return std::string(reinterpret_cast<const char *>(&val), sizeof val);
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

std::chrono::nanoseconds DurationNano(std::uint32_t dur, std::uint32_t timescale)
{
    //It's impossible to overflow int64_t here, but real question is:
    //is denominator always at least 64 bits long?
    return std::chrono::nanoseconds{dur * std::nano::den / timescale};
}

}//namespace internal

} //namespace vd