#!/bin/bash
xmake project -k vs2015 -m "debug;release" vs
xmake project -k vs2017 -m "debug;release" vs
xmake project -k vs2019 -m "debug;release" vs
