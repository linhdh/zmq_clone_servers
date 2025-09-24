//
// Created by linhdh on 24/09/2025.
//

#ifndef ZMQ_CLONE_SERVERS_KVMSG_HPP
#define ZMQ_CLONE_SERVERS_KVMSG_HPP

#include <random>
#include <string>
#include <unordered_map>
#include <csignal>
#include <atomic>
#include <cstdlib> // for rand
#include <zmqpp/zmqpp.hpp>
#include <iostream>
#include <sstream>
#include <stdarg.h>

using ustring = std::basic_string<unsigned char>;

class KVMsg {
public:
    KVMsg() = default;
    //  Constructor, sets sequence as provided
    KVMsg(int64_t sequence);
    //  Destructor
    ~KVMsg();
    //  Create duplicate of kvmsg
    KVMsg(const KVMsg &other);
    //  Create copy
    KVMsg& operator=(const KVMsg &other);
    //  Reads key-value message from socket, returns new kvmsg instance.
    static KVMsg* recv(zmqpp::socket_t &socket);
    //  Send key-value message to socket; any empty frames are sent as such.
    void send(zmqpp::socket_t &socket);

    //  Return key from last read message, if any, else NULL
    std::string key() const;
    //  Return sequence nbr from last read message, if any
    int64_t sequence() const;
    //  Return body from last read message, if any, else NULL
    ustring body() const;
    //  Return body size from last read message, if any, else zero
    size_t size() const;
    //  Return UUID from last read message, if any, else NULL
    std::string uuid() const;

    //  Set message key as provided
    void set_key(std::string key);
    //  Set message sequence number
    void set_sequence(int64_t sequence);
    //  Set message body
    void set_body(ustring body);
    //  Set message UUID to generated value
    void set_uuid();
    //  Set message key using printf format
    void fmt_key(const char *format, ...);
    //  Set message body using printf format
    void fmt_body(const char *format, ...);

    //  Get message property, if set, else ""
    std::string property(const std::string &name) const;
    //  Set message property
    //  Names cannot contain '='. Max length of value is 255 chars.
    void set_property(const std::string &name, const char *format, ...);

    //  Store entire kvmsg into hash map, if key/value are set
    //  Nullifies kvmsg reference, and destroys automatically when no longer
    //  needed.
    void store(std::unordered_map<std::string, KVMsg*> &hash);
    // clear the hash map, free elements
    static void clear_kvmap(std::unordered_map<std::string, KVMsg*> &hash);
    //  Dump message to stderr, for debugging and tracing
    std::string to_string();

    void encode_frames(zmqpp::message &frames);
    void decode_frames(const zmqpp::message &frames);

    //  Runs self test of class
    static bool test(int verbose);

private:
    //  Message is formatted on wire as 5 frames:
    //  frame 0: key (0MQ string)
    //  frame 1: sequence (8 bytes, network order)
    //  frame 2: uuid (blob, 16 bytes)
    //  frame 3: properties (0MQ string)
    //  frame 4: body (blob)
    static constexpr uint32_t FRAME_KEY = 0;
    static constexpr uint32_t FRAME_SEQ = 1;
    static constexpr uint32_t FRAME_UUID = 2;
    static constexpr uint32_t FRAME_PROPS = 3;
    static constexpr uint32_t FRAME_BODY = 4;
    static constexpr uint32_t KVMSG_FRAMES = 5;

    std::string key_;
    int64_t sequence_{};
    std::string uuid_;
    ustring body_;
    std::unordered_map<std::string, std::string> properties_;

    bool presents_[KVMSG_FRAMES];
};

namespace {
    std::string generateUUID() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);

        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; ++i) ss << dis(gen);
        // ss << "-";
        for (int i = 0; i < 4; ++i) ss << dis(gen);
        ss << "4";  // UUID version 4
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        // ss << "-";
        ss << dis2(gen);  // UUID variant
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        // ss << "-";
        for (int i = 0; i < 12; ++i) ss << dis(gen);
        return ss.str();
    }
}

//  ---------------------------------------------------------------------
//  Signal handling
//
//  Call s_catch_signals() in your application at startup, and then exit
//  your main loop if s_interrupted is ever 1. Works especially well with
//  zmq_poll.

static std::atomic<int> s_interrupted(0);

inline void s_signal_handler(int signal_value) {
    s_interrupted = 1;
}

// setting signal handler
inline void s_catch_signals() {
    std::signal(SIGINT, s_signal_handler);
    std::signal(SIGTERM, s_signal_handler);
}

//  Provide random number from 0..(num-1)
static int within(int num) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, num - 1);
    return dis(gen);
}

#endif //ZMQ_CLONE_SERVERS_KVMSG_HPP