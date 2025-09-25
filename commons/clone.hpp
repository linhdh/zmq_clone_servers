//
// Created by linhdh on 25/09/2025.
//

#ifndef ZMQ_CLONE_SERVERS_CLONE_HPP
#define ZMQ_CLONE_SERVERS_CLONE_HPP

#include <optional>
#include <string>
#include <zmqpp/zmqpp.hpp>

#include "kvmsg.hpp"
//  If no server replies within this time, abandon request
#define GLOBAL_TIMEOUT 4000 // msecs

class clone_t {
public:
    clone_t();
    ~clone_t();
    void connect(std::string endpoint, std::string service);
    void subtree(std::string subtree);
    void set(const std::string &key, const std::string &value, int ttl = 0);
    std::optional<std::string> get(const std::string &key);
private:
    bool clone_agent(zmqpp::socket_t *pipe, zmqpp::context_t *ctx);

    zmqpp::context_t *ctx; //  Our context wrapper
    zmqpp::actor *actor; //  Pipe through to clone agent
};

class server_t {
public:
    server_t(zmqpp::context_t *ctx, std::string address, int port, std::string subtree);
    ~server_t();
public:
    std::string address;   //  Server address
    int port;              //  Server port
    zmqpp::socket_t *snapshot; //  DEALER socket for snapshot
    zmqpp::socket_t *subscriber; //  SUB socket for updates
    std::chrono::time_point<std::chrono::steady_clock> expiry;       //  When server expires
    uint requests;         //  How many snapshot requests made?
};

//  .split backend agent class
//  Here is the implementation of the backend agent itself:

//  Number of servers to which we will talk to
#define SERVER_MAX 2

// Server considered dead if silent for this long
#define SERVER_TTL 5000 // msecs

// States we can be in
#define STATE_INITIAL 0 // Before asking server for state
#define STATE_SYNCING 1 // Getting state from server
#define STATE_ACTIVE 2  // Getting new updates from server

class agent_t {
    friend class clone_t;
public:
    agent_t(zmqpp::context_t *ctx, zmqpp::socket_t *pipe);
    ~agent_t();

private:
    int agent_control_message();

    zmqpp::context_t *ctx; // Context wrapper
    zmqpp::socket_t *pipe; // Pipe back to application
    std::unordered_map<std::string, KVMsg*> kvmap; // Actual key/value table
    std::string subtree; // Subtree specification, if any
    server_t *servers[SERVER_MAX];
    uint nbr_servers; // 0 to SERVER_MAX
    uint state; // Current state
    uint cur_server; // If active, server 0 or 1
    int64_t sequence; // Last kvmsg processed
    zmqpp::socket_t *publisher; // Outgoing updates
};

#endif //ZMQ_CLONE_SERVERS_CLONE_HPP