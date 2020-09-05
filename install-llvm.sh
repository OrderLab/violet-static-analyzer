#!/bin/bash

log=$(pwd)/install-llvm.log
> $log  # clear log first

function print_log() {
  echo "$1" | tee -a $log
}

function err_and_exit() {
  echo "$1" >&2
  exit 1
}

function color_make() {
  if hash unbuffer 2>/dev/null; then
    unbuffer make "$@" 2>&1 | tee -a $log
  else
    make "$@" 2>&1 | tee -a $log
  fi
}

function clone_source() {
  local source_url=$1
  local dest_dir=$2
  if [ ! -z "$(ls -A $dest_dir)" ]; then
    print_log "Skip cloning $source_url as $dest_dir is non-empty"
    return
  fi
  print_log "Cloning $source_url into $dest_dir..."
  svn export --force $source_url ${dest_dir} || err_and_exit "Failed"
  print_log "Done"
}

os="`uname`"
if [ $os == "Linux" ]; then
  num_cores=$(nproc)
elif [ $os == "Darwin" ]; then
  num_cores=$(sysctl -n hw.ncpu)
else
  num_cores=1
fi
target_dir=./
re_ver="^[0-9\.]+$"

if [ $# -lt 2 ] || ! [[ "$1" =~ ${re_ver} ]] || ! [ -d "$2" ]; then
	echo "usage: $0 <version> <directory> [<binutils_dir>]" >&2
	echo -e "\n\nexample: \n\t$0 5.0.1 /data/share/software/llvm" >&2
	echo -e "\t$0 5.0.1 /data/share/software/llvm /data/share/software/binutils/dist" >&2
	exit 1
fi
use_existing_binutils=0
if [ $# -eq 3 ] ; then
  use_existing_binutils=1
  binutils_dir=$(cd $3 && pwd)
  if [ ! -d ${binutils_dir}/include ]; then
    echo "${binutils_dir}/include does not exist" >&2
    exit 1
  fi
fi

this_script_dir=$(cd $(dirname "${BASH_SOURCE[0]}") >/dev/null 2>&1 && pwd)
version=$1
major_release=${version%%.*}
release_tag="RELEASE_${version//.}"
target_dir=$(cd $2 && pwd)
src_dir="${target_dir}/${version}/src"
build_dir="${target_dir}/${version}/build"
dist_dir="${target_dir}/${version}/dist"
mkdir -p ${target_dir}/${version}/{src,build,dist}

echo "Getting the complete LLVM source code"
echo "Get llvm"
clone_source http://llvm.org/svn/llvm-project/llvm/tags/${release_tag}/final/ ${src_dir}
cd ${src_dir}/tools
echo "Get clang"
clone_source http://llvm.org/svn/llvm-project/cfe/tags/${release_tag}/final/ clang
cd clang/tools
echo "Get clang-tools-extra"
clone_source http://llvm.org/svn/llvm-project/clang-tools-extra/tags/${release_tag}/final/ extra
cd ../..
echo "Get lld"
clone_source http://llvm.org/svn/llvm-project/lld/tags/${release_tag}/final/ lld
echo "Get polly"
clone_source http://llvm.org/svn/llvm-project/polly/tags/${release_tag}/final/ polly
cd ../projects
echo "Get compiler-rt"
clone_source http://llvm.org/svn/llvm-project/compiler-rt/tags/${release_tag}/final compiler-rt
echo "Get openmp"
clone_source http://llvm.org/svn/llvm-project/openmp/tags/${release_tag}/final openmp
echo "Get libcxx"
clone_source http://llvm.org/svn/llvm-project/libcxx/tags/${release_tag}/final libcxx
echo "Get libcxxabi"
clone_source http://llvm.org/svn/llvm-project/libcxxabi/tags/${release_tag}/final libcxxabi
echo "Get test-suite"
clone_source http://llvm.org/svn/llvm-project/test-suite/tags/${release_tag}/final test-suite
if [ $use_existing_binutils -eq 0 ]; then
  mkdir -p ${target_dir}/binutils/{src,build,dist}
  binutils_dir=${target_dir}/binutils/dist
  cd ${target_dir}/binutils
  if [ ! -z "$(ls -A dist/include)" ]; then
    print_log "Binutils already built, using it"
  else
    if [ -z "$(ls -A src)" ]; then
      print_log "Get the source code of binutils"
      git clone --depth 1 git://sourceware.org/git/binutils-gdb.git src
    fi
    cd build
    print_log "Build binutils..."
    ../src/configure --prefix=$binutils_dir --enable-gold --enable-plugins --disable-werror
    color_make -j${num_cores} all-gold || err_and_exit "failed to build binutils"
    color_make -j${num_cores} || err_and_exit "failed to build binutils"
    make install
  fi
fi
print_log "LLVM source code and plugins are set up"

if [ $major_release -lt 5 ]; then
  # only for LLVM version below 5.x
  glibc_version=$(ldd --version | grep GLIBC | cut -d' ' -f 5)
  glibc_minor=${glibc_version##*.}
  if [ $glibc_minor -gt 23 ]; then
    print_log "glibc > 23, the compiler-rt build will likely fail, patching its source..."
    current_dir=$(pwd)
    cd ${src_dir}/projects/compiler-rt
    for p in `ls ${this_script_dir}/patches/compiler-rt-${version}-*.patch`;
    do
      print_log "Applying patch $p..."
      echo git apply $p
    done
  fi
fi

echo "Build the LLVM project"
cd ${build_dir} || err_and_exit "failed to go to ${build_dir}" 
if [ $major_release -lt 5 ]; then
  cmake_flags="-DLLVM_ENABLE_CXX1Y=OFF"
else
  cmake_flags="-DLLVM_ENABLE_CXX1Y=ON"
fi
cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=${dist_dir} $cmake_flags -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_BINUTILS_INCDIR=${binutils_dir}/include ../src
color_make -j${num_cores} 
if [ $? -ne 0 ]; then
  print_log "Failed to build LLVM"
  print_log "If it's related to missing xlocale.h, run the following command:"
  print_log "sudo ln -s /usr/include/locale.h /usr/include/xlocale.h"
  exit 1
fi
print_log "Installing LLVM"
make install
print_log "Successfully installed LLVM"
