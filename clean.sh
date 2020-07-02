#!/bin/bash

if [[ "$OSTYPE" == "msys" ]]; then 
    echo "don't run this on windows.."
    exit 0
fi

cd "$(dirname "$0")"

src=`ls include/co/*.h include/co/*/*.h */*.cc */*/*.{h,cc,cpp} */*/*/*.{h,cc,cpp}`

for x in $src
do
    if [ -f $x ]; then
        sed -i 's/[ ]*$//g' $x
    fi
done
