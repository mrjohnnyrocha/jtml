// ReactiveDict.h
#pragma once

#include "jtml/value.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <stdexcept>

namespace JTMLInterpreter {

// Forward declaration to avoid circular dependency
class Environment;

class ReactiveDict {
public:
    ReactiveDict(std::weak_ptr<Environment> env, const CompositeKey& key);

    // Dictionary methods
    void set(const std::string& dictKey, const std::shared_ptr<VarValue>& value);
    void deleteKey(const std::string& dictKey);
    std::shared_ptr<VarValue> get(const std::string& dictKey) const;
    std::vector<std::string> keys() const;
    const std::unordered_map<std::string, std::shared_ptr<VarValue>>& getDictData() const;

    void setKey(const CompositeKey& newKey);
    const CompositeKey getKey() const;
    const std::string& getName() const;

    // Utility
    std::string toString() const;

private:
    std::weak_ptr<Environment> environment;
    CompositeKey dictKey;
    std::unordered_map<std::string, std::shared_ptr<VarValue>> dictData;
    std::string name;

    // Helper to validate key existence
    void validateKey(const std::string& key) const;
};

} // namespace JTMLInterpreter
