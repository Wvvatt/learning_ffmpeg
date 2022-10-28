#!/bin/bash

./configure \
    --prefix=../build \
    --enable-shared --disable-static --enable-gpl --enable-version3 --enable-pic --enable-nonfree --disable-doc \
    --enable-debug --disable-optimizations --disable-asm --disable-stripping \
    --enable-libx264 

make clean && make -j&{nproc} && make install