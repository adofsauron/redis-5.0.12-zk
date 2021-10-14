#include "server.h"
#include "cluster.h"
#include "zmalloc.h"
#include "zklink.h"

#include <string.h>

static void zklinkWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
static void zklinkCompletion(int rc, const char* value, int value_len, const struct Stat* stat, const void* data);

static zklinkClient* zklinkCreateClient(struct redisServer* server);
static void zklinkInitServer(zklinkClient* zkc);
static void zklinkUpdte(zklinkClient* zkc);
static void zklinkCreatePath(zklinkClient* zkc);
static void zklinkWatchPath(zklinkClient* zkc);
static void zklinkUpdateStateOnWatch(zklinkClient* zkc, const char* path, int type);
static void zklinkDelOldMasterPath(zklinkClient* zkc);

int zklinkTryCreateOldMasterPath(struct redisServer* server)
{
    clusterNode* myself = server->cluster->myself;
    if (!nodeIsSlave(myself)) {
        serverLog(LL_WARNING, "zookeeper, zklinkTryCreateOldMasterPath fail, myself not slave");
        return 0;
    }

    clusterNode* master = myself->slaveof;
    if (!master) {
        serverLog(LL_WARNING, "zookeeper, zklinkTryCreateOldMasterPath fail, no master");
        return 0;
    }

    zklinkClient* zkc = server->zklc;
    if (!zkc->replace_path) {
        serverLog(LL_WARNING, "zookeeper, zklinkTryCreateOldMasterPath fail, not watch master");
        return 0;
    }

    // master changed
    if (strncmp(zkc->replace_path, master->name, CLUSTER_NAMELEN)) {
        serverLog(LL_WARNING, "zookeeper, zklinkTryCreateOldMasterPath fail, master change, replace_path = [%s], master->name = [%s]", zkc->replace_path, master->name);
        return 0;
    }

    char path[CLUSTER_NAMELEN+9] = {0};
    snprintf(path, CLUSTER_NAMELEN+9 , "/replace-%s", zkc->replace_path);

    char data[CLUSTER_NAMELEN] = {0};
    snprintf(data, CLUSTER_NAMELEN , "%s", myself->name);

    int rc = zoo_create(zkc->zh, (const char*) path, data,
        CLUSTER_NAMELEN, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0);

    // create by other slave
    if (ZNODEEXISTS == rc) {
        serverLog(LL_VERBOSE, "zookeeper, zoo_create replace_path fail, ZNODEEXISTS, path = [%s], myself not be master", path);
        return 0;
    }

    if (ZOK != rc) {
        serverLog(LL_WARNING, "zookeeper, zoo_create replace_path fail, path = [%s], rc = [%d]", path, rc);
        return 0;
    }

    zkc->replace_path_del_tm = mstime() + ZK_DEL_REPLACE_PATH_DELAY;

    serverLog(LL_VERBOSE, "zookeeper, zoo_create replace_path success, path = [%s], myself will be master, replace path will be del on [%lld]", 
        path, zkc->replace_path_del_tm);

    return 1;
}

void zklinkUpdateStateOnWatch(zklinkClient* zkc, const char* path, int type)
{
    serverLog(LL_VERBOSE, "zookeeper, Watcher zklinkUpdateStateOnWatch, path = [%s], type = [%d]", path, type);

    struct redisServer* server = zkc->redis_server;
    clusterNode* myself = server->cluster->myself;

    if (nodeIsMaster(myself)) {
        return;
    }

    clusterNode* master = myself->slaveof;
    if (!master) {
        return;
    }

    char master_path[CLUSTER_NAMELEN+1] = {0};
    snprintf(master_path, CLUSTER_NAMELEN+1 , "/%s", master->name);

    if (strncmp(path, master_path, CLUSTER_NAMELEN+1)) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher strncmp not equal, path = [%s], master->name = [%s]", path, master_path);
        return;
    }

    zkc->zk_on_wath = 0; // watch触发后需要继续添加

    if (ZOO_CREATED_EVENT == type) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher type = ZOO_CREATED_EVENT, master path = [%s] be created", path);
        // TODO:

        return;
    } 
    
    if (ZOO_DELETED_EVENT == type) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher type = ZOO_DELETED_EVENT, master path = [%s] be deleted, mark master = [%s] as fail", path, master->name);

        master->flags &= ~CLUSTER_NODE_PFAIL;
        master->flags |= CLUSTER_NODE_FAIL;
        clusterSendFail(master->name);
        return;
    }

    serverLog(LL_VERBOSE, "zookeeper, Watcher, zklinkUpdateStateOnWatch don't process, path = [%s], type = [%d]", path, type);
}

void zklinkWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    serverLog(LL_VERBOSE, "zookeeper, Watcher, type=[%d] state=[%d] path=[%s]\n", type, state, path);

    zklinkClient* zkc = (zklinkClient*) (watcherCtx);
    struct redisServer* server = zkc->redis_server;

    if ((ZOO_SESSION_EVENT == type) && (ZOO_CONNECTING_STATE == state) && ( (!path) || (0 == strlen(path)))) {
        zkc->zkstate = ZKS_DIS_CONNECT;
        zkc->zk_create_path = 0;
        zkc->zk_on_wath = 0;

        serverLog(LL_WARNING, "zookeeper, Watcher broken conn, disconnect to zk server[%s:%d], type = ZOO_SESSION_EVENT, state = ZOO_CONNECTING_STATE, path = NULL",
            server->zk_host, server->zk_port);
        return;
    }

    if (ZOO_CONNECTED_STATE != state) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher, zklinkWatcher type [%d] state [%d] not ZOO_CONNECTED_STATE, don't process, path = [%s]", type, state, path);
        return;
    }

    if (ZOO_SESSION_EVENT == type) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher, connect to zk server = [%s:%d] over", zkc->redis_server->zk_host, zkc->redis_server->zk_port);
        zkc->zkstate = ZKS_CONNECTED;
        return;
    }

    zklinkUpdateStateOnWatch(zkc, path, type);
    return;
}

void zklinkCompletion(int rc, const char* value, int value_len, const struct Stat* stat, const void* data)
{
    serverLog(LL_DEBUG, "zookeeper, -------completion... value=[%s] value_len=[%d]\n", value, value_len);
}

zklinkClient* zklinkCreateClient(struct redisServer* server)
{
    zklinkClient* zkc = zmalloc(sizeof(*zkc));

    zkc->zh = NULL;
    zkc->zkstate = ZKS_UNINIT;
    zkc->redis_server = server;
    zkc->zk_create_path = 0;
    zkc->zk_on_wath = 0;
    zkc->replace_path = NULL;
    zkc->replace_path_del_tm = 0;

    return zkc;
}

void zklinkInitServer(zklinkClient* zkc)
{
    if (zkc->zh) {
        zookeeper_close(zkc->zh);
        zkc->zh = NULL;
    }

    struct redisServer* server = zkc->redis_server;

    char addr[256] = {0};
    snprintf(addr, 256, "%s:%d", (const char*) server->zk_host, server->zk_port);

    zhandle_t* zh = zookeeper_init(addr,zklinkWatcher,server->zk_recv_timeout,NULL,zkc,0);
    if (!zh) {
        serverLog(LL_WARNING, "zookeeper, zookeeper_init fail, addr = [%s]", addr);
        return;
    }

    zkc->zh = zh;
    zkc->zkstate = ZKS_WAIT_CONNECT;

    serverLog(LL_VERBOSE, "zookeeper, zookeeper_init, wait connect over, addr = [%s]", addr);
}

void zklinkCron(struct redisServer* server)
{
    if ((!server->zk_host) || (server->zk_port <= 0)) {
        return;
    }

    if (!server->zklc) {
        server->zklc = zklinkCreateClient(server);
        return;
    }

    if (!server->zklc) {
        return;
    }

    zklinkClient* zkc = server->zklc;

    if (ZKS_WAIT_CONNECT == zkc->zkstate) {
        return;
    }

    if (ZKS_UNINIT == zkc->zkstate) {
        zklinkInitServer(zkc);
        return;
    }

    if (ZKS_DIS_CONNECT == zkc->zkstate) {
        zklinkInitServer(zkc);
        return;
    }

    if (ZKS_CONNECTED != zkc->zkstate) {
        return;
    }
    
    zklinkUpdte(zkc);
}

