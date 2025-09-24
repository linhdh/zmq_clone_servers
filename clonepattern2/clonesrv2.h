//
// Created by linhdh on 24/09/2025.
//

#ifndef ZMQ_CLONE_SERVERS_CLONESRV2_H
#define ZMQ_CLONE_SERVERS_CLONESRV2_H

#include "../commons/kvsimple.hpp"
#include <thread>

static int s_send_snapshot(std::unordered_map<std::string, kvmsg>& kvmap, zmq::socket_t* snapshot);
static void state_manager(zmq::context_t* ctx);

// simulate zthread_fork, create attached thread and return the pipe socket
std::pair<std::thread, zmq::socket_t> zthread_fork(zmq::context_t& ctx, void (*thread_func)(zmq::context_t*)) {
    // create the pipe socket for the main thread to communicate with its child thread
    zmq::socket_t pipe(ctx, ZMQ_PAIR);
    pipe.connect("inproc://state_manager");

    // start child thread
    std::thread t(thread_func, &ctx);

    return std::make_pair(std::move(t), std::move(pipe));
}

#endif //ZMQ_CLONE_SERVERS_CLONESRV2_H