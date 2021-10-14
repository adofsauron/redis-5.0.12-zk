#ifndef __ZKLINK_H
#define __ZKLINK_H

#ifndef THREADED
#define THREADED
#endif // THREADED
#include "zookeeper.h"
#ifdef THREADED
#undef THREADED
#endif // THREADED

struct redisServer;

enum zklinkState {
    ZKS_UNINIT          = 0,
    ZKS_WAIT_CONNECT    = 1,
    ZKS_CONNECTED       = 2,
    ZKS_DIS_CONNECT     = 3,
};

typedef struct zklinkClient {
    zhandle_t *zh;
    enum zklinkState zkstate;
    struct redisServer* redis_server;
    int zk_create_path;
    int zk_on_wath;
    char* replace_path;
    long long replace_path_del_tm;
}zklinkClient;


void zklinkCron(struct redisServer* server);

// 1: true, 0:fail
int zklinkTryCreateOldMasterPath(struct redisServer* server);


#endif /* __ZKLINK_H */
