#!/bin/bash -e

if [[ "$OSTYPE" == "msys" ]]; then 
    echo "don't run this on windows.."
    exit 0
fi

cd "$(dirname "$0")"

rm -rf ../inc
mkdir -p ../inc/base
cp *.h ../inc/base/
cp ../LICENSE ../inc/base/

dirs='co hash unix win'

for x in $dirs
do
    mkdir ../inc/base/$x
    cp $x/*.h ../inc/base/$x
done

mkdir ../inc/base/stack_trace
cp stack_trace/stack_trace.h ../inc/base/stack_trace/
