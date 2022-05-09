#!/bin/bash


redis-cli --cluster reshard 127.0.0.1:7000 \
    --cluster-from  b8a0677b57df2752c716cbc9a71df306928ebecf \
    --cluster-to    1cfd18da0560e845f776cbb4abd5868c839f209b \
    --cluster-slots 10 \
    --cluster-yes

echo `date` $?
