//
// Created by linhdh on 24/09/2025.
//

#ifndef ZMQ_CLONE_SERVERS_CLONESRV4_H
#define ZMQ_CLONE_SERVERS_CLONESRV4_H

#include "../commons/kvsimple.hpp"

//  Routing information for a key-value snapshot
typedef struct {
    zmq::socket_t *socket; //  ROUTER socket to send to
    std::string identity; //  Identity of peer who requested state
    std::string subtree; //  Client subtree specification
} kvroute_t;

//  Send one state snapshot key-value pair to a socket
//  Hash item data is our kvmsg object, ready to send
static int s_send_snapshot(std::unordered_map<std::string, kvmsg>& kvmap, kvroute_t& kvroute) {
    for (auto& kv : kvmap) {
        if (kvroute.subtree.size() <= kv.first.size() && kv.first.compare(0, kvroute.subtree.size(), kvroute.subtree) == 0) {
            s_sendmore(*kvroute.socket, kvroute.identity);
            kv.second.send(*kvroute.socket);
        }
    }
    return 0;
}

#endif //ZMQ_CLONE_SERVERS_CLONESRV4_H