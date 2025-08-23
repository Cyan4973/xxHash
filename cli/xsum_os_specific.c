/*
 * xxhsum - Command line interface for xxhash algorithms
 * Copyright (C) 2013-2021 Yann Collet
 *
 * GPL v2 License
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can contact the author at:
 *   - xxHash homepage: https://www.xxhash.com
 *   - xxHash source repository: https://github.com/Cyan4973/xxHash
 */

#include "xsum_os_specific.h"  /* XSUM_API */
#include <sys/stat.h>   /* stat() / _stat64() */

/*
 * This file contains all of the ugly boilerplate to make xxhsum work across
 * platforms.
 */
#if defined(_MSC_VER) || XSUM_WIN32_USE_WCHAR
    typedef struct __stat64 XSUM_stat_t;
# if defined(_MSC_VER)
    typedef int mode_t;
# endif
#else
    typedef struct stat XSUM_stat_t;
#endif

#if defined(__EMSCRIPTEN__) && defined(XSUM_NODE_JS)
#  include <unistd.h>   /* isatty */
#  include <emscripten.h> /* EM_ASM_INT */

/* The Emscripten SDK does not properly detect when the standard streams
 * are piped to node.js, and there does not seem to be any way to tell in
 * plain C. To work around it, inline JavaScript is used to call Node's
 * isatty() function. */
static int XSUM_IS_CONSOLE(FILE* stdStream)
{
    /* https://github.com/iliakan/detect-node */
    int is_node = EM_ASM_INT((
        return (Object.prototype.toString.call(
            typeof process !== 'undefined' ? process : 0
        ) == '[object process]') | 0
    ));
    if (is_node) {
        return EM_ASM_INT(
            return require('node:tty').isatty($0),
            fileno(stdStream)
        );
    } else {
        return isatty(fileno(stdStream));
    }
}
#elif defined(__EMSCRIPTEN__) || (defined(__linux__) && (XSUM_PLATFORM_POSIX_VERSION >= 1)) \
 || (XSUM_PLATFORM_POSIX_VERSION >= 200112L) \
 || defined(__DJGPP__) \
 || defined(__MSYS__) \
 || defined(__HAIKU__)
#  ifdef __OpenBSD__
#    include <errno.h>       /* errno */
#    include <string.h>      /* strerror */
#    include "xsum_output.h" /* XSUM_log */
#  endif
#  include <unistd.h>   /* isatty */
#  define XSUM_IS_CONSOLE(stdStream) isatty(fileno(stdStream))
#elif defined(MSDOS) || defined(OS2)
#  include <io.h>       /* _isatty */
#  define XSUM_IS_CONSOLE(stdStream) _isatty(_fileno(stdStream))
#elif defined(_WIN32)
#  include <io.h>      /* _isatty */
#  include <windows.h> /* DeviceIoControl, HANDLE, FSCTL_SET_SPARSE */
#  include <stdio.h>   /* FILE */
static __inline int XSUM_IS_CONSOLE(FILE* stdStream)
{
    DWORD dummy;
    return _isatty(_fileno(stdStream)) && GetConsoleMode((HANDLE)_get_osfhandle(_fileno(stdStream)), &dummy);
}
#else
#  define XSUM_IS_CONSOLE(stdStream) 0
#endif

#if defined(MSDOS) || defined(OS2) || defined(_WIN32)
#  include <fcntl.h>   /* _O_BINARY */
#  include <io.h>      /* _setmode, _fileno, _get_osfhandle */
#  if !defined(__DJGPP__)
#    include <windows.h> /* DeviceIoControl, HANDLE, FSCTL_SET_SPARSE */
#    include <winioctl.h> /* FSCTL_SET_SPARSE */
#    define XSUM_SET_BINARY_MODE(file) { int const unused=_setmode(_fileno(file), _O_BINARY); (void)unused; }
#  else
#    define XSUM_SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#  endif
#else
#  define XSUM_SET_BINARY_MODE(file) ((void)file)
#endif

XSUM_API int XSUM_isConsole(FILE* stream)
{
    return XSUM_IS_CONSOLE(stream);
}

