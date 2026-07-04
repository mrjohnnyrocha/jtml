// ReactiveDict.cpp
#include "jtml/dict.h"
#include "jtml/environment.h"
#include <iostream>

namespace JTMLInterpreter {

ReactiveDict::ReactiveDict(std::weak_ptr<Environment> env, const CompositeKey& key)
    : environment(std::move(env)), dictKey(key) {

    }

void ReactiveDict::setKey(const CompositeKey& newKey) { dictKey = newKey; }

const std::string& ReactiveDict::getName() const { return name; }

const CompositeKey ReactiveDict::getKey() const  { return dictKey; }

const std::unordered_map<std::string, std::shared_ptr<VarValue>>& ReactiveDict::getDictData() const { return dictData; }

void ReactiveDict::set(const std::string& dictKeyName, const std::shared_ptr<VarValue>& value) {
    dictData[dictKeyName] = value;
    if (auto envPtr = environment.lock()) {
        if (envPtr->hasVariable(dictKey)) {
            envPtr->markDirty(dictKey);
            std::cout << "[ReactiveDict] set: Set key '" << dictKeyName << "' in dict '" << envPtr->getCompositeName(dictKey) << "'.\n";
        }
    } else {
        throw std::runtime_error("Environment is no longer valid");
    }
}

void ReactiveDict::deleteKey(const std::string& dictKeyName) {
    if (auto envPtr = environment.lock()) {
        if (dictData.erase(dictKeyName) > 0) {
            envPtr->markDirty(dictKey);
            std::cout << "[ReactiveDict] deleteKey: Deleted key '" << dictKeyName << "' from dict '" << envPtr->getCompositeName(dictKey) << "'.\n";
        } else {
            std::cerr << "[ReactiveDict] deleteKey: Key '" << dictKeyName << "' not found in dict '" << envPtr->getCompositeName(dictKey) << "'.\n";
        }
    } else {
        throw std::runtime_error("Environment is no longer valid");
    }
}

std::shared_ptr<VarValue> ReactiveDict::get(const std::string& dictKeyName) const {
    auto it = dictData.find(dictKeyName);
    if (it != dictData.end()) {
        return it->second;
    }
    throw std::runtime_error("ReactiveDict: Key '" + dictKeyName + "' not found.");
}

std::vector<std::string> ReactiveDict::keys() const {
    std::vector<std::string> keyList;
    for (const auto& [k, _] : dictData) {
        keyList.push_back(k);
    }
    return keyList;
}

std::string ReactiveDict::toString() const {
    std::string result = "{";
    size_t count = 0;
    for (const auto& [k, v] : dictData) {
        result += "\"" + k + "\": " + v->toString();
        if (count != dictData.size() - 1) result += ", ";
        ++count;
    }
    result += "}";
    return result;
}

void ReactiveDict::validateKey(const std::string& key) const {
    if (dictData.find(key) == dictData.end()) {
        throw std::runtime_error("ReactiveDict: Key '" + key + "' does not exist.");
    }
}

} // namespace JTMLInterpreter
