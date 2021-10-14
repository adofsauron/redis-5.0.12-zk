#!/bin/bash

HERE=`pwd`

cd zookeeper-client-c

rm -rvf CMakeCache.txt
rm -rvf CMakeFiles

cmake CMakeLists.txt

make clean
make -j"$(nproc)" zookeeper

cd $HERE

echo `date` rm -vf ./*.a
rm -vf ./*.a

echo `date` cp -vf zookeeper-client-c/libzookeeper.a  ./
cp -vf zookeeper-client-c/libzookeeper.a  ./

echo `date` cp -vf zookeeper-client-c/libhashtable.a  ./
cp -vf zookeeper-client-c/libhashtable.a  ./