XSUM_API void XSUM_setBinaryMode(FILE* stream)
{
    XSUM_SET_BINARY_MODE(stream);
}

#if !XSUM_WIN32_USE_WCHAR

XSUM_API FILE* XSUM_fopen(const char* filename, const char* mode)
{
    return fopen(filename, mode);
}
XSUM_ATTRIBUTE((__format__(__printf__, 2, 0)))
XSUM_API int XSUM_vfprintf(FILE* stream, const char* format, va_list ap)
{
    return vfprintf(stream, format, ap);
}

static int XSUM_stat(const char* infilename, XSUM_stat_t* statbuf)
{
#if defined(_MSC_VER)
    return _stat64(infilename, statbuf);
#else
    return stat(infilename, statbuf);
#endif
}

#ifndef XSUM_NO_MAIN
int main(int argc, const char* argv[])
{
#ifdef __OpenBSD__
    /*
     * xxhsum(1) does not create or write files, permit reading only.
     */
    if (pledge("stdio rpath", NULL) == -1) {
        XSUM_log("pledge: %s\n", strerror(errno));
        return 1;
    }
#endif

    return XSUM_main(argc, argv);
}
#endif

/* Unicode helpers for Windows to make UTF-8 act as it should. */
#else
#  include <windows.h>
#  include <wchar.h>
#  if defined(XXHSUM_WIN32_LONGPATH) && XXHSUM_WIN32_LONGPATH
#    include <pathcch.h> /* PathCchCanonicalizeEx, PathCchCombineEx */
#  endif

/*****************************************************************************
 *                       Unicode conversion tools
 *****************************************************************************/

/*
 * Converts a UTF-8 string to UTF-16. Acts like strdup. The string must be freed afterwards.
 * This version allows keeping the output length.
 */
static wchar_t* XSUM_widenString(const char* str, int* lenOut)
{
    int const len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (lenOut != NULL) *lenOut = len;
    if (len == 0) return NULL;
    {   wchar_t* buf = (wchar_t*)malloc((size_t)len * sizeof(wchar_t));
        if (buf != NULL) {
            if (MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, len) == 0) {
                free(buf);
                return NULL;
       }    }
       return buf;
    }
}

/*
 * Converts a UTF-16 string to UTF-8. Acts like strdup. The string must be freed afterwards.
 * This version allows keeping the output length.
 */
static char* XSUM_narrowString(const wchar_t *str, int *lenOut)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
    if (lenOut != NULL) *lenOut = len;
    if (len == 0) return NULL;
    {   char* const buf = (char*)malloc((size_t)len * sizeof(char));
        if (buf != NULL) {
            if (WideCharToMultiByte(CP_UTF8, 0, str, -1, buf, len, NULL, NULL) == 0) {
                free(buf);
                return NULL;
        }    }
        return buf;
    }
}

/*
 * Converts a UTF-8 path to absolute extended-length path with "\\?\" prefix in UTF-16.
 * Acts like strdup. The string must be freed afterwards.
 * This version allows keeping the output length.
 *
 * Note: The \\?\ prefix (prefix with question) designates a file-system-only path.
 * Unlike the \\.\ prefix (prefix with dot), it does not provide access to DOS device names (e.g. COM1, NUL, CON, etc).
 */
