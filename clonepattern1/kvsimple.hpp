//
// Created by linhdh on 24/09/2025.
//

#ifndef ZMQ_CLONE_SERVERS_KVSIMPLE_HPP
#define ZMQ_CLONE_SERVERS_KVSIMPLE_HPP

#include "zhelpers.hpp"
#include <zmq.hpp>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

using ustring = std::basic_string<unsigned char>;
struct kvmsg {
  kvmsg(std::string key, int64_t sequence, ustring body);
  kvmsg() = default;
  //  Reads key-value message from socket, returns new kvmsg instance.
  static std::optional<kvmsg> recv(zmq::socket_t &socket);
  //  Send key-value message to socket; any empty frames are sent as such.
  void send(zmq::socket_t &socket);

  //  Return key from last read message, if any, else NULL
  std::string key() const;
  //  Return sequence nbr from last read message, if any
  int64_t sequence() const;
  //  Return body from last read message, if any, else NULL
  ustring body() const;
  //  Return body size from last read message, if any, else zero
  size_t size() const;

  //  Set message key as provided
  void set_key(std::string key);
  //  Set message sequence number
  void set_sequence(int64_t sequence);
  //  Set message body
  void set_body(ustring body);

  //  Dump message to stderr, for debugging and tracing
  std::string to_string();

  //  Runs self test of class
  static bool test(int verbose);

private:
  static constexpr uint32_t kvmsg_key_max = 255;
  static constexpr uint32_t frame_key = 0;
  static constexpr uint32_t frame_seq = 1;
  static constexpr uint32_t frame_body = 2;
  static constexpr uint32_t kvmsg_frames = 3;

  std::string key_;
  ustring body_;
  int64_t sequence_{};
};

#endif //ZMQ_CLONE_SERVERS_KVSIMPLE_HPP