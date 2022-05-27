# CMake generated Testfile for 
# Source directory: /mnt/d/work/code/redis-5.0.12-zk/trunk/redis-5.0.12/deps/zkcli/zookeeper-client-c
# Build directory: /mnt/d/work/code/redis-5.0.12-zk/trunk/redis-5.0.12/deps/zkcli/zookeeper-client-c
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(zktest_runner "/mnt/d/work/code/redis-5.0.12-zk/trunk/redis-5.0.12/deps/zkcli/zookeeper-client-c/zktest")
set_tests_properties(zktest_runner PROPERTIES  ENVIRONMENT "ZKROOT=/mnt/d/work/code/redis-5.0.12-zk/trunk/redis-5.0.12/deps/zkcli/zookeeper-client-c/../..;CLASSPATH=\$CLASSPATH:\$CLOVER_HOME/lib/clover*.jar" _BACKTRACE_TRIPLES "/mnt/d/work/code/redis-5.0.12-zk/trunk/redis-5.0.12/deps/zkcli/zookeeper-client-c/CMakeLists.txt;263;add_test;/mnt/d/work/code/redis-5.0.12-zk/trunk/redis-5.0.12/deps/zkcli/zookeeper-client-c/CMakeLists.txt;0;")
