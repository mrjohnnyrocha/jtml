// ReactiveArray.cpp
#include "jtml/array.h"
#include "jtml/environment.h"
#include <iostream>

namespace JTMLInterpreter {

ReactiveArray::ReactiveArray(std::weak_ptr<Environment> env, const CompositeKey& key)
    : environment(env), arrayKey(key) {

}

void ReactiveArray::setKey(const CompositeKey& newKey) { arrayKey = newKey; }

const std::string& ReactiveArray::getName() const { return name; }

const CompositeKey ReactiveArray::getKey() const  { return arrayKey; }

const std::vector<std::shared_ptr<VarValue>>& ReactiveArray::getArrayData() const { return arrayData; }

void ReactiveArray::push(const std::shared_ptr<VarValue>& value) {
    arrayData.push_back(value);
    if (auto envPtr = environment.lock()) {
        if (envPtr->hasVariable(arrayKey)) {
            envPtr->markDirty(arrayKey);
        }
        std::cout << "[ReactiveArray] push: Added value to array '" << envPtr->getCompositeName(arrayKey) << "'.\n";
    } else {
        throw std::runtime_error("Invalid weak_ptr to Environment in ReactiveArray::push.");
    }
}

std::shared_ptr<VarValue> ReactiveArray::pop() {
    if (arrayData.empty()) {
        throw std::runtime_error("Cannot pop from an empty array.");
    }
    if (auto envPtr = environment.lock()) {
        auto value = arrayData.back();
        arrayData.pop_back();
        envPtr->markDirty(arrayKey);
        std::cout << "[ReactiveArray] pop: Removed value from array '" << envPtr->getCompositeName(arrayKey) << "'.\n";
        return value;
    } else {
        throw std::runtime_error("Invalid weak_ptr to Environment in ReactiveArray::pop.");
    }
}

void ReactiveArray::splice(int index, int deleteCount, const std::vector<std::shared_ptr<VarValue>>& values) {
    validateIndex(index);
    if (deleteCount < 0 || index + deleteCount > static_cast<int>(arrayData.size())) {
        throw std::runtime_error("splice: Invalid deleteCount or index.");
    }
    if (auto envPtr = environment.lock()) {
        auto begin = arrayData.begin() + index;
        auto end = begin + deleteCount;
        arrayData.erase(begin, end);
        arrayData.insert(begin, values.begin(), values.end());
        envPtr->markDirty(arrayKey);
        std::cout << "[ReactiveArray] splice: Modified array '" << envPtr->getCompositeName(arrayKey) << "'.\n";
    } else {
        throw std::runtime_error("Invalid weak_ptr to Environment in ReactiveArray::splice.");
    }
}

std::shared_ptr<VarValue> ReactiveArray::get(int index) const {
    validateIndex(index);
    return arrayData[index];
}

std::shared_ptr<VarValue>& ReactiveArray::operator[](size_t index) {
    return arrayData[index];
}

const std::shared_ptr<VarValue>& ReactiveArray::operator[](size_t index) const {
    return arrayData[index];
}

void ReactiveArray::set(int index, const std::shared_ptr<VarValue>& value) {
    validateIndex(index);
    arrayData[index] = value;
    if (auto envPtr = environment.lock()) {
        if (envPtr->hasVariable(arrayKey)) {
            envPtr->markDirty(arrayKey);
        }
        std::cout << "[ReactiveArray] set: Updated index " << index << " in array '" << envPtr->getCompositeName(arrayKey) << "'.\n";
    } else {
        throw std::runtime_error("Invalid weak_ptr to Environment in ReactiveArray::set.");
    }
}

size_t ReactiveArray::size() const {
    return arrayData.size();
}

std::string ReactiveArray::toString() const {
    std::string result = "[";
    for (size_t i = 0; i < arrayData.size(); ++i) {
        result += arrayData[i]->toString();
        if (i != arrayData.size() - 1) result += ", ";
    }
    result += "]";
    return result;
}

void ReactiveArray::validateIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(arrayData.size())) {
        throw std::runtime_error("ReactiveArray: Index out of bounds.");
    }
}

} // namespace JTMLInterpreter
