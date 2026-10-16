#!/usr/bin/env bash
# Builds a VitaSDK toolchain from source into /usr/local/vitasdk.
#
# Written for sandboxes where the prebuilt VitaSDK downloads (GitHub release
# assets, Docker Hub, gnu.org mirrors) are blocked but git clones of public
# GitHub repos and the Ubuntu package archive are reachable. Takes ~1.5 h on
# 2 cores. Pinned to the component revisions of vitasdk/buildscripts (Oct 2026).
set -euo pipefail

V=/usr/local/vitasdk
SRC=${SRC:-$HOME/vsdk-src}
JOBS=${JOBS:-$(nproc)}
mkdir -p "$SRC" && cd "$SRC"

apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  libgmp-dev libmpfr-dev libmpc-dev libisl-dev libyaml-dev libzip-dev zlib1g-dev \
  cmake ninja-build texinfo bison flex pkg-config autoconf automake libtool

clone_at() { # dir url rev
  [ -d "$1/.git" ] && return 0
  git init -q "$1" && git -C "$1" remote add origin "$2"
  git -C "$1" fetch -q --depth 1 origin "$3" && git -C "$1" checkout -q FETCH_HEAD
}

git clone -q --depth 1 https://github.com/vitasdk/buildscripts 2>/dev/null || true
clone_at newlib https://github.com/vitasdk/newlib 2e428297c0b6aefd830c5a75a7daa7e774562a42
clone_at vita-headers https://github.com/vitasdk/vita-headers e66ebe90b73d1fa4cce005a5b2072cec27322544
clone_at vita-toolchain https://github.com/vitasdk/vita-toolchain 008ddc2d7fd4bd16631d1e49cacf308de6f545f1
clone_at pthread-embedded https://github.com/vitasdk/pthread-embedded 11d2e5722d98c86f33c908fc47b2cf6e55205db5
git -C vita-toolchain submodule update --init --depth 1
[ -d gcc ] || git clone -q --depth 1 -b releases/gcc-15.2.0 https://github.com/gcc-mirror/gcc gcc
[ -d libvita2d ] || git clone -q --depth 1 https://github.com/xerpi/libvita2d

