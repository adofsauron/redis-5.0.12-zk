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
    snprintf(path, CLUSTER_NAMELEN+1 , "/replace-%s", zkc->replace_path);

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
    serverLog(LL_VERBOSE, "zookeeper, Watcher zklinkUpdateStateOnWatch, path = [%s]", path);

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
        serverLog(LL_DEBUG, "zookeeper, Watcher strncmp not equal, path = [%s], master->name = [%s]", path, master_path);
        return;
    }

    if (ZOO_CREATED_EVENT == type) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher, master path = [%s] be created", path);
        // TODO:

        return;
    } 
    
    if (ZOO_DELETED_EVENT == type) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher, master path = [%s] be deleted, mark master = [%s] as fail", path, master->name);

        master->flags &= ~CLUSTER_NODE_PFAIL;
        master->flags |= CLUSTER_NODE_FAIL;
        clusterSendFail(master->name);

        return;
    }

    serverLog(LL_VERBOSE, "zookeeper, Watcher, zklinkUpdateStateOnWatch un process, path = [%s], type = [%d]", path, type);
}

void zklinkWatcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    zklinkClient* zkc = (zklinkClient*) (watcherCtx);

    serverLog(LL_DEBUG, "zookeeper, Watcher, type=[%d] state=[%d] path=[%s] zkstate=[%d] \n", type, state, path, zkc->zkstate);

    if (ZOO_CONNECTED_STATE != state) {
        serverLog(LL_VERBOSE, "zookeeper, Watcher, zklinkWatcher not process, path = [%s], state = [%d]", path, state);
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
    if (!server->zk_host) {
        return;
    }

    if ((server->zk_host) && (server->zk_port > 0) && (!server->zklc)) {
        server->zklc = zklinkCreateClient(server);
        return;
    }

    if (!server->zklc) {
        return;
    }

    zklinkClient* zkc = server->zklc;

    if (ZKS_UNINIT == zkc->zkstate) {
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
        serverLog(LL_WARNING, "zookeeper, start del replace path, path = [%s], replace_path_del_tm = [%lld]", zkc->replace_path, zkc->replace_path_del_tm);
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
    snprintf(path, CLUSTER_NAMELEN+1 , "/replace-%s", zkc->replace_path);

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
        zkc->zk_on_wath = 0;
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
