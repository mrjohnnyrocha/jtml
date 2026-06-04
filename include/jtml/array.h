#pragma once

#include "jtml/value.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <string>

// Forward declare Environment to avoid circular dependency
namespace JTMLInterpreter {
class Environment;
}

namespace JTMLInterpreter {

class ReactiveArray {
public:
    ReactiveArray(std::weak_ptr<Environment> env, const CompositeKey& key);

    // Array methods
    void push(const std::shared_ptr<VarValue>& value);
    std::shared_ptr<VarValue> pop();
    void splice(int index, int deleteCount, const std::vector<std::shared_ptr<VarValue>>& values);
    const std::vector<std::shared_ptr<VarValue>>& getArrayData() const;
    // Accessors
    std::shared_ptr<VarValue> get(int index) const;
    void set(int index, const std::shared_ptr<VarValue>& value);
    size_t size() const;

    // Mutators
    void setKey(const CompositeKey& newKey);
    const CompositeKey getKey() const;
    const std::string& getName() const;

    // Utility
    std::string toString() const;

    std::shared_ptr<VarValue>& operator[](size_t index);

    const std::shared_ptr<VarValue>& operator[](size_t index) const;

private:
    std::weak_ptr<Environment> environment; // Use forward-declared class
    CompositeKey arrayKey;
    std::vector<std::shared_ptr<VarValue>> arrayData;
    std::string name;

    // Helper to validate index
    void validateIndex(int index) const;
};

} // namespace JTMLInterpreter
