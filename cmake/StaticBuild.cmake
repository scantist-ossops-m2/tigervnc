#
# Best-effort magic that tries to produce semi-static binaries
# (i.e. only depends on "safe" libraries like libc and libX11)
#
# Note that this often fails as there is no way to automatically
# determine the dependencies of the libraries we depend on, and
# a lot of details change with each different build environment.
#

option(BUILD_STATIC
    "Link statically against most libraries, if possible" OFF)

option(BUILD_STATIC_GCC
    "Link statically against only libgcc and libstdc++" OFF)

macro(find_static_library var)
  unset(${var})
  unset(${var} CACHE)

  cmake_parse_arguments(ARGS "" "" "NAMES;HINTS" ${ARGN})

  unset(STATIC_NAMES)
  foreach(NAME IN LISTS ARGS_NAMES)
    list(APPEND STATIC_NAMES ${CMAKE_STATIC_LIBRARY_PREFIX}${NAME}${CMAKE_STATIC_LIBRARY_SUFFIX})
  endforeach()

  find_library(${var} NAMES ${STATIC_NAMES} HINTS ${ARGS_HINTS})

  if(NOT ${var})
    find_library(${var} NAMES ${ARGS_NAMES} HINTS ${ARGS_HINTS})
    if(${var})
      message("Could not find static version of \"${ARGS_NAMES}\", using dynamic linking")
    endif()
  endif()
endmacro()

