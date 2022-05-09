#!/bin/bash

cd 7000
nohup redis-server ./redis.conf  > /dev/null &
cd ..

cd 7001
nohup redis-server ./redis.conf  > /dev/null &
cd ..