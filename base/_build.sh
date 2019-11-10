#!/bin/bash -e

cd "$(dirname "$0")"

if [[ ! -d "../lib" ]];then
    mkdir ../lib
fi

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
    AR="ar"
    OUT="libbase.a"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="mac"
    CC="clang++"
    CCFLAGS="-std=c++11"
    AR="ar"
    OUT="libbase.a"
elif [[ "$OSTYPE" == "msys" ]]; then
    OS="win"
    CC="clang++"
    CCFLAGS="-Xclang -flto-visibility-public-std -Wno-deprecated"
    AR="llvm-ar"
    OUT="base.lib"
else
    OS="oth"
    CC="g++"
    CCFLAGS="-std=c++11"
    AR="ar"
    OUT="libbase.a"
fi

$CC $CCFLAGS -O2 -g3 -c *.cc */*.cc co/impl/*.cc co/context/context.S

if [[ ! -d "../lib/$OS/$PLAT" ]]; then
    mkdir -p ../lib/$OS/$PLAT
fi

$AR rc ../lib/$OS/$PLAT/$OUT *.o
rm -rf *.o

echo lib/$OS/$PLAT/$OUT created ok.

exit 0
