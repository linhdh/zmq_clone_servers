//
// Created by linhdh on 24/09/2025.
//

#ifndef ZMQ_CLONE_SERVERS_CLONESRV5_H
#define ZMQ_CLONE_SERVERS_CLONESRV5_H

#include "../commons/kvmsg.hpp"

//  Routing information for a key-value snapshot
typedef struct {
    zmqpp::socket_t *socket; //  ROUTER socket to send to
    std::string identity; //  Identity of peer who requested state
    std::string subtree; //  Client subtree specification
} kvroute_t;

typedef struct {
    zmqpp::context_t *ctx;  // Our context
    std::unordered_map<std::string, KVMsg*> kvmap; // Key-value store
    int64_t sequence;  // How many updates we're at
    int port;          // Main port we're working on
    zmqpp::socket_t* snapshot;    // Handle snapshot requests
    zmqpp::socket_t* publisher;   // Publish updates to clients
    zmqpp::socket_t* collector;   // Collect updates from clients
} clonesrv_t;

// loop event handlers
static bool s_snapshots(clonesrv_t *self);
static bool s_collector(clonesrv_t *self);
static bool s_flush_ttl(clonesrv_t *self);

#endif //ZMQ_CLONE_SERVERS_CLONESRV5_H