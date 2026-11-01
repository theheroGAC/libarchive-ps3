#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${SRC:-$ROOT/src}"
PREFIX="${PREFIX:-$ROOT/dist/ps3}"
PS3DEV="${PS3DEV:-/usr/local/ps3dev}"
PSL1GHT="${PSL1GHT:-$PS3DEV}"
PORTLIBS="${PORTLIBS:-$PS3DEV/portlibs/ppu}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
TRIPLE="${TRIPLE:-powerpc64-ps3-elf}"
CC="$TRIPLE-gcc"
AR="$TRIPLE-ar"
RANLIB="$TRIPLE-ranlib"
STRIP="$TRIPLE-strip"
BUILD_TRIPLET="$(gcc -dumpmachine 2>/dev/null || echo x86_64-pc-linux-gnu)"

if [ -z "${BUILD_ROOT:-}" ]; then
    if [ -n "${WSL_DISTRO_NAME:-}" ]; then
        BUILD_ROOT="$HOME/.cache/libarchive-ps3-build"
    else
        BUILD_ROOT="$ROOT/build"
    fi
fi

export PS3DEV
export PSL1GHT
export CC
export AR
export RANLIB
export STRIP
export PATH="$PS3DEV/bin:$PS3DEV/ppu/bin:$PATH"

COMMON_CFLAGS="-mcpu=cell -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections -O2 -I$PREFIX/include -I$PORTLIBS/include -I$PSL1GHT/ppu/include"
COMMON_CPPFLAGS="-I$PREFIX/include -I$PORTLIBS/include"
COMMON_LDFLAGS="-L$PREFIX/lib -L$PORTLIBS/lib -L$PSL1GHT/ppu/lib"
LIBARCHIVE_CFLAGS="$COMMON_CFLAGS -Dstrtoimax=strtol -Dstrtoumax=strtoul"

mkdir -p "$PREFIX/include" "$PREFIX/lib/pkgconfig" "$BUILD_ROOT"

for source_dir in libarchive-3.8.11 xz-5.8.4 zstd-1.5.7 lz4-1.10.0; do
    if [ ! -d "$SRC/$source_dir" ]; then
        printf 'Missing source: %s\n' "$source_dir" >&2
        printf 'Run %s/scripts/fetch.sh first.\n' "$ROOT" >&2
        exit 1
    fi
done

build_lzma()
{
    if [ -f "$PREFIX/lib/liblzma.a" ]; then
        return
    fi

    rm -rf "$BUILD_ROOT/xz"
    mkdir -p "$BUILD_ROOT/xz"
    cd "$BUILD_ROOT/xz"

    CFLAGS="$COMMON_CFLAGS" \
    CPPFLAGS="$COMMON_CPPFLAGS" \
    LDFLAGS="$COMMON_LDFLAGS" \
    "$SRC/xz-5.8.4/configure" \
        --host="$TRIPLE" \
        --build="$BUILD_TRIPLET" \
        --prefix="$PREFIX" \
        --disable-shared \
        --enable-static \
        --disable-xz \
        --disable-xzdec \
        --disable-lzmadec \
        --disable-lzmainfo \
        --disable-lzma-links \
        --disable-scripts \
        --disable-doc \
        --disable-nls \
        --disable-rpath \
        --enable-threads=no

    make -j"$JOBS"
    make install
}

build_zstd()
{
    if [ -f "$PREFIX/lib/libzstd.a" ]; then
        return
    fi

    rm -rf "$BUILD_ROOT/zstd-src"
    cp -a "$SRC/zstd-1.5.7" "$BUILD_ROOT/zstd-src"
    rm -f "$BUILD_ROOT/zstd-src/lib/libzstd.a"

    make -C "$BUILD_ROOT/zstd-src/lib" -j"$JOBS" libzstd.a \
        CC="$CC" AR="$AR" RANLIB="$RANLIB" \
        CFLAGS="$COMMON_CFLAGS -DZSTD_DISABLE_ASM"

    install -m 644 "$BUILD_ROOT/zstd-src/lib/libzstd.a" "$PREFIX/lib/libzstd.a"
    install -m 644 "$BUILD_ROOT/zstd-src/lib/zstd.h" "$PREFIX/include/zstd.h"
    install -m 644 "$BUILD_ROOT/zstd-src/lib/zstd_errors.h" "$PREFIX/include/zstd_errors.h"
    install -m 644 "$BUILD_ROOT/zstd-src/lib/zdict.h" "$PREFIX/include/zdict.h"
}

