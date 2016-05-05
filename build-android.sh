#!/bin/bash

sudo apt-get install git
sudo apt-get install g++
sudo apt-get install cmake
sudo apt-get install ant


# Download for arm architecture

cd ./android_prerequisite
bash ./setup_android_env.sh
read -s -p "Info: Setup Done, Press Key To Continue\n" -n1 key
cd ..
source .exportvars
rm -rf build/android
mkdir -p build/android
cd build/android
android-cmake ../../
read -s -p "Info: Setup Done, Press Key To Continue\n" -n1 key
cd ../../thirdparty
./thirdparty-build-android.sh
# cp json/jsoncpp-src/libs/linux-gcc-4.8/* $ANDROID_STANDALONE_TOOLCHAIN/user/lib/
read -s -p "Info: Thirdparty Toolset done, Press Key To Continue\n" -n1 key
cd ../build/android
android-cmake ../../ -DANDROID_SERVER=1
make -j4
read -s -p "Info: Server Made, Press Key To Continue\n" -n1 key
rm CMakeCache.txt
android-cmake ../../ -DBUILD_SERVER=NO
read -s -p "Info So Made, Press Key To Continue\n" -n1 key
make -j4

