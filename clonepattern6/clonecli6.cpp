//
// Created by linhdh on 25/09/2025.
//

#include "clonecli6.h"

int main() {
    //  Create distributed hash instance
    auto *clone = new clone_t();

    // Specify configuration
    clone->subtree(SUBTREE);
    clone->connect("tcp://localhost", "5556");
    clone->connect("tcp://localhost", "5566");

    //  Set random tuples into the distributed hash
    while (true) {
        std::string key = SUBTREE + std::to_string(within(10000));
        std::string value = std::to_string(within(1000000));
        clone->set(key, value, within(30));
        std::cout << "I: create key=" << key << " value=" << value << std::endl;
        // sleep 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}