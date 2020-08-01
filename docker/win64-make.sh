#!/bin/bash
make -C bsnes -j8 local=false build=performance platform=windows compiler="x86_64-w64-mingw32-g++-posix" windres="x86_64-w64-mingw32-windres"
