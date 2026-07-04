#include "jtml/environment.h"
#include "jtml/array.h"
#include "jtml/dict.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>


namespace JTMLInterpreter {



Environment::Environment(std::shared_ptr<Environment> parentEnv, size_t id, Renderer* rend)
    : parent(parentEnv), instanceID(id), renderer(rend) {}

void Environment::setRenderer(Renderer* rend) {
    this->renderer = rend;
}

std::unordered_map<std::string, std::vector<BindingInfo>> Environment::getBindings() {
    return bindings;
}

bool Environment::isGlobalEnvironment() const {
    return instanceID == 0;
}

std::shared_ptr<ReactiveArray> Environment::createReactiveArray(const CompositeKey& key) {
        return std::make_shared<ReactiveArray>(weak_from_this(), key);
}


std::shared_ptr<ReactiveDict> Environment::createReactiveDict(const CompositeKey& key) {
    return std::make_shared<ReactiveDict>(weak_from_this(), key);
}

// Variable Lookup
std::shared_ptr<VarValue> Environment::getVariable(const CompositeKey& key) const {
    auto it = variables.find(key);
    if (it != variables.end()) {
        return it->second->currentValue;
    }
    auto parentEnv = parent;
    while (parentEnv) {
        CompositeKey parentKey = { parentEnv->instanceID, key.varName };
        return parentEnv->getVariable(parentKey);
        parentEnv = parentEnv->parent;
    }
    throw std::runtime_error("Undefined variable: " + getCompositeName(key));
}

// Variable Assignment - updates existing variables only
void Environment::setVariable(const CompositeKey& key, std::shared_ptr<VarValue> value) {
    auto it = variables.find(key);

    if (it != variables.end()) {
        if (it->second->kind == VarKind::Frozen) {
            throw std::runtime_error("Cannot assign to const variable '" + getCompositeName(key) + "'");
        }
        it->second->currentValue = value;
        markDirty(key);
        std::cout << "[DEBUG] Set variable '" << getCompositeName(key) << "' = " << value->toString() << "\n";
        return;
    }

    // Try parent scope
    if (parent && parent->hasVariable(key)) {
        CompositeKey parentKey = { parent->instanceID, key.varName };
        parent->setVariable(parentKey, value);
        std::cout << "[DEBUG] Set variable '" << getCompositeName(key) << "' = " << value->toString() << "\n";
        return;
    }

    // Variable not found anywhere - this is an error for assignment
    throw std::runtime_error("Assignment to undefined variable: " + getCompositeName(key));
}

// Variable Definition - creates new variables in current scope only
void Environment::defineVariable(const CompositeKey& key, std::shared_ptr<VarValue> value, VarKind kind) {
    auto it = variables.find(key);
    if (it != variables.end()) {
        // Variable already exists - this is an error for define
        if (it->second->kind == VarKind::Frozen) {
            throw std::runtime_error("Cannot redefine const variable '" + getCompositeName(key) + "'");
        }
        // Redefinition in same scope - update value but preserve kind unless changing to derived
        if (kind != VarKind::Derived || it->second->kind != VarKind::Derived) {
            it->second->currentValue = value;
            if (kind == VarKind::Derived) {
                it->second->kind = kind;
            }
            markDirty(key);
            std::cout << "[DEBUG] Updated variable '" << getCompositeName(key) << "' = " << value->toString() << "\n";
            return;
        }
    }

    // Create new variable in current scope
    auto varInfo = std::make_shared<VarInfo>();
    varInfo->kind = kind;
    varInfo->currentValue = value;

    if (value->isArray()) {
        auto array = value->getArray();
        array->setKey(key);
        std::cout << "[DEBUG] Assigned name '" << getCompositeName(array->getKey()) << "' to ReactiveArray\n";
    }

    if (value->isDict()) {
        auto dict = value->getDict();
        dict->setKey(key); // Update dict's internal key
        std::cout << "[DEBUG] Assigned name '" << getCompositeName(dict ->getKey()) << "' to ReactiveDict\n";
    }

    variables[key] = varInfo;

    std::cout << "[DEBUG] Defined variable '" << getCompositeName(key) << "' = " << value->toString() << "\n";
}

// Data Bindings
void Environment::registerBinding(const BindingInfo& binding) {
std::lock_guard<std::mutex> lock(bindingMutex);

// Register the binding in the current environment only (avoid duplicate propagation to parents)
bindings[binding.varName.varName].push_back(binding);
std::cout << "[DEBUG] Binding registered: VarName=" << binding.varName.varName
            << ", ElementID=" << binding.elementId
            << ", Attribute=" << binding.attribute
            << ", BindingType=" << binding.bindingType << "\n";
}



// Derive Variable
void Environment::deriveVariable(const CompositeKey& key,
                    std::unique_ptr<ExpressionStatementNode> expr,
                    const std::vector<CompositeKey>& deps,
                    ExpressionEvaluator evaluator)
{
    // Check for cyclic dependencies
    if (detectCycle(key)) {
        throw std::runtime_error("Cyclic dependency detected while deriving variable '" + getCompositeName(key) + "'");
    }

    auto it = variables.find(key);
    if (it != variables.end()) {
        // Studio, dev-server reloads, and component re-interpretation can visit
        // the same generated binding key again. Replace the derived definition
        // idempotently instead of treating the old dependency edges as fatal.
        for (const auto& depKey : it->second->dependencies) {
            removeDependency(depKey, key);
        }

        // Clear existing subscriptions and function subscriptions
        VarID varID = getVarID(key);
        eventSubscribers.erase(varID);
        functionSubscriptions.erase(varID);

        std::cout << "[REDEFINE] " << getCompositeName(key) << " as Derived\n";
    }

    // Create or redefine the derived variable
    auto info = std::make_shared<VarInfo>();
    info->kind = VarKind::Derived;
    info->expression = std::move(expr);
    info->dependencies = std::move(deps);

    std::cout <<"[DEBUG] Derived variable expression: " << info->expression->toString() << "\n";

    try {
        // Evaluate the initial value of the derived variable using the provided evaluator
        info->currentValue = evaluator(info->expression.get());
    } catch (const std::exception& e) {
        throw std::runtime_error("Error evaluating initial value for derived variable '" + getCompositeName(key) + "': " + e.what());
    }

    variables[key] = info;


    for (const auto& dep : deps) {
        addDependency(dep, key);

        // Propagate subscriptions from the derived variable to its dependencies
        VarID varID = getVarID(key);
        VarID depID = getVarID(dep);
        for (const auto& [funcName, subID] : functionSubscriptions[varID]) {
            auto callbackIt = eventSubscribers[varID].find(subID);
            if (callbackIt != eventSubscribers[varID].end()) {
                    CompositeKey depKey = idToKey[depID];
                    subscribeToVariable(depKey, funcName, callbackIt->second);
            } else {
                std::cout << "Callback not found for SubscriptionID " + std::to_string(subID) << "\n";
        }
    }
    }
    // Initial calculation
    markDirty(key);

    std::cout << "[DERIVE] " << getCompositeName(key) << " = "
            << (info->currentValue ? info->currentValue->toString() : "undefined") << "\n";
}

void Environment::unbindVariable(const CompositeKey& key) {
    auto it = variables.find(key);
    if (it == variables.end()) {
        throw std::runtime_error("Attempted to unbind undefined variable '" + key.varName + "'");
    }

    VarID varID = getVarID(key);
    eventSubscribers.erase(varID);

    if (it->second->kind == VarKind::Derived) {
        // Remove dependencies if it's a derived variable
        for (const auto& depKey : it->second->dependencies) {
            removeDependency(depKey, key);
        }

        it->second->kind = VarKind::Normal;
        it->second->dependencies.clear();
        it->second->expression.reset();  // Clear the derived expression
        clearDirty(varID);

        std::cout << "[UNBIND] Derived variable '" << getCompositeName(key)
                    << "' (retains value: " << it->second->currentValue->toString() << ")\n";
    } else {
        // For normal variables, just remove subscriptions
        std::cout << "[UNBIND] Normal variable '" << getCompositeName(key)
                    << "' (retains value if any)\n";
    }

    // Remove outgoing dependencies
    for (const auto& depVarID : adjacency[varID]) {
        CompositeKey depKey = idToKey[depVarID];
        removeDependency(depKey, key);
    }

    // Clear all outgoing dependencies for this variable
    reverseAdjacency[varID].clear();
}

// Dependency Tracking
std::string Environment::getCompositeName(const CompositeKey& key) const {
    return std::to_string(key.instanceID) + "." + key.varName;
}

// Dependency Tracking
VarID Environment::getVarID(const CompositeKey& key) const {
    auto it = nameToId.find(key);
    if (it != nameToId.end()) {
        return it->second;
    }
    // Assign new ID
    VarID newID = idToKey.size();
    mutable_cast()->nameToId[key] = newID;
    mutable_cast()->idToKey.emplace_back(key);
    mutable_cast()->adjacency.emplace_back();
    mutable_cast()->reverseAdjacency.emplace_back();
    return newID;
}

void Environment::addDependency(const CompositeKey& dependency, const CompositeKey& dependent) {
    VarID depID = getVarID(dependency);
    VarID depntID = getVarID(dependent);
    adjacency[depID].push_back(depntID);
    reverseAdjacency[depntID].push_back(depID);
}

void Environment::removeDependency(const CompositeKey& dependency, const CompositeKey& dependent) {
    VarID depID = getVarID(dependency);
    VarID depntID = getVarID(dependent);

    auto& depList = adjacency[depID];
    depList.erase(std::remove(depList.begin(), depList.end(), depntID), depList.end());

    auto& revDepList = reverseAdjacency[depntID];
    revDepList.erase(std::remove(revDepList.begin(), revDepList.end(), depID), revDepList.end());
}


// Function Lookup
std::shared_ptr<Function> Environment::getFunction(const CompositeKey& key) const {
    std::lock_guard<std::mutex> lock(envMutex);
    auto it = functions.find(key);
    if (it != functions.end()) {
        return it->second;
    }


    auto parentEnv = parent;
    while (parentEnv) {
        CompositeKey parentKey = { parentEnv->instanceID, key.varName };
        return parentEnv->getFunction(parentKey);
        parentEnv = parentEnv->parent;
    }

    throw std::runtime_error("Undefined function: " + key.varName + " (InstanceID: " + std::to_string(key.instanceID) + ")");
}

// Function Definition
void Environment::defineFunction(const CompositeKey& key, std::shared_ptr<Function> func) {
    std::lock_guard<std::mutex> lock(envMutex);
    if (functions.find(key) != functions.end()) {
        throw std::runtime_error("Function already defined: " + key.varName + " (InstanceID: " + std::to_string(key.instanceID) + ")");
    }
    functions[key] = func;
    std::cout << "[DEBUG] Defined function '" << key.varName << "' in InstanceID " << key.instanceID << "\n";
}

// Event System: Subscribe to variable changes
// Subscribe a function to a variable with prevention of duplicates
SubscriptionID Environment::subscribeFunctionToVariable(
    const CompositeKey& key,
    const std::string& funcName,
    std::function<void()> callback
) {
    VarID varID = getVarID(key);

    // Check if the function is already subscribed to this varID
    auto varSubsIt = functionSubscriptions.find(varID);
    if (varSubsIt != functionSubscriptions.end()) {
        auto& funcSubs = varSubsIt->second;
        if (funcSubs.find(funcName) != funcSubs.end()) {
            // Already subscribed – skip
            std::cout << "[SUBSCRIBE] Skipped duplicate subscription for '"
                        << funcName << "' to variable '" << getCompositeName(key) << "'\n";
            return funcSubs[funcName];
        }
    }

    // Otherwise, create a new subscription
    SubscriptionID id = nextSubscriptionID++;
    eventSubscribers[varID][id] = callback;
    functionSubscriptions[varID][funcName] = id;
    std::cout << "[SUBSCRIBE] Function '" << funcName
                << "' subscribed to variable '" << getCompositeName(key)
                << "' with SubscriptionID " << id << "\n";
    return id;
}

void Environment::subscribeToVariable(const CompositeKey& key,
                            const std::string& funcName,
                            std::function<void()> callback)
{
    subscribeFunctionToVariable(key, funcName, callback);
}

void Environment::unsubscribeFunctionFromVariable(const CompositeKey& key, const std::string& funcName) {
    VarID varID = getVarID(key);
    auto varSubsIt = functionSubscriptions.find(varID);
    if (varSubsIt != functionSubscriptions.end()) {
        auto& funcSubs = varSubsIt->second;
        auto funcIt = funcSubs.find(funcName);
        if (funcIt != funcSubs.end()) {
            SubscriptionID id = funcIt->second;
            unsubscribeFromVariable(varID, id);
            funcSubs.erase(funcIt);
            return;
        }
    }
    std::cerr << "[UNSUBSCRIBE ERROR] Function '" << funcName
                << "' not found for variable '" << getCompositeName(key) << "'\n";
}

void Environment::unsubscribeFromVariable(VarID varID, SubscriptionID id) {
    auto varIt = eventSubscribers.find(varID);
    if (varIt != eventSubscribers.end()) {
        auto& subscribers = varIt->second;
        auto subIt = subscribers.find(id);
        if (subIt != subscribers.end()) {
            subscribers.erase(subIt);
            std::cout << "[UNSUBSCRIBE] Removed subscription ID "
                        << id << " from variable '" << idToKey[varID] << "'\n";
            return;
        }
    }
    std::cerr << "[UNSUBSCRIBE ERROR] Subscription ID "
                << id << " not found for variable '" << idToKey[varID] << "'\n";
}
// Trigger callbacks for a variable
void Environment::notifySubscribers(VarID varID) {
    auto varIt = eventSubscribers.find(varID);
    if (varIt != eventSubscribers.end()) {
        // Copy to allow safe iteration even if subscribers are modified
        auto subscribersCopy = varIt->second;
        for (const auto& [id, callback] : subscribersCopy) {
            try {
                callback();
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Callback execution failed for SubscriptionID "
                            << id << ": " << e.what() << "\n";
            }
        }
    }
}

// Optionally cascade notifications to dependents
void Environment::notifySubscribersRecursive(VarID varID) {
    notifySubscribers(varID);

    for (VarID dependent : adjacency[varID]) {
        notifySubscribers(dependent);
    }
    // Propagate to parent environment if needed
    if (parent && parent->nameToId.count(idToKey[varID])) {
        parent->notifySubscribers(parent->getVarID(idToKey[varID]));
    }
}

void Environment::emitEvents(VarID varID) {
    notifySubscribersRecursive(varID);

    CompositeKey key = idToKey[varID];
    auto it = bindings.find(key.varName);
    if (it != bindings.end()) {
    std::shared_ptr<VarValue> val = variables[key]->currentValue;
    std::string newVal = val ? val->toString() : "";
    if (renderer) {
        for (auto& b : it->second) {
            if (b.bindingType=="content") {renderer->sendBindingUpdate(b.elementId, newVal);};
            if (b.bindingType=="attribute") {renderer->sendAttributeUpdate(b.elementId, b.attribute, newVal);};
            // etc.
        }
    } else {
        throw std::runtime_error("Renderer not available in environment");
    }

}
}



// Cycle detection using timestamp-based visitation
bool Environment::detectCycle(const CompositeKey& key) const {
    VarID varID = getVarID(key);
    return detectCycleUsingTimestamp(varID);
}

// Cycle detection using timestamp-based visitation
bool Environment::detectCycleUsingTimestamp(VarID node) const {
    mutable_cast()->currentTimestamp++;
    return mutable_cast()->dfsCycleCheck(node, currentTimestamp);
}

bool Environment::dfsCycleCheck(VarID node, int timestamp) const {
    auto it = visitTimestamp.find(node);
    if (it != visitTimestamp.end() && it->second == timestamp) {
        return true; // Cycle detected
    }

    // Mark the node with the current timestamp
    mutable_cast()->visitTimestamp[node] = timestamp;

    // Recursively check all neighbors
    for (const auto& neighbor : adjacency[node]) {
        if (dfsCycleCheck(neighbor, timestamp)) {
            return true; // Cycle detected in neighbors
        }
    }

    return false; // No cycle detected
}

// Dirty Variables Management
void Environment::markDirty(const CompositeKey& key) {
    VarID varID = getVarID(key);

    if (dirtyVars.insert(varID).second) {
        int priority = calculateDependencyDepth(varID);
        dirtyQueue.push({varID, priority});

        // DO NOT call emitEvents() here - events are deferred until after recalcDirty()
        // to ensure subscribers see consistent state after topological sort

        for (VarID dependentID : adjacency[varID]) {
            std::cout << "[MARK DIRTY] Propagating to dependent: " << idToKey[dependentID] << "\n";
            // Assuming compositeName remains consistent
            CompositeKey dependentKey = idToKey[dependentID];
            markDirty(dependentKey);
        }
        std::cout << "[MARK DIRTY] Variable: " << getCompositeName(key)
                    << " (ID: " << varID << ", Priority: " << priority << ")\n";
    }

    if (parent && parent->hasVariable(key)) {
        parent->markDirty(key);
    }
}

void Environment::clearDirty() {
    dirtyVars.clear();
    while (!dirtyQueue.empty()) {
        dirtyQueue.pop();
    }
}

void Environment::clearDirty(VarID varID) {
    dirtyVars.erase(varID); // Remove from the dirtyVars set
    // Note: No need to clear from dirtyQueue explicitly as it will only process variables still in dirtyVars
}

void Environment::recalcDirty(std::function<void(VarID)> updater) {
    // Step 1: Identify the subgraph consisting only of dirty variables
    std::unordered_set<VarID> dirtySet(dirtyVars.begin(), dirtyVars.end());

    // Debug: Log the dirty set
    std::cout << "[RECALC_DIRTY] Dirty Set: ";
    for (const auto& varID : dirtySet) {
        std::cout << idToKey[varID] << " ";
    }
    std::cout << "\n";

    // Step 2: Initialize in-degree for each dirty variable
    std::unordered_map<VarID, int> inDegree;
    for (const auto& varID : dirtySet) {
        inDegree[varID] = 0;
    }

    // Step 3: Calculate in-degree within the dirty subset
    for (const auto& varID : dirtySet) {
        for (const auto& dependent : adjacency[varID]) {
            if (dirtySet.find(dependent) != dirtySet.end()) {
                inDegree[dependent]++;
            }
        }
    }

    // Step 4: Initialize the queue with variables having zero in-degree
    std::queue<VarID> zeroInDegreeQueue;
    for (const auto& [varID, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegreeQueue.push(varID);
        }
    }

    // Step 5: Perform topological sort
    std::vector<VarID> sortedVars;
    while (!zeroInDegreeQueue.empty()) {
        VarID current = zeroInDegreeQueue.front();
        zeroInDegreeQueue.pop();
        sortedVars.push_back(current);

        for (const auto& dependent : adjacency[current]) {
            if (dirtySet.find(dependent) != dirtySet.end()) {
                if (--inDegree[dependent] == 0) {
                    zeroInDegreeQueue.push(dependent);
                }
            }
        }
    }

    // Step 6: Check for cycles
    if (sortedVars.size() != dirtySet.size()) {
        throw std::runtime_error("Cycle detected in dependencies");
    }

    // Step 7: Update variables in sorted order
    std::cout << "[RECALC_DIRTY] Sorted Vars: ";
    for (const auto& varID : sortedVars) {
        std::cout << idToKey[varID] << " ";
    }
    std::cout << "\n";

    for (const auto& varID : sortedVars) {
        std::cout << "[RECALC_DIRTY] Updating variable: " << idToKey[varID] << "\n";
        updater(varID);
    }

    // Step 8: Clear the processed dirty set before notifying subscribers.
    // Subscriber callbacks may mutate state and mark new variables dirty; keep
    // those marks for a follow-up recalculation instead of erasing them here.
    dirtyVars.clear();
    while (!dirtyQueue.empty()) {
        dirtyQueue.pop();
    }

    for (const auto& varID : sortedVars) {
        notifySubscribers(varID);
    }

    if (!dirtyVars.empty()) {
        recalcDirty(std::move(updater));
    }
}


int Environment::calculateDependencyDepth(VarID varID) const {
    std::unordered_map<VarID, int> depthCache;
    return mutable_cast()->dfsCalculateDepth(varID, depthCache);
}

int Environment::dfsCalculateDepth(VarID varID, std::unordered_map<VarID, int>& cache) const {
    // Return cached result if available
    if (cache.find(varID) != cache.end()) {
        return cache[varID];
    }

    // Base case: No dependents
    if (adjacency[varID].empty()) {
        return 1;
    }

    // Recursive case: Calculate depth for all dependents
    int maxDepth = 0;
    for (VarID dependent : adjacency[varID]) {
        maxDepth = std::max(maxDepth, dfsCalculateDepth(dependent, cache));
    }

    // Cache and return the result
    cache[varID] = 1 + maxDepth;
    return cache[varID];
}

// Check if a variable exists
bool Environment::hasVariable(const CompositeKey& key) const {
    if (variables.find(key) != variables.end()) {
        return true;
    }
    CompositeKey localKey = { instanceID, key.varName };
    if (variables.find(localKey) != variables.end()) {
        return true;
    }
    if (parent) {
        CompositeKey parentKey = { parent->instanceID, key.varName };
        return parent->hasVariable(parentKey);
    }
    return false;
}

    // Additional methods for dependency management can be added here

} // namespace JTMLInterpreter
