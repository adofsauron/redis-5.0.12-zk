#!/bin/bash


HERE=`pwd`


REDIS_DIR=redis-5.0.12

cd $REDIS_DIR

# zkcli

cd deps/zkcli
dos2unix ./*
bash build.sh

cd -

# tcmalloc

cd deps/gperftools-2.9.1
dos2unix ./*
find . -type f -exec sed -i 's/\r//' {} \;
bash ./configure --enable-frame-pointers --enable-static=yes --prefix=/usr
make clean
make -j"$(nproc)"
make install

if [ ! -f "/lib64/libtcmalloc.so.4" ]; then
    ln -s /usr/lib/libtcmalloc.so.4 /lib64/libtcmalloc.so.4
fi

cd -

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

# deps

cd deps

make distclean
make hiredis
make linenoise
make lua

cd -

# redis-server

ARCH=`uname -m`
echo `date` $ARCH

malloc="tcmalloc"
echo `date` $malloc

echo `date` make -j"$(nproc)" MALLOC=$malloc redis-server
make -j"$(nproc)" MALLOC=$malloc redis-server

echo `date` make -j"$(nproc)" MALLOC=$malloc redis-sentinel
make -j"$(nproc)" MALLOC=$malloc redis-sentinel

echo `date` make -j"$(nproc)" MALLOC=$malloc redis-cli
make -j"$(nproc)" MALLOC=$malloc redis-cli

echo `date` make -j"$(nproc)" MALLOC=$malloc redis-check-aof
make -j"$(nproc)" MALLOC=$malloc redis-check-aof

echo `date` make -j"$(nproc)" MALLOC=$malloc redis-check-rdb
make -j"$(nproc)" MALLOC=$malloc redis-check-rdb

echo `date` cd $HERE
cd $HERE

# here

echo `date` rm ./redis-server -f
rm ./redis-server -f

echo `date` rm ./redis-sentinel -f
rm ./redis-sentinel -f

echo `date` rm ./redis-cli -f
rm ./redis-cli -f

echo `date` rm ./redis-check-aof -f
rm ./redis-check-aof -f

echo `date` rm ./redis-check-rdb -f
rm ./redis-check-rdb -f

echo `date` cp ./$REDIS_DIR/src/redis-server ./ -f
cp ./$REDIS_DIR/src/redis-server ./ -f

echo `date` cp ./$REDIS_DIR/src/redis-sentinel ./ -f
cp ./$REDIS_DIR/src/redis-sentinel ./ -f

echo `date` cp ./$REDIS_DIR/src/redis-cli ./ -f
cp ./$REDIS_DIR/src/redis-cli ./ -f

echo `date` cp ./$REDIS_DIR/src/redis-check-aof ./ -f
cp ./$REDIS_DIR/src/redis-check-aof ./ -f

echo `date` cp ./$REDIS_DIR/src/redis-check-rdb ./ -f
cp ./$REDIS_DIR/src/redis-check-rdb ./ -f

# /usr/bin

echo `date` cp ./redis-check-aof /usr/bin/ -f
cp ./redis-check-aof /usr/bin/ -f

echo `date` cp ./redis-check-rdb /usr/bin/ -f
cp ./redis-check-rdb /usr/bin/ -f

echo `date` cp ./redis-server /usr/bin/ -f
cp ./redis-server /usr/bin/ -f

echo `date` cp ./redis-sentinel /usr/bin/ -f
cp ./redis-sentinel /usr/bin/ -f

echo `date` cp ./redis-cli /usr/bin/ -f
cp ./redis-cli /usr/bin/ -f
