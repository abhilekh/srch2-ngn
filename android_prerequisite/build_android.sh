#!/bin/bash -
#===============================================================================
#
#          FILE: build_android.sh
#
#         USAGE: ./build_android.sh
#
#   DESCRIPTION:
#
#       OPTIONS: ---
#  REQUIREMENTS: ---
#          BUGS: ---
#         NOTES: ---
#        AUTHOR: Jianfeng Jia ()
#       CREATED: 08/20/2013 09:50:03 PM PDT
#      REVISION:  ---
#===============================================================================

set -o nounset                              # Treat unset variables as an error

export SOFT="$HOME/software"
export ANDROID_SDK_HOME="$SOFT/android-sdk"
export ANDROID_NDK_HOME="$SOFT/android-ndk"
export ANDROID_NDK_ROOT=$ANDROID_NDK_HOME
export ANDROID_CMAKE_HOME="$SOFT/android-cmake"
export ANDROID_STANDALONE_TOOLCHAIN="$SOFT/android-toolchain-arm"
export OPENSSL_HOME="$SOFT/openssl-1.0.2"
SCRIPT_PWD=$PWD

# download android-cmake
[ ! -d $ANDROID_CMAKE_HOME ] && git clone https://github.com/abhilekh/android-cmake.git $ANDROID_CMAKE_HOME

./install_sdk_ndk.sh
[ $? -ne 0 ] && { echo "install sdk and ndk error"; exit -1;}


# default arm:  --toolchain=arm-linux-androideabi-4.6
# set to x86:   --toolchain=x86-4.4.3 --arch=x86
# set to mips:  --toolchain=mipsel-linux-android-4.6 --arch=mips
# TODO: make it work for each chip set automaticlly
#       hint: also can be set using android-cmake by setting ANDROID_ABI
$ANDROID_NDK_HOME/build/tools/make-standalone-toolchain.sh --platform=android-8 \
        --install-dir=$ANDROID_STANDALONE_TOOLCHAIN --toolchain=arm-linux-androideabi-4.9

ANDTOOLCHAIN=$HOME/software/android-cmake/toolchain/android.toolchain.cmake
if grep -q $ANDTOOLCHAIN $HOME/.bashrc;
then
    echo "skip alias"
else
    echo "export ANDROID_STANDALONE_TOOLCHAIN=$ANDROID_STANDALONE_TOOLCHAIN" >> $HOME/.bashrc
    echo "alias android-cmake='cmake -DCMAKE_TOOLCHAIN_FILE=$ANDTOOLCHAIN -DLIBRARY_OUTPUT_PATH_ROOT=.'" >> $HOME/.bashrc
fi

#Install Boost
sudo apt-get install libboost-dev
if [ `ls -l $ANDROID_STANDALONE_TOOLCHAIN/user/lib/libboost_*.a | wc -l` -lt 9 ];then
    rm -f $ANDROID_STANDALONE_TOOLCHAIN/user/lib/libboost_*.so #Force to rm shared library
    cd $ANDROID_CMAKE_HOME/common-libs/boost
    ./get_boost.sh
    mv CMakeLists.txt CMakeLists.txt.orig
    cp $SCRIPT_PWD/settings/CMakeLists.Boost_add_serialization.txt CMakeLists.txt
    mkdir build
    cd build_android
    cmake -DCMAKE_TOOLCHAIN_FILE=$ANDTOOLCHAIN ..
    make -j3
    make install
    cd $SCRIPT_PWD
else
    echo "Boost already installed"
fi

#Install open-ssl
if [ ! -d $OPENSSL_HOME ];then
    git clone https://github.com/guardianproject/openssl-android.git $OPENSSL_HOME
    cd $OPENSSL_HOME
    # Change the version of 4.* into the exist one by checking
    # $ANDROID_NDK_HOME/toolchain/*-androideabi-4.*
    $ANDROID_NDK_HOME/ndk-build NDK_TOOLCHAIN_VERSION=4.9
    cp -r armeabi-v7a/lib/* $ANDROID_STANDALONE_TOOLCHAIN/lib/
    cp -r include/*.h   $ANDROID_STANDALONE_TOOLCHAIN/include/
fi


# if [ ! -d $OPENSSL_HOME ];then
#     sudo -u $USER git clone https://github.com/guardianproject/openssl-android.git $OPENSSL_HOME
#     cd $OPENSSL_HOME
#     sudo -u $USER wget https://wiki.openssl.org/images/7/70/Setenv-android.sh
#     # Change the version of 4.* into the exist one by checking
#     # $ANDROID_NDK_HOME/toolchain/*-androideabi-4.*
#     $ANDROID_NDK_HOME/ndk-build NDK_TOOLCHAIN_VERSION=4.9
#     export ANDROID_NDK_ROOT=$ANDROID_NDK_HOME
#     export _ANDROID_EABI="arm-linux-androideabi-4.9"
#     sudo -u $USER bash ./Setenv-android.sh
#     sudo -u $USER perl -pi -e 's/install: all install_docs install_sw/install: install_docs install_sw/g' Makefile.org
#     sudo -u $USER . ./config shared no-ssl2 no-ssl3 no-comp no-hw no-engine --openssldir=/usr/local/ssl/android-14/

#     sudo -u $USER cp -r libs/armeabi/*.so $ANDROID_STANDALONE_TOOLCHAIN/lib/
#     sudo -u $USER cp -r include/openssl   $ANDROID_STANDALONE_TOOLCHAIN/include/
#     cd SCRIPT_PWD
# fi
