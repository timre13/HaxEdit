#!/bin/sh

wget https://raw.githubusercontent.com/first20hours/google-10000-english/master/google-10000-english.txt -O ./external/dictionary.txt

echo "\033[32mGetting submodules\033[0m"
git submodule update --init --recursive

cd external/LspCpp
mkdir build
cd build
cmake -DUri_BUILD_TESTS=OFF ..
make -j`nproc`
