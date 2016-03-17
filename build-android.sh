#!/bin/bash

# Download for armarchitecture

cd ./android_prerequisite
./build_android.sh
read -s -p "Press Key To Continue\n" -n1 key
cd ..
source .exportvars
myDir=$PWD
cd $OPENSSL_HOME
$ANDROID_NDK_HOME/ndk-build NDK_TOOLCHAIN_VERSION=4.9
cp -r armeabi-v7a/lib/* $ANDROID_STANDALONE_TOOLCHAIN/lib/
cp -r include/*   $ANDROID_STANDALONE_TOOLCHAIN/include/
cd $myDir
rm -rf build/android
mkdir -p build/android
cd build/android
android-cmake ../../
read -s -p "Press Key To Continue\n" -n1 key
cd ../../thirdparty
./thirdparty-build-android.sh
read -s -p "Press Key To Continue\n" -n1 key
cd ../build/android
android-cmake ../../ -DANDROID_SERVER=1
read -s -p "Press Key To Continue\n" -n1 key
make -j4