build_lz4()
{
    if [ -f "$PREFIX/lib/liblz4.a" ]; then
        return
    fi

    rm -rf "$BUILD_ROOT/lz4-src"
    cp -a "$SRC/lz4-1.10.0" "$BUILD_ROOT/lz4-src"
    rm -f "$BUILD_ROOT/lz4-src/lib/liblz4.a"

    make -C "$BUILD_ROOT/lz4-src/lib" -j"$JOBS" liblz4.a \
        CC="$CC" AR="$AR" RANLIB="$RANLIB" \
        CFLAGS="$COMMON_CFLAGS -DXXH_NAMESPACE=LZ4_"

    install -m 644 "$BUILD_ROOT/lz4-src/lib/liblz4.a" "$PREFIX/lib/liblz4.a"
    install -m 644 "$BUILD_ROOT/lz4-src/lib/lz4.h" "$PREFIX/include/lz4.h"
    install -m 644 "$BUILD_ROOT/lz4-src/lib/lz4frame.h" "$PREFIX/include/lz4frame.h"
    install -m 644 "$BUILD_ROOT/lz4-src/lib/lz4hc.h" "$PREFIX/include/lz4hc.h"
    install -m 644 "$BUILD_ROOT/lz4-src/lib/xxhash.h" "$PREFIX/include/xxhash.h"
}

copy_system_libs()
{
    if [ -f "$PORTLIBS/lib/libz.a" ]; then
        install -m 644 "$PORTLIBS/lib/libz.a" "$PREFIX/lib/libz.a"
    fi
    if [ -f "$PORTLIBS/lib/libbz2.a" ]; then
        install -m 644 "$PORTLIBS/lib/libbz2.a" "$PREFIX/lib/libbz2.a"
    fi
    if [ -f "$PORTLIBS/include/zlib.h" ]; then
        install -m 644 "$PORTLIBS/include/zlib.h" "$PREFIX/include/zlib.h"
    fi
    if [ -f "$PORTLIBS/include/zconf.h" ]; then
        install -m 644 "$PORTLIBS/include/zconf.h" "$PREFIX/include/zconf.h"
    fi
    if [ -f "$PORTLIBS/include/bzlib.h" ]; then
        install -m 644 "$PORTLIBS/include/bzlib.h" "$PREFIX/include/bzlib.h"
    fi

    mkdir -p "$BUILD_ROOT/compat"
    if [ -f "$PREFIX/lib/libbz2.a" ]; then
        "$CC" $COMMON_CFLAGS -c "$ROOT/scripts/compat/bz_internal_error.c" -o "$BUILD_ROOT/compat/bz_internal_error.o"
        "$AR" r "$PREFIX/lib/libbz2.a" "$BUILD_ROOT/compat/bz_internal_error.o"
        "$RANLIB" "$PREFIX/lib/libbz2.a"
    fi
}

