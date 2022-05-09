#!/bin/bash

pkill redis-sentinel

sleep 2

nohup redis-sentinel ./sentinel/sentinel-0.conf > ./sentinel/sentinel-0.log & > /dev/null

nohup redis-sentinel ./sentinel/sentinel-1.conf > ./sentinel/sentinel-1.log & > /dev/null

nohup redis-sentinel ./sentinel/sentinel-2.conf > ./sentinel/sentinel-2.log & > /dev/null

