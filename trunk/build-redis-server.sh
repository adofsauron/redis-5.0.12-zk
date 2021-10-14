#!/bin/bash

echo `date` rm -vf ./redis-cli
rm -vf ./redis-cli

echo `date` rm -vf ./redis-server
rm -vf ./redis-server


HERE=`pwd`

cd redis-5.0.12

# other

dos2unix src/mkreleasehdr.sh
dos2unix deps/update-jemalloc.sh

chmod +x src/mkreleasehdr.sh
chmod +x runtest*
chmod +x deps/update-jemalloc.sh
chmod +x deps/jemalloc/*

echo `date` make clean
make clean

rm ./src/*.o -f
rm ./src/deps/jemalloc/src/*.o -f
rm ./src/deps/hiredis/*.o -f
rm ./src/deps/hiredis/libhiredis.a -f
rm ./src/deps/lua/src/*.o -f
rm ./src/deps/lua/src/liblua.a -f
rm ./src/deps/linenoise/*.o -f

ARCH=`uname -m`
echo `date` $ARCH

malloc="tcmalloc"
echo `date` $malloc

echo `date` make -j"$(nproc)" MALLOC=$malloc redis-server
make -j"$(nproc)" MALLOC=$malloc redis-server

echo `date` make -j"$(nproc)" MALLOC=$malloc redis-cli
make -j"$(nproc)" MALLOC=$malloc redis-cli

echo `date` cd $HERE
cd $HERE

echo `date` rm ./redis-server -f
rm ./redis-server -f

echo `date` rm ./redis-cli -f
rm ./redis-cli -f

echo `date` cp ./redis-5.0.12/src/redis-server ./ -f
cp ./redis-5.0.12/src/redis-server ./ -f

echo `date` cp ./redis-5.0.12/src/redis-cli ./ -f
cp ./redis-5.0.12/src/redis-cli ./ -f


echo `date` cp ./redis-server /usr/bin/ -f
cp ./redis-server /usr/bin/ -f

echo `date` cp ./redis-cli /usr/bin/ -f
cp ./redis-cli /usr/bin/ -f
