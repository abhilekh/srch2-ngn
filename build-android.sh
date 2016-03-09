#!/bin/bash

cd ./android_prerequisite
./build_android.sh
read -s -n 1 key
cd ..
source .exportvars
rm -rf build/android
mkdir -p build/android
cd build/android
android-cmake ../../
read -s -n 1 key
cd ../../thirdparty
./thirdparty-build-android.sh
read -s -n 1 key
cd ../build/android
android-cmake ../../ -DANDROID_SERVER=1
read -s -n 1 key
make -j4
