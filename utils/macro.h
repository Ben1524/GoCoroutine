///
/// Created by cxk_zjq on 25-5-29.
///

#ifndef MACRO_H
#define MACRO_H

#if defined(__APPLE__) || defined(__FreeBSD__)
# define LIBGO_SYS_FreeBSD 1
# define LIBGO_SYS_Unix 1
#elif defined(__linux__)
# define LIBGO_SYS_Linux 1
# define LIBGO_SYS_Unix 1
#elif defined(_WIN32)
# define LIBGO_SYS_Windows 1
#endif



#if defined(LIBGO_SYS_Windows)
# define FCONTEXT_CALL __stdcall
#else
# define FCONTEXT_CALL
#endif


# define ALWAYS_INLINE __attribute__ ((always_inline)) inline  /// 强制内联宏定义

#endif ///MACRO_H
