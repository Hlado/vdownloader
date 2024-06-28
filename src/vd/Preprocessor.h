#ifndef VDOWNLOADER_VD_PREPROCESSOR_H_
#define VDOWNLOADER_VD_PREPROCESSOR_H_

//We do not expect this code to be compiled by something else than MSVC/GCC/Clang,
//so all conditionals are based on that assumption

#ifdef _MSC_VER
    #define VDOWNLOADER_FUNC __FUNCSIG__
#else
    #define VDOWNLOADER_FUNC __PRETTY_FUNCTION__
#endif

#endif //VDOWNLOADER_VD_PREPROCESSOR_H_