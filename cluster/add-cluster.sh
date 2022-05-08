#!/bin/bash

## 注意,只有第一次需要执行以下命令

# 已有节点的端口
HAS_PORT=7000
# 新的master节点的端口
N_MASTER_PORT=7008
# 新的slave节点的端口
N_SLAVE_PORT=7009

# 1.添加master节点。 7000为原有节点
redis-cli --cluster add-node 127.0.0.1:7008 127.0.0.1:7000

echo `date` $?

sleep 1

# 2.拿到新增加的端口为7008的节点的id, 注意awk以空格为分隔符
GID=`redis-cli  -p 7000 cluster nodes | grep 7008 | awk -F ' ' '{ print $1 }'`

echo `date` $GID

# 3. 添加slave从节点, 7009为slave, 7008为master
redis-cli --cluster add-node 127.0.0.1:7009 127.0.0.1:7008 \
    --cluster-slave \
    --cluster-master-id $GID

echo `date` $?

sleep 1

# 4.自动分配slots

redis-cli --cluster rebalance 127.0.0.1:7000 \
    --cluster-weight 1 \
    --cluster-use-empty-masters

echo `date` $?
