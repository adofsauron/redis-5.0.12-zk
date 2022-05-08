#!/bin/bash

redis-cli -p 7001 cluster nodes

ps -ef | grep redis