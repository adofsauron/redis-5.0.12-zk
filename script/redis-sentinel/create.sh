#!/bin/bash

# 4 cluster
# redis-cli --cluster create   \
#         --cluster-replicas 1 \
#         --cluster-yes        \
#         127.0.0.1:7000 \
#         127.0.0.1:7001 \
#         127.0.0.1:7002 \
#         127.0.0.1:7003 \
#         127.0.0.1:7004 \
#         127.0.0.1:7005 \
#         127.0.0.1:7006 \
#         127.0.0.1:7007

# 2 cluster
# redis-cli --cluster create   \
#         --cluster-replicas 1 \
#         --cluster-yes        \
#         127.0.0.1:7000 \
#         127.0.0.1:7001 \
#         127.0.0.1:7002 \
#         127.0.0.1:7003
        
# 1 cluster
redis-cli --cluster create   \
        --cluster-replicas 1 \
        --cluster-yes        \
        127.0.0.1:7000 \
        127.0.0.1:7001
        
echo `date` $?
