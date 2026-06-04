// InstanceIDGenerator.h

#ifndef INSTANCE_ID_GENERATOR_H
#define INSTANCE_ID_GENERATOR_H

#include <mutex>

namespace JTMLInterpreter {
class InstanceIDGenerator {
public:
    // Deleted constructors to prevent instantiation
    InstanceIDGenerator() = delete;
    InstanceIDGenerator(const InstanceIDGenerator&) = delete;
    InstanceIDGenerator& operator=(const InstanceIDGenerator&) = delete;

    // Static method to get the next unique ID
    static size_t getNextID() { // Changed return type to size_t
        // Static variables ensure that they are initialized only once
        static std::mutex idMutex;
        static size_t currentID = 1000; // Starting from 1000 to differentiate from globalEnv

        std::lock_guard<std::mutex> lock(idMutex);
        return ++currentID;
    }
};
} // namespace JTMLInterpreter

#endif // INSTANCE_ID_GENERATOR_H

