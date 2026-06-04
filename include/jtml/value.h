#pragma once

#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "jtml/ast.h"

using InstanceID = size_t;
/**
 * Forward declarations so we can reference these types.
 */

namespace JTMLInterpreter {


class ReactiveArray;
class ReactiveDict;

struct CompositeKey {
    InstanceID instanceID;
    std::string varName;

    bool operator==(const CompositeKey& other) const {
        return instanceID == other.instanceID && varName == other.varName;
    }
};

struct CompositeKeyHash {
    std::size_t operator()(const CompositeKey& key) const {
        return std::hash<InstanceID>()(key.instanceID) ^ (std::hash<std::string>()(key.varName) << 1);
    }
};

inline std::ostream& operator<<(std::ostream& os, const CompositeKey& key) {
    os << "(" << key.instanceID << ", " << key.varName << ")";
    return os;
}

struct BindingInfo {
    CompositeKey varName; // The variable associated with the binding
    std::string elementId; // The frontend element ID
    std::string attribute; // The attribute to bind (empty if binding content)
    std::string bindingType; // The binding type
    std::shared_ptr<ExpressionStatementNode> expression;
};

class Environment;
class VarValue; // to allow e.g. std::shared_ptr<VarValue> below

/**
 * @brief Structure referencing an object instance in your runtime:
 *        a pointer to the instance's Environment.
 */
struct ObjectHandle {
    std::shared_ptr<Environment> instanceEnv;

    // Optionally store other object metadata (className, ID, etc.)
    // e.g.: std::string className;
};

/**
 * @brief A dictionary type: key => pointer to VarValue
 */
using DictType = std::unordered_map<std::string, std::shared_ptr<VarValue>>;

/**
 * @brief A variant type representing all possible values in your language.
 *        - double (numbers)
 *        - bool
 *        - std::string
 *        - std::vector<std::shared_ptr<VarValue>> (arrays)
 *        - DictType (dictionaries)
 *        - ObjectHandle (objects)
 */
using ValueVariant = std::variant<
    double,                                          // Numeric
    bool,                                            // Boolean
    std::string,                                     // String
    std::shared_ptr<ReactiveArray>,
    std::shared_ptr<ReactiveDict>,                                      // Dictionary
    ObjectHandle                                     // Object
>;

/**
 * @brief A class representing a runtime value (VarValue).
 *
 * This class can hold:
 * - numbers (double)
 * - booleans
 * - strings
 * - arrays (vector of VarValue pointers)
 * - dictionaries (unordered_map<string, VarValue ptr>)
 * - objects (via ObjectHandle)
 */
class VarValue {
public:
    // ------------------- Constructors -------------------

    /**
     * @brief Default constructor. Initializes with an empty string.
     */
    VarValue();

    /**
     * @brief Construct a numeric VarValue.
     */
    explicit VarValue(double val);

    /**
     * @brief Construct a size_t VarValue, by converting it to double.
     */
    explicit VarValue(size_t val);

    /**
     * @brief Construct a boolean VarValue.
     */
    explicit VarValue(bool val);

    /**
     * @brief Construct a string VarValue (copy).
     */
    explicit VarValue(const std::string& s);
    /**
     * @brief Construct a string VarValue (move).
     */
    explicit VarValue(std::string&& s);

    /**
     * @brief Construct an array VarValue.
     *
     * @param arr A shared pointer to a ReactiveArray.
     */
    explicit VarValue(const std::shared_ptr<ReactiveArray>& arr);

    /**
     * @brief Construct a dictionary VarValue.
     *
     * @param dict A shared pointer to a ReactiveDict.
     */
    explicit VarValue(const std::shared_ptr<ReactiveDict>& dict);
    /**
     * @brief Construct an object VarValue from an ObjectHandle.
     */
    explicit VarValue(const ObjectHandle& objHandle);

    /**
     * @brief Construct an object VarValue from a moved ObjectHandle.
     */
    explicit VarValue(ObjectHandle&& objHandle);

    /**
     * @brief Construct a VarValue from a ValueVariant (universal).
     */
    explicit VarValue(ValueVariant v);

    // ------------------- Type Checkers -------------------

    bool isNumber()  const;
    bool isBool()    const;
    bool isString()  const;
    bool isArray()   const;
    bool isDict()    const;

    /**
     * @brief True if this VarValue is an object (via ObjectHandle).
     */
    bool isObject()  const;

    // ------------------- Getters -------------------

    double getNumber() const ;

    bool getBool() const;

    const std::string& getString() const;

    std::shared_ptr<ReactiveArray> getArray() const;

    std::shared_ptr<ReactiveDict> getDict() const;

    /**
     * @brief Get the object handle by reference.
     */
    ObjectHandle& getObjectHandle();

    /**
     * @brief Get the object handle by const reference.
     */
    const ObjectHandle& getObjectHandle() const;

    // ------------------- Setters -------------------

    void setNumber(double val);
    void setBool(bool val);
    void setString(const std::string& s);
    void setString(std::string&& s);

    void setArray(const std::shared_ptr<ReactiveArray>& arr);

    void setDict(const std::shared_ptr<ReactiveDict>& dict);

    /**
     * @brief Sets the VarValue to hold an object handle.
     */
    void setObject(const ObjectHandle& objHandle);
    void setObject(ObjectHandle&& objHandle);

    // ------------------- Conversion -------------------
    std::shared_ptr<ReactiveArray> asArray() const;
    std::shared_ptr<ReactiveDict> asDict() const;

    // ------------------- Utility -------------------

    /**
     * @brief Convert this VarValue to a string for debugging/printing.
     */
    std::string toString() const;

private:
    ValueVariant data;
};

} // namespace JTMLInterpreter
