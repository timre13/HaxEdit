#!/bin/sh

wget https://raw.githubusercontent.com/first20hours/google-10000-english/master/google-10000-english.txt -O ./external/dictionary.txt

echo "\033[32mGetting submodules\033[0m"
git submodule update --init --recursive

# Compile ICU
cd ./external/icu/icu4c/source
echo "\033[32mConfiguring ICU\033[0m"
./runConfigureICU Linux
echo "\033[32mBuilding ICU\033[0m"
make -j `nproc`
