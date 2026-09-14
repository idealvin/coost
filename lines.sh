#!/bin/bash

inc="include/co"
src="src"
echo -n "core: "
cat $inc/*.h  $src/*.{h,cc} $src/*/*.{h,cc} $src/co/epoll/*.{h,cc} | wc -l

echo -n "benchmark: "
cat benchmark/*.cc | wc -l

echo -n "test: "
cat test/*.cc test/*/*.cc | wc -l

echo -n "unitest: "
cat unitest/*.cc | wc -l

