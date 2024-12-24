#ifndef VDOWNLOADER_VD_AP4_HELPERS_
#define VDOWNLOADER_VD_AP4_HELPERS_

#include "Ap4Headers.h"

namespace vd
{

const auto gAtomTypeMoof = AP4_ATOM_TYPE('m','o','o','f');
const auto gAtomTypeSkip = AP4_ATOM_TYPE('s', 'k', 'i', 'p');
const auto gBrandTypeDash = AP4_ATOM_TYPE('d', 'a', 's', 'h');

}//namespace vd



template <typename T>
T *begin(AP4_Array<T> &container)
{
    if(container.ItemCount() == 0)
    {
        return nullptr;
    }
    else
    {
        return &container[0];
    }
}

template <typename T>
T *end(AP4_Array<T> &container)
{
    if(container.ItemCount() == 0)
    {
        return nullptr;
    }
    else
    {
        auto pointer = &container[container.ItemCount() - 1];
        return ++pointer;
    }
}

template <typename T>
const T *begin(const AP4_Array<T> &container)
{
    return begin(const_cast<AP4_Array<T> &>(container));
}

template <typename T>
const T *end(const AP4_Array<T> &container)
{
    return end(const_cast<AP4_Array<T> &>(container));
}

#endif //VDOWNLOADER_VD_AP4_HELPERS_