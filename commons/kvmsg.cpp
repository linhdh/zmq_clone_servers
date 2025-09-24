//
// Created by linhdh on 24/09/2025.
//

#include "kvmsg.hpp"

KVMsg::KVMsg(int64_t sequence) {
    sequence_ = sequence;
    presents_[FRAME_SEQ] = true;
}

KVMsg::~KVMsg() {
    std::cout << "DEBUG: freeing key=" << key_ << std::endl;
}

KVMsg::KVMsg(const KVMsg &other) {
    std::cout << "copy construct\n";
    key_ = other.key_;
    sequence_ = other.sequence_;
    uuid_ = other.uuid_;
    body_ = other.body_;
    properties_ = other.properties_;
    for (int i = 0; i < KVMSG_FRAMES; i++) {
        presents_[i] = other.presents_[i];
    }
}

KVMsg& KVMsg::operator=(const KVMsg &other) {
    std::cout << "copy assign\n";
    key_ = other.key_;
    sequence_ = other.sequence_;
    uuid_ = other.uuid_;
    body_ = other.body_;
    properties_ = other.properties_;
    for (int i = 0; i < KVMSG_FRAMES; i++) {
        presents_[i] = other.presents_[i];
    }
    return *this;
}

// implement the static method recv
KVMsg* KVMsg::recv(zmqpp::socket_t &socket) {
    KVMsg* kvmsg = new KVMsg(-1);
    zmqpp::message frames;
    if (!socket.receive(frames)) {
        return nullptr;
    }
    kvmsg->decode_frames(frames);
    return kvmsg;
}

void KVMsg::send(zmqpp::socket_t &socket) {
    zmqpp::message frames;
    encode_frames(frames);
    socket.send(frames);
}

std::string KVMsg::key() const {
    return key_;
}

int64_t KVMsg::sequence() const {
    return sequence_;
}

ustring KVMsg::body() const {
    return body_;
}

size_t KVMsg::size() const {
    return body_.size();
}

std::string KVMsg::uuid() const {
    return uuid_;
}

void KVMsg::set_key(std::string key) {
    key_ = key;
    presents_[FRAME_KEY] = true;
}

void KVMsg::set_sequence(int64_t sequence) {
    sequence_ = sequence;
    presents_[FRAME_SEQ] = true;
}

void KVMsg::set_body(ustring body) {
    body_ = body;
    presents_[FRAME_BODY] = true;
}

void KVMsg::set_uuid() {
    uuid_ = generateUUID();
    presents_[FRAME_UUID] = true;
}

void KVMsg::fmt_key(const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 256, format, args);
    va_end(args);
    key_ = buffer;
    presents_[FRAME_KEY] = true;
}

void KVMsg::fmt_body(const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 256, format, args);
    va_end(args);
    // body_ = ustring(buffer, buffer + strlen(buffer));
    body_ = ustring((unsigned char *)buffer, strlen(buffer));
    presents_[FRAME_BODY] = true;
}

std::string KVMsg::property(const std::string &name) const {
    if (!presents_[FRAME_PROPS]) {
        return "";
    }
    auto it = properties_.find(name);
    if (it == properties_.end()) {
        return "";
    }
    return it->second;
}

void KVMsg::set_property(const std::string &name, const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 256, format, args);
    va_end(args);
    properties_[name] = buffer;
    presents_[FRAME_PROPS] = true;
}

void KVMsg::encode_frames(zmqpp::message &frames) {
    // assert(frames.parts() == 0);
    if (presents_[FRAME_KEY]) {
        frames.add(key_);
    } else {
        frames.add("");
    }
    if (presents_[FRAME_SEQ]) {
        frames.add(sequence_);
    } else {
        frames.add(-1);
    }
    if (presents_[FRAME_UUID]) {
        frames.add(uuid_);
    } else {
        frames.add("");
    }
    if (presents_[FRAME_PROPS]) {
        std::string props;
        for (auto &prop : properties_) {
            props += prop.first + "=" + prop.second + "\n";
        }
        frames.add(props);
    } else {
        frames.add("");
    }
    if (presents_[FRAME_BODY]) {
        frames.add_raw(body_.data(), body_.size());
    } else {
        frames.add("");
    }
}

