#ifndef VDOWNLOADER_VD_AP4_HEADERS_
#define VDOWNLOADER_VD_AP4_HEADERS_

#include "Preprocessor.h"

#if defined(VDOWNLOADER_COMPILER_MSVC)
    #pragma warning(push, 0)
#elif defined(VDOWNLOADER_COMPILER_GCC)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <Ap4.h>

#if defined(VDOWNLOADER_COMPILER_MSVC)
    #pragma warning(pop)
#elif defined(VDOWNLOADER_COMPILER_GCC)
    #pragma GCC diagnostic pop
#endif

#endif //VDOWNLOADER_VD_AP4_HEADERS_