build_libarchive()
{
    if [ -f "$PREFIX/lib/libarchive.a" ]; then
        return
    fi

    if ! grep -q "__lv2ppu__" "$SRC/libarchive-3.8.11/libarchive/archive_write_set_format_iso9660.c"; then
        patch -d "$SRC/libarchive-3.8.11" -p1 < "$ROOT/scripts/patches/libarchive-3.8.11-ps3.patch"
    fi

    rm -rf "$BUILD_ROOT/libarchive"
    mkdir -p "$BUILD_ROOT/libarchive"
    cd "$BUILD_ROOT/libarchive"

    PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PORTLIBS/lib/pkgconfig" \
    PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig:$PORTLIBS/lib/pkgconfig" \
    CFLAGS="$LIBARCHIVE_CFLAGS" \
    CPPFLAGS="$COMMON_CPPFLAGS" \
    LDFLAGS="$COMMON_LDFLAGS" \
    ac_cv_func_arc4random_buf=no \
    ac_cv_func_chflags=no \
    ac_cv_func_close_range=no \
    ac_cv_func_closefrom=no \
    ac_cv_func_fchflags=no \
    ac_cv_func_fchmod=no \
    ac_cv_func_fchown=no \
    ac_cv_func_fdopendir=no \
    ac_cv_func_fork=no \
    ac_cv_func_fstatat=no \
    ac_cv_func_fstatfs=no \
    ac_cv_func_fstatvfs=no \
    ac_cv_func_futimens=no \
    ac_cv_func_futimes=no \
    ac_cv_func_futimesat=no \
    ac_cv_func_getgrgid_r=no \
    ac_cv_func_getgrnam_r=no \
    ac_cv_func_getpwnam_r=no \
    ac_cv_func_getpwuid_r=no \
    ac_cv_func_lchflags=no \
    ac_cv_func_lchmod=no \
    ac_cv_func_lchown=no \
    ac_cv_func_linkat=no \
    ac_cv_func_lstat=no \
    ac_cv_func_lutimes=no \
    ac_cv_func_mkdirat=no \
    ac_cv_func_mkfifo=no \
    ac_cv_func_mknod=no \
    ac_cv_func_openat=no \
    ac_cv_func_pipe=no \
    ac_cv_func_posix_spawn=no \
    ac_cv_func_posix_spawnp=no \
    ac_cv_func_readlink=no \
    ac_cv_func_readlinkat=no \
    ac_cv_func_statfs=no \
    ac_cv_func_statvfs=no \
    ac_cv_func_symlink=no \
    ac_cv_func_unlinkat=no \
    ac_cv_func_utimensat=no \
    ac_cv_func_utimes=no \
    ac_cv_func_vfork=no \
    ac_cv_header_pthread_h=no \
    "$SRC/libarchive-3.8.11/configure" \
        --host="$TRIPLE" \
        --build="$BUILD_TRIPLET" \
        --prefix="$PREFIX" \
        --disable-shared \
        --enable-static \
        --disable-bsdtar \
        --disable-bsdcat \
        --disable-bsdcpio \
        --disable-bsdunzip \
        --disable-xattr \
        --disable-acl \
        --disable-rpath \
        --without-iconv \
        --without-xml2 \
        --without-expat \
        --without-bsdxml \
        --without-openssl \
        --without-mbedtls \
        --without-nettle \
        --without-libb2 \
        --without-lzo2 \
        --without-cng \
        --without-md

    make -j"$JOBS"
    make install
}

write_pkgconfig()
{
    cat > "$PREFIX/lib/pkgconfig/libarchive.pc" <<'EOF'
prefix=${pcfiledir}/../..
exec_prefix=${prefix}
libdir=${prefix}/lib
includedir=${prefix}/include

Name: libarchive
Description: Library for reading and writing streaming archives
Version: 3.8.11
Requires:
Libs: -L${libdir} -larchive -lz -lbz2 -llzma -lzstd -llz4 -lm
Cflags: -I${includedir}
EOF
}

build_lzma
build_zstd
build_lz4
copy_system_libs
build_libarchive
write_pkgconfig

mkdir -p "$BUILD_ROOT/compat"
"$CC" $COMMON_CFLAGS -c "$ROOT/scripts/compat/ps3_compat.c" -o "$BUILD_ROOT/compat/ps3_compat.o"
"$AR" r "$PREFIX/lib/libarchive.a" "$BUILD_ROOT/compat/ps3_compat.o"

"$RANLIB" "$PREFIX/lib/libarchive.a"
"$RANLIB" "$PREFIX/lib/liblzma.a"
"$RANLIB" "$PREFIX/lib/libzstd.a"
"$RANLIB" "$PREFIX/lib/liblz4.a"

printf 'libarchive PS3: %s\n' "$PREFIX/lib/libarchive.a"