void KVMsg::decode_frames(const zmqpp::message &frames) {
    assert(frames.parts() == KVMSG_FRAMES);
    frames.get(key_, 0);
    if (!key_.empty()) {
        presents_[FRAME_KEY] = true;
    }
    frames.get(sequence_, 1);
    if (sequence_ != -1) {
        presents_[FRAME_SEQ] = true;
    }
    frames.get(uuid_, 2);
    if (!uuid_.empty()) {
        presents_[FRAME_UUID] = true;
    }
    auto props = frames.get<std::string>(3);
    properties_.clear();
    if (!props.empty()) {
        presents_[FRAME_PROPS] = true;
        size_t pos = 0;
        while (pos < props.size()) {
            size_t end = props.find('=', pos);
            std::string name = props.substr(pos, end - pos);
            pos = end + 1;
            end = props.find('\n', pos);
            std::string value = props.substr(pos, end - pos);
            pos = end + 1;
            properties_[name] = value;
        }
    }
    char const* raw_body = frames.get<char const*>(4);
    size_t size = frames.size(4);
    if (size > 0) {
        presents_[FRAME_BODY] = true;
        body_ = ustring((unsigned char const*)raw_body, size);
    }
}

void KVMsg::store(std::unordered_map<std::string, KVMsg*> &hash) {
    if (size() == 0) {
        hash.erase(key_);
        return;
    }
    if (presents_[FRAME_KEY] && presents_[FRAME_BODY]) {
        hash[key_] = this;
    }
}

void KVMsg::clear_kvmap(std::unordered_map<std::string, KVMsg*> &hash) {
    for (auto &kv : hash) {
        delete kv.second;
        kv.second = nullptr;
    }
    hash.clear();
}

std::string KVMsg::to_string() {
    std::stringstream ss;
    ss << "key=" << key_ << ",sequence=" << sequence_ << ",uuid=" << uuid_ << std::endl;
    ss << "propes={";
    for (auto &prop : properties_) {
        ss << prop.first << "=" << prop.second << ",";
    }
    ss << "},";
    ss << "body=";
    for (auto &byte : body_) {
        ss << std::hex << byte;
    }
    return ss.str();
}

bool KVMsg::test(int verbose) {
    zmqpp::context context;
    zmqpp::socket output(context, zmqpp::socket_type::dealer);
    output.bind("ipc://kvmsg_selftest.ipc");
    zmqpp::socket input(context, zmqpp::socket_type::dealer);
    input.connect("ipc://kvmsg_selftest.ipc");

    KVMsg kvmsg(1);
    kvmsg.set_key("key");
    kvmsg.set_uuid();
    kvmsg.set_body((unsigned char *)"body");
    if (verbose) {
        std::cout << kvmsg.to_string() << std::endl;
    }
    kvmsg.send(output);

    std::unordered_map<std::string, KVMsg*> kvmap;
    kvmsg.store(kvmap);
    std::cout << "print from kvmap[key]" << std::endl;
    std::cout << kvmap["key"]->to_string() << std::endl;

    KVMsg *kvmsg_p = KVMsg::recv(input);
    if (!kvmsg_p) {
        return false;
    }
    assert(kvmsg_p->key() == "key");
    delete kvmsg_p;

    kvmsg_p = new KVMsg(2);
    kvmsg_p->set_key("key2");
    kvmsg_p->set_property("prop1", "value1");
    kvmsg_p->set_property("prop2", "value2");
    kvmsg_p->set_body((unsigned char *)"body2");
    kvmsg_p->set_uuid();
    assert(kvmsg_p->property("prop2") == "value2");
    kvmsg_p->send(output);
    delete kvmsg_p;

    kvmsg_p = KVMsg::recv(input);
    if (!kvmsg_p) {
        return false;
    }
    assert(kvmsg_p->key() == "key2");
    assert(kvmsg_p->property("prop2") == "value2");
    if (verbose) {
        std::cout << kvmsg_p->to_string() << std::endl;
    }
    delete kvmsg_p;

    std::cout << "KVMsg self test passed" << std::endl;
    return true;
}