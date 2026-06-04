#pragma once

#include "jtml/value.h"
#include "jtml/function.h"
#include "jtml/ast.h" // Assuming all AST node definitions are here
#include "jtml/renderer.h"

#include <mutex>
#include "jtml/instance_id_generator.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <queue>
#include <functional>
#include <iostream>


namespace JTMLInterpreter {

class ReactiveArray; // Forward declaration
class ReactiveDict;

using ExpressionEvaluator = std::function<std::shared_ptr<VarValue>(const ExpressionStatementNode*)>;

using VarID = int;
using SubscriptionID = size_t;
using DependencyList = std::vector<VarID>;
// Alias for Instance ID
using InstanceID = size_t;

enum class VarKind { Normal, Derived, Frozen };

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment* mutable_cast() const {
        return const_cast<Environment*>(this);
    }

    // Variable Information Structure
    struct VarInfo {
        VarKind kind;
        std::shared_ptr<VarValue> currentValue;
        std::unique_ptr<ExpressionStatementNode> expression; // For derived variables
        std::vector<CompositeKey> dependencies; // Variable names this variable depends on
    };

    static constexpr VarID INVALID_VAR_ID = -1;

    // Currently processing variable
    VarID currentlyProcessingVar = INVALID_VAR_ID;

    // Execution queue for dynamic subscriptions
    std::vector<std::function<void()>> executionQueue;

    // Separate maps for variables and functions
    std::unordered_map<CompositeKey, std::shared_ptr<VarInfo>, CompositeKeyHash> variables;
    std::unordered_map<CompositeKey, std::shared_ptr<Function>, CompositeKeyHash> functions;

    mutable std::mutex envMutex; // Mutex to protect environment's data
    mutable std::mutex bindingMutex; // Mutex to protect environment's data

        // Renderer instance
    Renderer* renderer;

    // Dependency Tracking using integer IDs
    std::unordered_map<CompositeKey, VarID, CompositeKeyHash> nameToId; // Maps CompositeKey to VarID
    std::vector<CompositeKey> idToKey;
    std::vector<DependencyList> adjacency; // adjacency[VarID] = list of dependent VarIDs
    std::vector<DependencyList> reverseAdjacency;

    // Dirty variables for recalculation
    std::unordered_set<VarID> dirtyVars;
    std::priority_queue<std::pair<VarID, int>,
                    std::vector<std::pair<VarID, int>>,
                    std::greater<>> dirtyQueue;

        // Event Subscribers: varID -> list of function callbacks
        // Event Subscribers: varID -> (subscriptionID -> callback)
    std::unordered_map<VarID, std::unordered_map<SubscriptionID, std::function<void()>>> eventSubscribers;
    std::unordered_map<VarID, std::unordered_map<std::string, SubscriptionID>> functionSubscriptions;

    // Data bindings
    std::unordered_map<std::string, std::vector<BindingInfo>> bindings;

    // Subscription ID counter
    SubscriptionID nextSubscriptionID = 1;

    // Cycle Detection
    std::unordered_map<VarID, int> visitTimestamp; // Timestamp for visitation
    int currentTimestamp = 0;

    // Parent Environment for scoping
    std::shared_ptr<Environment> parent;

    // Instance ID
    InstanceID instanceID;

    Environment(std::shared_ptr<Environment> parentEnv = nullptr, size_t id = InstanceIDGenerator::getNextID(), Renderer* rend = nullptr);

    void setRenderer(Renderer* rend);

    bool isGlobalEnvironment() const;

    std::shared_ptr<ReactiveArray> createReactiveArray(const CompositeKey& key);
    std::shared_ptr<ReactiveDict> createReactiveDict(const CompositeKey& key);
    // Variable Lookup
    std::shared_ptr<VarValue> getVariable(const CompositeKey& key) const;

    // Variable Assignment
    void setVariable(const CompositeKey& key, std::shared_ptr<VarValue> value);
    void defineVariable(const CompositeKey& key, std::shared_ptr<VarValue> value, VarKind kind = VarKind::Normal);

    // Data Bindings
void registerBinding(const BindingInfo& binding);
std::unordered_map<std::string, std::vector<BindingInfo>> getBindings();


    // Derive Variable
    void deriveVariable(const CompositeKey& key,
                        std::unique_ptr<ExpressionStatementNode> expr,
                        const std::vector<CompositeKey>& deps,
                        ExpressionEvaluator evaluator);

    void unbindVariable(const CompositeKey& key);
    // Dependency Tracking
   std::string getCompositeName(const CompositeKey& key) const;
   VarID getVarID(const CompositeKey& key) const;

    void addDependency(const CompositeKey& dependency, const CompositeKey& dependent);

    void removeDependency(const CompositeKey& dependency, const CompositeKey& dependent);


    // Function Lookup
   std::shared_ptr<Function> getFunction(const CompositeKey& key) const;

    // Function Definition
    void defineFunction(const CompositeKey& key, std::shared_ptr<Function> func);
    // Event System: Subscribe to variable changes
    // Subscribe a function to a variable with prevention of duplicates
    SubscriptionID subscribeFunctionToVariable(
        const CompositeKey& key,
        const std::string& funcName,
        std::function<void()> callback
    );

    void subscribeToVariable(const CompositeKey& key,
                             const std::string& funcName,
                             std::function<void()> callback);

    void unsubscribeFunctionFromVariable(const CompositeKey& key, const std::string& funcName);

    void unsubscribeFromVariable(VarID varID, SubscriptionID id);
    // Trigger callbacks for a variable
    void notifySubscribers(VarID varID);

    // Optionally cascade notifications to dependents
    void notifySubscribersRecursive(VarID varID);

    void emitEvents(VarID varID);



    // Cycle detection using timestamp-based visitation
    bool detectCycle(const CompositeKey& key) const;
    // Cycle detection using timestamp-based visitation
    bool detectCycleUsingTimestamp(VarID node) const ;

    bool dfsCycleCheck(VarID node, int timestamp) const;

    // Dirty Variables Management
    void markDirty(const CompositeKey& key);

    void clearDirty();

    void clearDirty(VarID varID);

    void recalcDirty(std::function<void(VarID)> updater);

    int calculateDependencyDepth(VarID varID) const;

    int dfsCalculateDepth(VarID varID, std::unordered_map<VarID, int>& cache) const;

    // Check if a variable exists
    bool hasVariable(const CompositeKey& key) const;

    // Additional methods for dependency management can be added here

    };
} // namespace JTMLInterpreter
