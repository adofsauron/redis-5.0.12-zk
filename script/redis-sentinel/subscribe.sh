#!/bin/bash

echo redis-cli -p 7000 subscribe __sentinel__:hello

redis-cli -p 7000 subscribe __sentinel__:hello
