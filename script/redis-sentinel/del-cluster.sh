#!/bin/bash

# 7009: slave
# 7008：master

# 1.先删除slave节点

echo `date` "-----------del slave"

GID_SLAVE=`redis-cli  -p 7000 cluster nodes | grep 7009 | awk -F ' ' '{ print $1 }'`

echo `date` $GID_SLAVE

redis-cli --cluster del-node 127.0.0.1:7009 $GID_SLAVE

echo `date` $?

sleep 1

echo `date` "-----------remove master slots"

GID_FROM=`redis-cli  -p 7000 cluster nodes | grep 7008 | awk -F ' ' '{ print $1 }'`
GID_TO=`redis-cli  -p 7000 cluster nodes | grep 7002 | awk -F ' ' '{ print $1 }'`

echo `date` $GID_FROM
echo `date` $GID_TO

# 2.将master节点的数据迁出
redis-cli --cluster reshard 127.0.0.1:7000 \
    --cluster-from  $GID_FROM \
    --cluster-to    $GID_TO \
    --cluster-slots 16384 \
    --cluster-yes

echo `date` $?

sleep 1

echo `date` "-----------del master"

# 3.删除master节点
redis-cli --cluster del-node 127.0.0.1:7008 $GID_FROM

echo `date` $?

