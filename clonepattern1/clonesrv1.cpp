//
// Created by linhdh on 24/09/2025.
//

#include "clonesrv1.h"

int main() {
    // Prepare our context and publisher socket
    zmq::context_t ctx(1);
    zmq::socket_t publisher(ctx, ZMQ_PUB);
    publisher.bind("tcp://*:5555");
    s_sleep(5000); // Sleep for a short while to allow connections to be established

    // Initialize key-value map and sequence
    unordered_map<string,string> kvmap;
    int64_t sequence = 0;
    srand(time(nullptr));

    s_catch_signals();
    while (!s_interrupted) {
        // Distribute as key-value message
        string key = to_string(within(10000));
        string body = to_string(within(1000000));
        kvmsg kv(key, sequence, (unsigned char *)body.c_str());
        kv.send(publisher); // Send key-value message
        // Store key-value pair in map
        kvmap[key] = body;
        sequence++;

        // Sleep for a short while before sending the next message
        s_sleep(1000);
    }

    cout << "Interrupted" << endl;
    cout << sequence << " messages out" << endl;
    return 0;
}