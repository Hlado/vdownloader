#ifndef VDOWNLOADER_VD_PREPROCESSOR_H_
#define VDOWNLOADER_VD_PREPROCESSOR_H_

//We do not expect this code to be compiled by something else than MSVC/GCC/Clang
//for something else than Windows/Linux, so all conditionals are based on that assumption

#if defined(_MSC_VER)
    #define VDOWNLOADER_COMPILER_MSVC
#elif defined(__GNUG__)
    #define VDOWNLOADER_COMPILER_GCC
#endif

#if defined(_WIN32)
    #define VDOWNLOADER_OS_WINDOWS
#endif

#if defined(VDOWNLOADER_COMPILER_MSVC)
    #define VDOWNLOADER_FUNC __FUNCSIG__
#else
    #define VDOWNLOADER_FUNC __PRETTY_FUNCTION__
#endif

#if defined(VDOWNLOADER_COMPILER_GCC)
    #define VDOWNLOADER_UNUSED __attribute__ ((unused))
#else
    #define VDOWNLOADER_UNUSED
#endif

#endif //VDOWNLOADER_VD_PREPROCESSOR_H_