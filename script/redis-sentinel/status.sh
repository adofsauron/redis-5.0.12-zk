#!/bin/bash

echo redis-cli -p 7001 cluster nodes
redis-cli -p 7001 cluster nodes

echo -e "\n"

echo redis-cli -p 26379 info Sentinel
redis-cli -p 26379 info Sentinel

echo -e "\n"

echo "ps -ef | grep redis | grep -v grep"
ps -ef | grep redis | grep -v grep