void zklinkCreatePath(zklinkClient* zkc)
{
    struct redisServer* server = zkc->redis_server;
    clusterNode *myself = server->cluster->myself;

    char path[CLUSTER_NAMELEN+1] = {0};
    snprintf(path, CLUSTER_NAMELEN+1 , "/%s", myself->name);

    if (nodeIsSlave(myself)) {
        if (0 == zkc->zk_create_path) {
            return;
        }

        int rc = zoo_exists(zkc->zh, path, 0, NULL);
        if (ZNONODE == rc) {
            zkc->zk_create_path = 0;
            return;
        }

        if (ZOK != rc) {
            serverLog(LL_WARNING, "zookeeper, node is slave, zoo_exists fail, path = [%s] rc = [%d]", path, rc);
            return;
        }

        rc = zoo_delete(zkc->zh, path, -1);
        if (ZNONODE == rc) {
            zkc->zk_create_path = 0;
            return;
        }

        if (ZOK != rc) { 
            serverLog(LL_WARNING, "zookeeper, node is slave, del zk path fail, path = [%s] rc = [%d]", path, rc);
            return;
        }

        zkc->zk_create_path = 0;
        serverLog(LL_VERBOSE, "zookeeper, node is slave, del zk path, path = [%s]", path);
        return;
    }

    if ((zkc->replace_path_del_tm) && ((zkc->replace_path_del_tm < mstime()))) {
        serverLog(LL_VERBOSE, "zookeeper, start del replace path, path = [%s], replace_path_del_tm = [%lld]", zkc->replace_path, zkc->replace_path_del_tm);
        zklinkDelOldMasterPath(zkc);
        zkc->replace_path_del_tm = 0;
    }

    if (1 == zkc->zk_create_path) {
        return;
    }

    int rc = zoo_create(zkc->zh, (const char*) path, NULL, 0, &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0);

    if (ZNODEEXISTS == rc) {
        serverLog(LL_WARNING, "zookeeper, zoo_create fail, path = [%s] already exist", path);
        return;
    }

    if (ZOK != rc) {
        serverLog(LL_WARNING, "zookeeper, zoo_create fail, path = [%s], rc = [%d]", path, rc);
        return;
    }
        
    zkc->zk_create_path = 1;
    serverLog(LL_VERBOSE, "zookeeper, zoo_create over, path = [%s]", path);
}

void zklinkDelOldMasterPath(zklinkClient* zkc)
{
    if (!zkc->replace_path) {
        return;
    }

    char path[CLUSTER_NAMELEN+9] = {0};
    snprintf(path, CLUSTER_NAMELEN+9 , "/replace-%s", zkc->replace_path);

    int rc = zoo_delete(zkc->zh, path, -1);
    if ((ZNONODE != rc) && (ZOK != rc)) {
        // TODO: 下次心跳继续删? 
        serverLog(LL_WARNING, "zookeeper, zoo_delete old master fail, path = [%s], rc = [%d]", path, rc);
    }
    
    serverLog(LL_VERBOSE, "zookeeper, zklinkDelOldMasterPath over, path = [%s]", path);
    zfree(zkc->replace_path);
    zkc->replace_path = NULL;
}

void zklinkWatchPath(zklinkClient* zkc)
{
    struct redisServer* server = zkc->redis_server;
    clusterNode* myself = server->cluster->myself;

    if (nodeIsMaster(myself)) {
        return;
    }

    if (1 == zkc->zk_on_wath) {
        return;
    }

    clusterNode* master = myself->slaveof;
    if (!master) {
        return;
    }

    char path[CLUSTER_NAMELEN+1] = {0};
    snprintf(path, CLUSTER_NAMELEN+1 , "/%s", master->name);

    int rc = zoo_awget(zkc->zh, path, zklinkWatcher, zkc,  zklinkCompletion, NULL);
    if (ZOK != rc) {
        serverLog(LL_WARNING, "zookeeper, zoo_awget fail, path = [%s], rc = [%d]", path, rc);
        return;
    }

    zkc->zk_on_wath = 1;
    zkc->replace_path = zstrdup(master->name);
    
    serverLog(LL_VERBOSE, "zookeeper, zoo_awget add watch over, path = [%s]", path);
}

void zklinkUpdte(zklinkClient* zkc)
{
    zklinkCreatePath(zkc);
    zklinkWatchPath(zkc);
}