if(BUILD_STATIC)
  message(STATUS "Attempting to link static binaries...")

  set(BUILD_STATIC_GCC 1)

  find_static_library(JPEG_LIBRARIES NAMES jpeg)
  find_static_library(ZLIB_LIBRARIES NAMES z)
  find_static_library(PIXMAN_LIBRARIES NAMES pixman-1)

  # gettext is included in libc on many unix systems
  if(NOT LIBC_HAS_DGETTEXT)
    find_static_library(UNISTRING_LIBRARY NAMES unistring
      HINTS ${PC_GETTEXT_LIBDIR} ${PC_GETTEXT_LIBRARY_DIRS})
    find_static_library(INTL_LIBRARY NAMES intl
      HINTS ${PC_GETTEXT_LIBDIR} ${PC_GETTEXT_LIBRARY_DIRS})

    set(GETTEXT_LIBRARIES "")

    if(INTL_LIBRARY)
      list(APPEND GETTEXT_LIBRARIES ${INTL_LIBRARY})
    endif()

    if(UNISTRING_LIBRARY)
      list(APPEND GETTEXT_LIBRARIES ${UNISTRING_LIBRARY})
    endif()

    if (APPLE)
      list(APPEND GETTEXT_LIBRARIES -liconv)
    else()
      find_static_library(ICONV_LIBRARY NAMES iconv
        HINTS ${PC_GETTEXT_LIBDIR} ${PC_GETTEXT_LIBRARY_DIRS})
      list(APPEND GETTEXT_LIBRARIES ${ICONV_LIBRARY})
    endif()

    if(APPLE)
      list(APPEND GETTEXT_LIBRARIES "-framework Carbon")
    endif()
  endif()

  if(NETTLE_FOUND)
    find_static_library(NETTLE_LIBRARIES NAMES nettle)
    find_static_library(HOGWEED_LIBRARIES NAMES hogweed)
    find_static_library(GMP_LIBRARIES NAMES gmp)
  endif()

  if(GNUTLS_FOUND)
    find_static_library(GNUTLS_LIBRARIES NAMES gnutls
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})

    find_static_library(TASN1_LIBRARY NAMES tasn1
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})
    find_static_library(IDN2_LIBRARY NAMES idn2
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})
    find_static_library(P11KIT_LIBRARY NAMES p11-kit
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})
    find_static_library(UNISTRING_LIBRARY NAMES unistring
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})
    find_static_library(ZSTD_LIBRARY NAMES zstd
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})
    find_static_library(BROTLIENC_LIBRARY NAMES brotlienc
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})
    find_static_library(BROTLIDEC_LIBRARY NAMES brotlidec
      HINTS ${PC_GNUTLS_LIBDIR} ${PC_GNUTLS_LIBRARY_DIRS})

    if(TASN1_LIBRARY)
      list(APPEND GNUTLS_LIBRARIES ${TASN1_LIBRARY})
    endif()
    if(IDN2_LIBRARY)
      list(APPEND GNUTLS_LIBRARIES ${IDN2_LIBRARY})
    endif()
    if(P11KIT_LIBRARY)
      list(APPEND GNUTLS_LIBRARIES ${P11KIT_LIBRARY})
    endif()
    if(UNISTRING_LIBRARY)
      list(APPEND GNUTLS_LIBRARIES ${UNISTRING_LIBRARY})
    endif()
    if(ZSTD_LIBRARY)
      list(APPEND GNUTLS_LIBRARIES ${ZSTD_LIBRARY})
    endif()
    if(BROTLIENC_LIBRARY)
      list(APPEND GNUTLS_LIBRARIES ${BROTLIENC_LIBRARY})
    endif()
    if(BROTLIDEC_LIBRARY)
      list(APPEND GNUTLS_LIBRARIES ${BROTLIDEC_LIBRARY})
    endif()

    if (WIN32)
      # GnuTLS uses various crypto-api stuff
      list(APPEND GNUTLS_LIBRARIES -lcrypt32 -lncrypt)
      # And sockets
      list(APPEND GNUTLS_LIBRARIES -lws2_32)
    endif()

    # GnuTLS uses nettle, gettext and zlib, so make sure those are
    # always included and in the proper order
    list(APPEND GNUTLS_LIBRARIES ${HOGWEED_LIBRARIES})
    list(APPEND GNUTLS_LIBRARIES ${NETTLE_LIBRARIES})
    list(APPEND GNUTLS_LIBRARIES ${GMP_LIBRARIES})
    list(APPEND GNUTLS_LIBRARIES ${ZLIB_LIBRARIES})
    if(GETTEXT_LIBRARIES)
      list(APPEND GNUTLS_LIBRARIES ${GETTEXT_LIBRARIES})
    endif()
  endif()

  if(DEFINED FLTK_LIBRARIES)
    find_static_library(FLTK_LIBRARY NAMES fltk)
    find_static_library(FLTK_IMAGES_LIBRARY NAMES fltk_images)
    find_static_library(PNG_LIBRARY NAMES png)

    set(FLTK_LIBRARIES "")

    list(APPEND FLTK_LIBRARIES ${FLTK_IMAGES_LIBRARY})
    list(APPEND FLTK_LIBRARIES ${PNG_LIBRARY})
    list(APPEND FLTK_LIBRARIES ${JPEG_LIBRARIES})

    list(APPEND FLTK_LIBRARIES ${FLTK_LIBRARY})

    if(WIN32)
      list(APPEND FLTK_LIBRARIES -lcomctl32)
    elseif(APPLE)
      list(APPEND FLTK_LIBRARIES "-framework Cocoa")
    else()
      list(APPEND FLTK_LIBRARIES -lm -ldl)
    endif()

    if(X11_FOUND AND NOT APPLE)
      if(X11_Xcursor_FOUND)
        find_static_library(X11_Xcursor_LIB NAMES Xcursor)
        list(APPEND FLTK_LIBRARIES ${X11_Xcursor_LIB})
      endif()

      if(X11_Xfixes_FOUND)
        find_static_library(X11_Xfixes_LIB NAMES Xfixes)
        list(APPEND FLTK_LIBRARIES ${X11_Xfixes_LIB})
      endif()

      if(X11_Xft_FOUND)
        find_static_library(X11_Xft_LIB NAMES Xft)
        find_static_library(FONTCONFIG_LIBRARY NAMES fontconfig)
        find_static_library(EXPAT_LIBRARY NAMES expat)
        find_static_library(FREETYPE_LIBRARY NAMES freetype)
        find_static_library(BZ2_LIBRARY NAMES bz2)
        find_static_library(UUID_LIBRARY NAMES uuid)
        list(APPEND FLTK_LIBRARIES ${X11_Xft_LIB})
        list(APPEND FLTK_LIBRARIES ${FONTCONFIG_LIBRARY})
        list(APPEND FLTK_LIBRARIES ${EXPAT_LIBRARY})
        list(APPEND FLTK_LIBRARIES ${FREETYPE_LIBRARY})
        list(APPEND FLTK_LIBRARIES ${PNG_LIBRARY})
        list(APPEND FLTK_LIBRARIES ${BZ2_LIBRARY})
        list(APPEND FLTK_LIBRARIES ${UUID_LIBRARY})
      endif()

      if(X11_Xrender_FOUND)
        find_static_library(X11_Xrender_LIB NAMES Xrender)
        list(APPEND FLTK_LIBRARIES ${X11_Xrender_LIB})
      endif()

      if(X11_Xext_FOUND)
        list(APPEND FLTK_LIBRARIES ${X11_Xext_LIB})
        find_static_library(X11_Xinerama_LIB NAMES Xinerama)
      endif()

      if(X11_Xinerama_FOUND)
        list(APPEND FLTK_LIBRARIES ${X11_Xinerama_LIB})
        find_static_library(X11_Xext_LIB NAMES Xext)
      endif()

      list(APPEND FLTK_LIBRARIES -lX11)
    endif()
  endif()

  # X11 libraries change constantly on Linux systems so we have to link
  # them statically, even libXext. libX11 is somewhat stable, although
  # even it has had an ABI change once or twice.
  if(X11_FOUND AND NOT APPLE)
    if(X11_Xext_FOUND)
      find_static_library(X11_Xext_LIB NAMES Xext)
    endif()
    if(X11_Xtst_FOUND)
      find_static_library(X11_Xtst_LIB NAMES Xtst)
    endif()
    if(X11_Xdamage_FOUND)
      find_static_library(X11_Xdamage_LIB NAMES Xdamage)
    endif()
    if(X11_Xrandr_FOUND)
      find_static_library(X11_Xrandr_LIB NAMES Xrandr)
    endif()
    if(X11_Xi_FOUND)
      find_static_library(X11_Xi_LIB NAMES Xi)
    endif()
    if(X11_Xrandr_LIB)
      find_static_library(X11_Xrender_LIB NAMES Xrender)
      list(APPEND X11_Xrandr_LIB ${X11_Xrender_LIB})
    endif()
  endif()
