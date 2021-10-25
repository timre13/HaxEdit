#!/bin/sh

echo "\033[32mGetting submodules\033[0m"
git submodule update --recursive

# Compile ICU
cd external/icu/icu4c/source
echo "\033[32mConfiguring ICU\033[0m"
./runConfigureICU Linux
echo "\033[32mBuilding ICU\033[0m"
make -j `nproc`