# binutils + libelf 0.8.13 from the Ubuntu/Debian archives
[ -f binutils_2.46.orig.tar.xz ] || curl -sSfO http://archive.ubuntu.com/ubuntu/pool/main/b/binutils/binutils_2.46.orig.tar.xz
[ -d binutils-with-gold-2.46 ] || { tar xf binutils_2.46.orig.tar.xz
  for p in buildscripts/patches/binutils/*.patch; do patch -d binutils-with-gold-2.46 -p1 -s < "$p"; done
  cp buildscripts/config.guess buildscripts/config.sub binutils-with-gold-2.46/; }
[ -f libelf_0.8.13.orig.tar.gz ] || curl -sSfO http://deb.debian.org/debian/pool/main/libe/libelf/libelf_0.8.13.orig.tar.gz

# 1. binutils
mkdir -p build-binutils && (cd build-binutils &&
  CFLAGS="-std=gnu11 -O2" ../binutils-with-gold-2.46/configure --target=arm-vita-eabi --prefix=$V \
    --with-sysroot=$V/arm-vita-eabi --disable-nls --disable-werror --disable-separate-code \
    --enable-interwork --enable-plugins --without-zstd --disable-gold --disable-gdb --disable-gprofng &&
  make -j$JOBS INFO_DEPS= DVIS= pdfs= htmls= && make install INFO_DEPS= DVIS= pdfs= htmls=)

# 2. libelf 0.8.13 + vita-toolchain host tools
[ -d libelf-0.8.13.orig ] || { tar xf libelf_0.8.13.orig.tar.gz
  patch -d libelf-0.8.13.orig -p1 -s < buildscripts/patches/libelf-configure-in.patch; }
(cd libelf-0.8.13.orig && autoreconf -i . >/dev/null 2>&1 && cp ../buildscripts/config.guess ../buildscripts/config.sub . &&
  CFLAGS="-O2 -fPIC" ./configure --prefix=$SRC/deps --disable-shared --disable-nls && make -j$JOBS && make install)
mkdir -p vita-toolchain/build && (cd vita-toolchain/build &&
  PKG_CONFIG_PATH=$SRC/deps/lib/pkgconfig cmake .. -G Ninja -DCMAKE_INSTALL_PREFIX=$V -DCMAKE_BUILD_TYPE=Release \
    -DTOOLCHAIN_DEPS_DIR=$SRC/deps && CPATH=$SRC/deps/include ninja && ninja install)

# 3. GCC (compiler only)
for p in buildscripts/patches/gcc/*.patch; do patch -d gcc -p1 -N -s < "$p" || true; done
B=$V/bin/arm-vita-eabi
TOOLS="AR_FOR_TARGET=$B-ar LD_FOR_TARGET=$B-ld OBJDUMP_FOR_TARGET=$B-objdump NM_FOR_TARGET=$B-nm \
RANLIB_FOR_TARGET=$B-ranlib READELF_FOR_TARGET=$B-readelf STRIP_FOR_TARGET=$B-strip AS_FOR_TARGET=$B-as"
mkdir -p build-gcc && (cd build-gcc && env $TOOLS ../gcc/configure --target=arm-vita-eabi --prefix=$V \
  --libdir=$V/lib --libexecdir=$V/lib --with-sysroot=$V/arm-vita-eabi --enable-languages=c,c++ \
  --disable-decimal-float --disable-libcc1 --disable-libffi --disable-libmudflap --disable-libquadmath \
  --disable-libssp --disable-libstdcxx-pch --disable-nls --disable-shared --disable-tls --with-gnu-as \
  --with-gnu-ld --with-newlib --without-zstd --disable-multilib --with-arch=armv7-a --with-tune=cortex-a9 \
  --with-fpu=neon --with-float=hard --with-mode=thumb --with-headers=yes --enable-threads=posix \
  --enable-libgomp && make -j$JOBS all-gcc all-lto-plugin INHIBIT_LIBC_CFLAGS="-DUSE_TM_CLONE_REGISTRY=0" &&
  make install-gcc install-lto-plugin)

# 4. Stub libraries + headers
mkdir -p build-headers && (cd build-headers && $V/bin/vita-libs-gen-2 -yml=../vita-headers/db -output=. &&
  make -j$JOBS ARCH=$B && mkdir -p $V/arm-vita-eabi/lib $V/arm-vita-eabi/include &&
  cp *.a $V/arm-vita-eabi/lib/ && cp -r ../vita-headers/include/. $V/arm-vita-eabi/include/)

# 5. newlib
mkdir -p build-newlib && (cd build-newlib && env $TOOLS CC_FOR_TARGET=$B-gcc GCC_FOR_TARGET=$B-gcc \
  CXX_FOR_TARGET=$B-g++ ../newlib/configure "CFLAGS_FOR_TARGET=-g -O2 -ffunction-sections -fdata-sections" \
  --target=arm-vita-eabi --prefix=/ --with-build-sysroot=$V/arm-vita-eabi --enable-newlib-io-long-long \
  --enable-newlib-register-fini --disable-newlib-supplied-syscalls --enable-newlib-long-time_t --disable-nls \
  --enable-newlib-iconv --enable-newlib-mb && make -j$JOBS && make install DESTDIR=$V)

# 6. pthread-embedded
make -C pthread-embedded/platform/vita CC=$B-gcc CXX=$B-g++ AR=$B-ar GLOBAL_CFLAGS=-Wno-error=int-conversion PREFIX=$V
make -C pthread-embedded/platform/vita PREFIX=$V/arm-vita-eabi install

# 7. GCC target libraries (libgcc, libstdc++)
(cd build-gcc && env $TOOLS make -j$JOBS INHIBIT_LIBC_CFLAGS="-DUSE_TM_CLONE_REGISTRY=0" && make install)

# 8. libvita2d (core: no PNG/JPEG/freetype loaders needed)
(cd libvita2d/libvita2d && for f in vita2d vita2d_texture vita2d_draw utils vita2d_pgf vita2d_pvf \
    bin_packing_2d texture_atlas int_htab; do $B-gcc -O2 -Iinclude -c source/$f.c -o source/$f.o; done &&
  $B-ar rcs libvita2d.a source/vita2d.o source/vita2d_texture.o source/vita2d_draw.o source/utils.o \
    source/vita2d_pgf.o source/vita2d_pvf.o source/bin_packing_2d.o source/texture_atlas.o source/int_htab.o \
    shader/compiled/*_gxp.o && cp libvita2d.a $V/arm-vita-eabi/lib/ && cp include/vita2d.h $V/arm-vita-eabi/include/)
echo "VitaSDK ready in $V"