endif()

if(BUILD_STATIC_GCC)
  # This ensures that we don't depend on libstdc++ or libgcc_s
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nodefaultlibs")
  set(STATIC_BASE_LIBRARIES "")
  if(ENABLE_ASAN AND NOT WIN32 AND NOT APPLE)
    find_static_library(ASAN_LIBRARY NAMES asan)
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} ${ASAN_LIBRARY}")
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} -ldl -lpthread")
  endif()
  if(ENABLE_TSAN AND NOT WIN32 AND NOT APPLE AND CMAKE_SIZEOF_VOID_P MATCHES 8)
    find_static_library(TSAN_LIBRARY NAMES tsan)
    # libtsan redefines some C++ symbols which then conflict with a
    # statically linked libstdc++. Work around this by allowing multiple
    # definitions. The linker will pick the first one (i.e. the one
    # from libtsan).
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} -Wl,-z -Wl,muldefs")
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} ${TSAN_LIBRARY}")
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} -ldl -lm")
  endif()
  if(WIN32)
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} -lmingw32 -lgcc_eh -lgcc -lmoldname -lmingwex -lmsvcrt")
    find_static_library(PTHREAD_LIBRARY NAMES pthread)
    if(PTHREAD_LIBRARY)
      # pthread has to be statically linked after libraries above and before kernel32
      set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} ${PTHREAD_LIBRARY}")
    endif()
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} -luser32 -lkernel32 -ladvapi32 -lshell32")
    # mingw has some fun circular dependencies that requires us to link
    # these things again
    set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} -lmingw32 -lgcc_eh -lgcc -lmoldname -lmingwex -lmsvcrt")
  else()
    if (NOT APPLE)
      set(STATIC_BASE_LIBRARIES "${STATIC_BASE_LIBRARIES} -lm -lgcc -lgcc_eh -lc")
    endif()
  endif()
  set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} ${STATIC_BASE_LIBRARIES}")
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    find_static_library(CPP_LIBRARY NAMES c++)
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${CPP_LIBRARY}")
  else()
    find_static_library(STDCPP_LIBRARY NAMES stdc++)
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${STDCPP_LIBRARY}")
  endif()
  set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${STATIC_BASE_LIBRARIES}")
endif()
