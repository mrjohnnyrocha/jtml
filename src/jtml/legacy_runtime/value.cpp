#include "jtml/environment.h"
#include "jtml/array.h"
#include "jtml/dict.h"
#include "jtml/value.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>


namespace JTMLInterpreter {
    /**
     * @brief Default constructor. Initializes with an empty string.
     */
    VarValue::VarValue() : data(std::string()) {}

    /**
     * @brief Construct a numeric VarValue.
     */
    VarValue::VarValue(double val) : data(val) {}

    /**
     * @brief Construct a size_t VarValue, converting it to double.
     */

    VarValue::VarValue(size_t val) : data(static_cast<double>(val)) {}

    /**
     * @brief Construct a boolean VarValue.
     */
    VarValue::VarValue(bool val) : data(val) {}

    /**
     * @brief Construct a string VarValue (copy).
     */
    VarValue::VarValue(const std::string& s) : data(s) {}

    /**
     * @brief Construct a string VarValue (move).
     */
    VarValue::VarValue(std::string&& s) : data(std::move(s)) {}

    /**
     * @brief Construct an array VarValue.
     *
     * @param arr A shared pointer to a ReactiveArray.
     */
    VarValue::VarValue(const std::shared_ptr<ReactiveArray>& arr)
        : data(arr) {}

    /**
     * @brief Construct a dictionary VarValue.
     *
     * @param dict A shared pointer to a ReactiveDict.
     */
    VarValue::VarValue(const std::shared_ptr<ReactiveDict>& dict)
        : data(dict) {}
    /**
     * @brief Construct an object VarValue from an ObjectHandle.
     */
    VarValue::VarValue(const ObjectHandle& objHandle)
        : data(objHandle) {}

    /**
     * @brief Construct an object VarValue from a moved ObjectHandle.
     */
    VarValue::VarValue(ObjectHandle&& objHandle)
        : data(std::move(objHandle)) {}

    /**
     * @brief Construct a VarValue from a ValueVariant (universal).
     */
    VarValue::VarValue(ValueVariant v)
        : data(std::move(v)) {}

    // ------------------- Type Checkers -------------------

    bool VarValue::isNumber()  const { return std::holds_alternative<double>(data); }
    bool VarValue::isBool()    const { return std::holds_alternative<bool>(data); }
    bool VarValue::isString()  const { return std::holds_alternative<std::string>(data); }
    bool VarValue::isArray()   const { return std::holds_alternative<std::shared_ptr<ReactiveArray>>(data); }
    bool VarValue::isDict()    const { return std::holds_alternative<std::shared_ptr<ReactiveDict>>(data); }

    /**
     * @brief True if this VarValue is an object (via ObjectHandle).
     */
    bool VarValue::isObject()  const { return std::holds_alternative<ObjectHandle>(data); }

    // ------------------- Getters -------------------

    double VarValue::getNumber() const {
        if (!isNumber()) {
            throw std::runtime_error("VarValue is not a number");
        }
        return std::get<double>(data);
    }

    bool VarValue::getBool() const {
        if (!isBool()) {
            throw std::runtime_error("VarValue is not a bool");
        }
        return std::get<bool>(data);
    }

    const std::string& VarValue::getString() const {
        if (!isString()) {
            throw std::runtime_error("VarValue is not a string");
        }
        return std::get<std::string>(data);
    }

    std::shared_ptr<ReactiveArray> VarValue::getArray() const {
        if (!isArray()) {
            throw std::runtime_error("VarValue is not an array");
        }
        return std::get<std::shared_ptr<ReactiveArray>>(data);
    }

    std::shared_ptr<ReactiveDict> VarValue::getDict() const {
        if (!isDict()) {
            throw std::runtime_error("VarValue is not a dictionary");
        }
        return std::get<std::shared_ptr<ReactiveDict>>(data);
    }

    /**
     * @brief Get the object handle by reference.
     */
    ObjectHandle& VarValue::getObjectHandle() {
        if (!isObject()) {
            throw std::runtime_error("VarValue is not an object handle");
        }
        return std::get<ObjectHandle>(data);
    }

    /**
     * @brief Get the object handle by const reference.
     */
    const ObjectHandle& VarValue::getObjectHandle() const {
        if (!isObject()) {
            throw std::runtime_error("VarValue is not an object handle");
        }
        return std::get<ObjectHandle>(data);
    }

    // ------------------- Setters -------------------

    void VarValue::setNumber(double val) { data = val; }
    void VarValue::setBool(bool val)     { data = val; }
    void VarValue::setString(const std::string& s) { data = s; }
    void VarValue::setString(std::string&& s)      { data = std::move(s); }

    void VarValue::setArray(const std::shared_ptr<ReactiveArray>& arr) {
        data = arr;
    }

    void VarValue::setDict(const std::shared_ptr<ReactiveDict>& dict) {
        data = dict;
    }

    /**
     * @brief Sets the VarValue to hold an object handle.
     */
    void VarValue::setObject(const ObjectHandle& objHandle) {
        data = objHandle;
    }
    void VarValue::setObject(ObjectHandle&& objHandle) {
        data = std::move(objHandle);
    }


    std::shared_ptr<ReactiveArray> VarValue::asArray() const {
        if (std::holds_alternative<std::shared_ptr<ReactiveArray>>(data)) {
            return std::get<std::shared_ptr<ReactiveArray>>(data);
        }
        return nullptr;  // Return nullptr if not a ReactiveArray
    }

    std::shared_ptr<ReactiveDict> VarValue::asDict() const {
        if (std::holds_alternative<std::shared_ptr<ReactiveDict>>(data)) {
            return std::get<std::shared_ptr<ReactiveDict>>(data);
        }
        return nullptr;  // Return nullptr if not a ReactiveDict
    }

    // ------------------- Utility -------------------

    /**
     * @brief Convert this VarValue to a string for debugging/printing.
     */
    std::string VarValue::toString() const {
        // 1) String
        if (isString()) {
            return getString();
        }
        // 2) Number
        if (isNumber()) {
            std::ostringstream oss;
            oss << getNumber();
            return oss.str();
        }
        // 3) Bool
        if (isBool()) {
            return getBool() ? "true" : "false";
        }
        // 4) Array
         if (isArray()) {
            std::ostringstream oss;
            const auto& arr = getArray();
            oss << "[";
            for (size_t i = 0; i < arr->getArrayData().size(); ++i) {
                if (i > 0) oss << ", ";
                oss << arr->getArrayData()[i]->toString();
            }
            oss << "]";
            return oss.str();
        }
        // 5) Dictionary
        if (isDict()) {
            const auto& dict = getDict();
            std::ostringstream oss;
            oss << "{";
            bool first = true;
            for (const auto& [key, val] : dict->getDictData()) {
                if (!first) oss << ", ";
                first = false;
                oss << "\"" << key << "\": ";
                oss << (val ? val->toString() : "null");
            }
            oss << "}";
            return oss.str();
        }
        // 6) Object
        if (isObject()) {
            // Provide a more descriptive output for your object
            // e.g. show environment pointer address or a class name if you store it
            const auto& handle = getObjectHandle();
            // If you store "className" or "instanceID" in handle, show it:
            //   e.g. "ObjectHandle(class=Person, env=0x12345)"
            std::ostringstream oss;
            oss << "ObjectHandle(";
            if (handle.instanceEnv) {
                // we could show pointer address or a custom env ID
                oss << "env_ptr=" << handle.instanceEnv.get();
            } else {
                oss << "env=null";
            }
            oss << ")";
            return oss.str();
        }

        // fallback
        return "<unknown>";
    }

} // namespace JTMLInterpreter