static wchar_t* XSUM_widenStringAsExtendedLengthPath(const char* path)
{
#if defined(XXHSUM_WIN32_LONGPATH) && XXHSUM_WIN32_LONGPATH
    wchar_t* const wide_path = XSUM_widenString(path, NULL);  /* path in wchar_t */
    size_t const path_len = strlen(path);
    int const starts_with_extended_prefix = path_len >= 4 && path[0] == '\\' && path[1] == '\\' && path[2] == '?' && path[3] == '\\';

    /* If path starts with "\\?\" */
    if(starts_with_extended_prefix) {
        /* just return wchar_t version of it. */
        return wide_path;
    } else {
        wchar_t* result = NULL;

        size_t const size_in_wchars  = 32768; /* 32767 wchar_t + NUL */
        ULONG  const canonical_flags = PATHCCH_DO_NOT_NORMALIZE_SEGMENTS;
        ULONG  const combine_flags   = canonical_flags | PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH;

        /* exl_path : buffer for extended length path */
        wchar_t* const exl_path = (wchar_t*) malloc(size_in_wchars * sizeof(wchar_t));
        if(exl_path != NULL) {
            int const starts_with_unc_absolute = path_len >= 2 && path[0] == '\\' && path[1] == '\\';
            int const starts_with_dos_absolute = path_len >= 3 && isalpha(path[0]) && path[1] == ':' && (path[2] == '\\' || path[2] == '/');

            /* If path starts with "\\" or "[A-Za-z]:\" */
            if(starts_with_unc_absolute || starts_with_dos_absolute) {
                if(exl_path != NULL) {
                    HRESULT const hr = PathCchCanonicalizeEx(exl_path, size_in_wchars, wide_path, canonical_flags);
                    if(SUCCEEDED(hr)) {
                        result = exl_path;
                    }
                }
            } else {
                /* path is relative path */
                wchar_t* const cwd = (wchar_t*) malloc(size_in_wchars * sizeof(wchar_t));
                if(cwd != NULL) {
                    DWORD const n = GetCurrentDirectoryW((DWORD) size_in_wchars, cwd);
                    if(n != 0 && n < size_in_wchars) {
                        if(exl_path != NULL) {
                            HRESULT const hr = PathCchCombineEx(exl_path, size_in_wchars, cwd, wide_path, combine_flags);
                            if(SUCCEEDED(hr)) {
                                result = exl_path;
                            }
                        }
                    }
                    free(cwd);
                }
            }

            /* Tricky part: if result doesn't use exl_path, free exl_path */
            if(result != exl_path) {
                free(exl_path);
            }
        }
        free(wide_path);
        return result;
    }
#else
    return XSUM_widenString(path, NULL);  /* path in wchar_t */
#endif
}



/*****************************************************************************
 *                             File helpers
 *****************************************************************************/
/*
 * fopen wrapper that supports UTF-8
 *
 * fopen will only accept ANSI filenames, which means that we can't open Unicode filenames.
 *
 * In order to open a Unicode filename and long path, we need to convert filenames to UTF-16,
 * absolute path, UNC and use _wfopen.
 *
 * Note: The \\?\ prefix designates a file-system-only path.
 * Unlike the \\.\ prefix, it does not provide access to DOS device names (e.g. COM1, NUL, CON, etc).
 */
XSUM_API FILE* XSUM_fopen(const char* filename, const char* mode)
{
    FILE* f = NULL;
    wchar_t* const wide_filename = XSUM_widenStringAsExtendedLengthPath(filename);
    if (wide_filename != NULL) {
        wchar_t* const wide_mode = XSUM_widenString(mode, NULL);
        if (wide_mode != NULL) {
            f = _wfopen(wide_filename, wide_mode);
            free(wide_mode);
        }
        free(wide_filename);
    }
    return f;
}

/*
 * stat() wrapper which supports UTF-8 filenames.
 */
static int XSUM_stat(const char* infilename, XSUM_stat_t* statbuf)
{
    int r = -1;
    wchar_t* const wide_filename = XSUM_widenStringAsExtendedLengthPath(infilename);
    if (wide_filename != NULL) {
        r = _wstat64(wide_filename, statbuf);
        free(wide_filename);
    }
    return r;
}

/*
 * In case it isn't available, this is what MSVC 2019 defines in stdarg.h.
 */
#if defined(_MSC_VER) && !defined(__clang__) && !defined(va_copy)
#  define XSUM_va_copy(destination, source) ((destination) = (source))
#else
#  define XSUM_va_copy(destination, source) va_copy(destination, source)
#endif

/*
 * vasprintf for Windows.
 */
