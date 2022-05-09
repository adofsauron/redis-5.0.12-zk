#!/bin/bash

redis-cli --cluster rebalance 127.0.0.1:7000 \
    --cluster-weight 1 \
    --cluster-use-empty-masters

echo `date` $?