#!/bin/bash

alias redis-server="/bin/redis-server"

pkill redis-server

sleep 1

cd 7000
rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
nohup redis-server ./redis.conf & > /dev/null
cd ..

cd 7001
rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
nohup redis-server ./redis.conf & > /dev/null
cd ..

# cd 7002
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

# cd 7003
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

# cd 7004
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

# cd 7005
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

# cd 7006
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

# cd 7007
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

# cd 7008
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

# cd 7009
# rm appendonly.aof dump.rdb nodes.conf redis.log nohup.out -f
# nohup redis-server ./redis.conf & > /dev/null
# cd ..

exit 0