XSUM_ATTRIBUTE((__format__(__printf__, 2, 0)))
static int XSUM_vasprintf(char** strp, const char* format, va_list ap)
{
    int size;
    va_list copy;
    /*
     * To be safe, make a va_copy.
     *
     * Note that Microsoft doesn't use va_copy in its sample code:
     *   https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/vsprintf-vsprintf-l-vswprintf-vswprintf-l-vswprintf-l?view=vs-2019
     */
    XSUM_va_copy(copy, ap);
    /* Calculate how many characters we need */
    size = _vscprintf(format, ap);
    va_end(copy);

    if (size < 0) {
        *strp = NULL;
        return size;
    } else {
        int ret;
        *strp = (char*) malloc((size_t)size + 1);
        if (*strp == NULL) {
            return -1;
        }
        /* vsprintf into the new buffer */
        ret = vsprintf(*strp, format, ap);
        if (ret < 0) {
            free(*strp);
            *strp = NULL;
        }
        return ret;
    }
}

/*
 * fprintf wrapper that supports UTF-8.
 *
 * fprintf doesn't properly handle Unicode on Windows.
 *
 * Additionally, it is codepage sensitive on console and may crash the program.
 *
 * Instead, we use vsnprintf, and either print with fwrite or convert to UTF-16
 * for console output and use the codepage-independent WriteConsoleW.
 *
 * Credit to t-mat: https://github.com/t-mat/xxHash/commit/5691423
 */
XSUM_ATTRIBUTE((__format__(__printf__, 2, 0)))
XSUM_API int XSUM_vfprintf(FILE *stream, const char *format, va_list ap)
{
    int result;
    char* u8_str = NULL;

    /*
     * Generate the UTF-8 output string with vasprintf.
     */
    result = XSUM_vasprintf(&u8_str, format, ap);

    if (result >= 0) {
        const size_t nchar = (size_t)result + 1;

        /*
         * Check if we are outputting to a console. Don't use XSUM_isConsole
         * directly -- we don't need to call _get_osfhandle twice.
         */
        int fileNb = _fileno(stream);
        intptr_t handle_raw = _get_osfhandle(fileNb);
        HANDLE handle = (HANDLE)handle_raw;
        DWORD dwTemp;

        if (handle_raw < 0) {
             result = -1;
        } else if (_isatty(fileNb) && GetConsoleMode(handle, &dwTemp)) {
            /*
             * Convert to UTF-16 and output with WriteConsoleW.
             *
             * This is codepage independent and works on Windows XP's default
             * msvcrt.dll.
             */
            int len;
            wchar_t* const u16_buf = XSUM_widenString(u8_str, &len);
            if (u16_buf == NULL) {
                result = -1;
            } else {
                if (WriteConsoleW(handle, u16_buf, (DWORD)len - 1, &dwTemp, NULL)) {
                    result = (int)dwTemp;
                } else {
                    result = -1;
                }
                free(u16_buf);
            }
        } else {
            /* fwrite the UTF-8 string if we are printing to a file */
            result = (int)fwrite(u8_str, 1, nchar - 1, stream);
            if (result == 0) {
                result = -1;
            }
        }
        free(u8_str);
    }
    return result;
}

#ifndef XSUM_NO_MAIN
/*****************************************************************************
 *                    Command Line argument parsing
 *****************************************************************************/

