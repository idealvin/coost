#!/bin/bash -e

cd "$(dirname "$0")"

if [[ -z "$1" ]]; then
    echo usage:
    echo "    ./build.sh xxx.cc"
    exit 0
fi

src="$1"
sub=""
OUT="${src/.cc/$sub}"

platform=`uname -m`

if [[ "$platform" == "x86_64" ]]; then
    PLAT="x64"
else
    PLAT="x86"
fi

if [[ "$OSTYPE" == "linux-gnu" ]]; then
    OS="linux"
    CC="g++"
    CCFLAGS="-std=c++11"
	LIBS="-lpthread"
    LIBBASE="libbase.a"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="mac"
    CC="clang++"
    CCFLAGS="-std=c++11"
    LIBBASE="libbase.a"
elif [[ "$OSTYPE" == "msys" ]]; then
    OS="win"
    CC="clang++"
    CCFLAGS="-Xclang -flto-visibility-public-std -Wno-deprecated"
    LIBBASE="base.lib"
else
    OS="oth"
    CC="g++"
    CCFLAGS="-std=c++11"
	LIBS="-lpthread"
    LIBBASE="libbase.a"
fi

$CC $CCFLAGS -O2 -o ../build/$OUT $src ../lib/$LIBBASE -I.. $LIBS -ldl
echo create build/$OUT ok

exit 0