/* Converts a UTF-16 argv to UTF-8. */
static char** XSUM_convertArgv(int argc, wchar_t* utf16_argv[])
{
    char** const utf8_argv = (char**)malloc((size_t)(argc + 1) * sizeof(char*));
    if (utf8_argv != NULL) {
        int i;
        for (i = 0; i < argc; i++) {
            utf8_argv[i] = XSUM_narrowString(utf16_argv[i], NULL);
            if (utf8_argv[i] == NULL) {
                /* Out of memory, whoops. */
                while (i-- > 0) {
                    free(utf8_argv[i]);
                }
                free(utf8_argv);
                return NULL;
            }
        }
        utf8_argv[argc] = NULL;
    }
    return utf8_argv;
}
/* Frees arguments returned by XSUM_convertArgv */
static void XSUM_freeArgv(int argc, char** argv)
{
    int i;
    if (argv == NULL) {
        return;
    }
    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

static int XSUM_wmain(int argc, wchar_t* utf16_argv[])
{
    /* Convert the UTF-16 arguments to UTF-8. */
    char** utf8_argv = XSUM_convertArgv(argc, utf16_argv);

    if (utf8_argv == NULL) {
        /* An unfortunate but incredibly unlikely error. */
        fprintf(stderr, "xxhsum: error converting command line arguments!\n");
        abort();
    } else {
        int ret;

        /*
         * MinGW's terminal uses full block buffering for stderr.
         *
         * This is nonstandard behavior and causes text to not display until
         * the buffer fills.
         *
         * `setvbuf()` can easily correct this to make text display instantly.
         */
        setvbuf(stderr, NULL, _IONBF, 0);

        /* Call our real main function */
        ret = XSUM_main(argc, (void*)utf8_argv);

        /* Cleanup */
        XSUM_freeArgv(argc, utf8_argv);
        return ret;
    }
}

#if XSUM_WIN32_USE_WMAIN

/*
 * The preferred method of obtaining the real UTF-16 arguments. Always works
 * on MSVC, sometimes works on MinGW-w64 depending on the compiler flags.
 */
#ifdef __cplusplus
extern "C"
#endif
int __cdecl wmain(int argc, wchar_t* utf16_argv[])
{
    return XSUM_wmain(argc, utf16_argv);
}
#else /* !XSUM_WIN32_USE_WMAIN */

/*
 * Wrap `XSUM_wmain()` using `main()` and `__wgetmainargs()` on MinGW without
 * Unicode support.
 *
 * `__wgetmainargs()` is used in the CRT startup to retrieve the arguments for
 * `wmain()`, so we use it on MinGW to emulate `wmain()`.
 *
 * It is an internal function and not declared in any public headers, so we
 * have to declare it manually.
 *
 * An alternative that doesn't mess with internal APIs is `GetCommandLineW()`
 * with `CommandLineToArgvW()`, but the former doesn't expand wildcards and the
 * latter requires linking to Shell32.dll and its numerous dependencies.
 *
 * This method keeps our dependencies to kernel32.dll and the CRT.
 *
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/getmainargs-wgetmainargs?view=vs-2019
 */
typedef struct {
    int newmode;
} _startupinfo;

#ifdef __cplusplus
extern "C"
#endif
int __cdecl __wgetmainargs(
    int*          Argc,
    wchar_t***    Argv,
    wchar_t***    Env,
    int           DoWildCard,
    _startupinfo* StartInfo
);

int main(int ansi_argc, const char* ansi_argv[])
{
    int       utf16_argc;
    wchar_t** utf16_argv;
    wchar_t** utf16_envp;         /* Unused but required */
    _startupinfo startinfo = {0}; /* 0 == don't change new mode */

    /* Get wmain's UTF-16 arguments. Make sure we expand wildcards. */
    if (__wgetmainargs(&utf16_argc, &utf16_argv, &utf16_envp, 1, &startinfo) < 0)
        /* In the very unlikely case of an error, use the ANSI arguments. */
        return XSUM_main(ansi_argc, ansi_argv);

    /* Call XSUM_wmain with our UTF-16 arguments */
    return XSUM_wmain(utf16_argc, utf16_argv);
}

#endif /* !XSUM_WIN32_USE_WMAIN */
#endif /* !XSUM_NO_MAIN */
#endif /* XSUM_WIN32_USE_WCHAR */


/*
 * Determines whether the file at filename is a directory.
 */
XSUM_API int XSUM_isDirectory(const char* filename)
{
    XSUM_stat_t statbuf;
    int r = XSUM_stat(filename, &statbuf);
#ifdef _MSC_VER
    if (!r && (statbuf.st_mode & _S_IFDIR)) return 1;
#else
    if (!r && S_ISDIR(statbuf.st_mode)) return 1;
#endif
    return 0;
}

/*
 * Returns the filesize of the file at filename.
 */
XSUM_API XSUM_U64 XSUM_getFileSize(const char* filename)
{
    XSUM_stat_t statbuf;
    int r = XSUM_stat(filename, &statbuf);
    if (r || !S_ISREG(statbuf.st_mode)) return 0;   /* No good... */
    return (XSUM_U64)statbuf.st_size;
}
