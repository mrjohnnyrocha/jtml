// jtml_interpreter.h
#pragma once

#include "jtml/ast.h"
#include "jtml/transpiler.h"
#include "jtml/parser.h"
#include "jtml/lexer.h"
#include "jtml/value.h"
#include "jtml/dict.h"
#include "jtml/array.h"
#include "jtml/environment.h"
#include "jtml/instance_id_generator.h"
#include "jtml/value.h"  // so we can reference JTML::VarValue, ValueVariant, DictType
#include "jtml/function.h"
#include "jtml/renderer.h"
#include "jtml/semantic/module_graph.h"
#include "jtml/websocket_server.h"
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "json.hpp"


namespace JTML = JTMLInterpreter;
// Configuration for constructing an Interpreter.
// The default behaviour preserves the legacy one-shot mode used by
// `jtml serve`, `jtml interpret`, and `quick-test`: a WebSocket server is
// started on port 8080 inside the constructor. Tutorial / long-lived embedding
// use cases set `startWebSocket = true` together with a configurable port,
// or `startWebSocket = false` to drive the interpreter purely in-process.
struct InterpreterConfig {
    uint16_t wsPort = 8080;
    bool startWebSocket = true;
};

// Interpreter class
class Interpreter {
public:
    explicit Interpreter(JtmlTranspiler& transpiler, InterpreterConfig config = {});
    ~Interpreter(); // Default destructor

    void handleFrontendMessage(const std::string& msg, websocketpp::connection_hdl hdl);
    void populateBindings(websocketpp::connection_hdl hdl);

    // Interpret methods
    void interpret(const JtmlElementNode&);
    void interpret(const std::vector<std::unique_ptr<ASTNode>>& program);
    void interpret(const std::string& code);

    // Tear down the current program state (variables, derived values, bindings,
    // function declarations, class declarations, counters) WITHOUT stopping the
    // WebSocket server. Intended for the tutorial / playground flow where the
    // same process swaps programs as the user navigates lessons.
    void reset();

    // Convenience: reset() followed by interpret(program). Also broadcasts a
    // `reload` message to any connected frontends so they refetch bindings.
    void reload(const std::vector<std::unique_ptr<ASTNode>>& program);

    // Serialize the current environment's content / attribute bindings as a
    // JSON string of shape `{"content":{...}, "attributes":{...}}`. Used by
    // the tutorial server to inline initial values into the preview HTML so
    // the iframe renders correctly even when the WebSocket cannot be reached
    // (for example through an IDE's browser preview proxy).
    std::string getBindingsJSON();

    // Serialize runtime variables/functions for external tools, test runners,
    // IDEs, and framework hosts. Shape:
    // `{"variables": {...}, "functions": {...}}`.
    std::string getStateJSON();
    std::string getComponentsJSON();
    std::string getComponentDefinitionsJSON();

    // Dispatch a frontend-originated event without going through the
    // WebSocket. Identical semantics to the `type:"event"` branch in
    // `handleFrontendMessage`, but callable directly from an HTTP route.
    // On success returns true and populates `updatedBindingsJSON` with the
    // fresh bindings. On failure returns false and populates `errorOut`.
    bool dispatchEvent(const std::string& elementId,
                       const std::string& eventType,
                       const nlohmann::json& args,
                       std::string& updatedBindingsJSON,
                       std::string& errorOut);
    bool dispatchComponentAction(const std::string& componentId,
                                 const std::string& actionName,
                                 const nlohmann::json& args,
                                 std::string& updatedBindingsJSON,
                                 std::string& errorOut);

    std::shared_ptr<JTML::VarValue> evaluateExpression(const ExpressionStatementNode* exprNode, std::shared_ptr<JTML::Environment> env);


    double getNumericValue(const std::shared_ptr<JTML::VarValue>& valPtr);
    std::string getStringValue(const std::shared_ptr<JTML::VarValue>& valPtr);

    std::shared_ptr<JTML::Environment> getCurrentEnvironment() const;

    // Error handling

private:
    JtmlTranspiler& transpiler;
    std::shared_ptr<JTML::Environment> globalEnv;
    std::shared_ptr<JTML::Environment> currentEnv;
    bool inFunctionContext = false;

    std::thread wsThread;

    int nodeID;
    int uniqueVarID;

    int uniqueArrayVarID;
    int uniqueDictVarID ;

    std::unique_ptr<JTML::Renderer> renderer;
    std::shared_ptr<JTML::WebSocketServer> wsServer;

    std::unordered_map<std::string, std::shared_ptr<ClassDeclarationNode>> classDeclarations;

    struct BodyPlanNode {
        jtml::SemanticModuleId definitionModule = jtml::InvalidSemanticModuleId;
        int indent = 0;
        int parentIndex = -1;
        std::vector<int> childIndices;
        std::string kind;
        std::string head;
        std::string name;
        std::string text;
        std::string operatorToken;
        std::string expression;
        std::string keyExpression;
        bool renderRoot = false;
    };

    struct RuntimeComponentEventHandler {
        std::string name;
        nlohmann::json args = nlohmann::json::array();
    };

    struct RuntimeComponentInstance {
        jtml::SemanticModuleId moduleId = jtml::InvalidSemanticModuleId;
        jtml::SemanticModuleId definitionModule = jtml::InvalidSemanticModuleId;
        std::string id;
        std::string parentId;
        std::string component;
        std::string role;
        int sourceLine = 0;
        JTML::InstanceID instanceID = 0;
        std::map<std::string, std::string> params;
        std::map<std::string, std::string> locals;
        std::map<std::string, RuntimeComponentEventHandler> eventHandlers;
        std::vector<BodyPlanNode> slotPlan;
        std::shared_ptr<JTML::Environment> environment;
        bool runtimeReady = false;
        std::vector<std::string> stateNames;
        std::vector<std::string> derivedNames;
        std::vector<std::string> actionNames;
        std::vector<std::string> effectNames;
        std::size_t lastSeenRenderGeneration = 0;
    };

    struct RuntimeComponentDefinition {
        jtml::SemanticModuleId moduleId = jtml::InvalidSemanticModuleId;
        std::string name;
        int sourceLine = 0;
        std::vector<std::string> params;
        std::vector<std::string> emits;
        std::map<std::string, int> emitArity;
        std::map<std::string, std::vector<std::string>> emitPayloads;
        std::map<std::string, std::vector<std::string>> emitPayloadTypes;
        std::string body;
        std::vector<BodyPlanNode> bodyPlan;
        std::vector<std::string> localState;
        std::vector<std::string> localDerived;
        std::vector<std::string> localActions;
        std::vector<std::string> localEffects;
        std::vector<std::string> eventBindings;
        bool hasSlot = false;
        int bodyNodeCount = 0;
        int rootTemplateNodeCount = 0;
        int slotCount = 0;
    };

    std::vector<RuntimeComponentInstance> componentInstances;
    std::vector<RuntimeComponentDefinition> componentDefinitions;
    std::unordered_map<std::string, RuntimeComponentInstance> dynamicComponentInstances;
    std::size_t componentRenderGeneration = 0;

    // Function Execution
    std::shared_ptr<JTML::VarValue> executeFunction(
        const std::shared_ptr<JTML::Function>& func,
        const std::vector<std::shared_ptr<JTML::VarValue>>& args,
        const std::shared_ptr<JTML::VarValue>& thisPtr = nullptr,
        bool dispatchAsync = true
    );

    std::shared_ptr<JTML::VarValue> instantiateClass(
    const ClassDeclarationNode& classNode,
    const std::vector<std::unique_ptr<ExpressionStatementNode>>& arguments,
    std::shared_ptr<JTML::Environment> parentEnv
);

    // Dependency Tracking with Integer IDs

    void recalcDirty(std::shared_ptr<JTML::Environment> env);
    void recalcAllDirty();
    void updateVariable(JTML::VarID varID, std::shared_ptr<JTML::Environment> env);

    // Variable management methods
    void storeVariable(const std::string& scope, const std::string& varName);


    // Expression evaluation
    bool evaluateCondition(const ExpressionStatementNode* condition, std::shared_ptr<JTML::Environment> env);
    void gatherDeps(const ExpressionStatementNode* exprNode, std::vector<JTML::CompositeKey>& out, std::shared_ptr<JTML::Environment> env);

    bool isTruthy(const std::string& value);
    bool performNumericCompare(const std::string& op, double ln, double rn);
    bool performStringCompare(const std::string& op, const std::string& ls, const std::string& rs);



    struct BreakException : public std::exception {};
    struct ContinueException : public std::exception {};

    // Interpretation methods
    void interpret(const ASTNode& node);
    void interpretNode(const ASTNode& node);
    void interpretElement(const JtmlElementNode& elem);
    void interpretElementAttributes(const JtmlElementNode& node);
    void registerComponentInstances(const std::vector<std::unique_ptr<ASTNode>>& program);
    void collectComponentInstances(const ASTNode& node, std::vector<RuntimeComponentInstance>& out);
    void collectComponentDefinitions(const ASTNode& node, std::vector<RuntimeComponentDefinition>& out);
    void attachComponentEnvironment(RuntimeComponentInstance& instance);
    const RuntimeComponentDefinition* findComponentDefinition(const std::string& name) const;
    const RuntimeComponentDefinition* findComponentDefinition(
        const std::string& name,
        jtml::SemanticModuleId preferredModule,
        jtml::SemanticModuleId fallbackModule) const;
    RuntimeComponentInstance* findComponentInstance(const std::string& id);
    RuntimeComponentInstance* findComponentInstanceForElement(const JtmlElementNode& elem);
    std::shared_ptr<JTML::Environment> environmentForInstance(JTML::InstanceID instanceID) const;
    bool isComponentInstanceElement(const JtmlElementNode& elem) const;
    bool isEventAttribute(const std::string& attrName) const;
    std::string extractEventType(const std::string& attrName) const;
    bool containsExpression(const ExpressionStatementNode* exprNode) const;
    void interpretShowElement(const ShowStatementNode& node);
    void interpretIfElement(const IfStatementNode& node);
    void interpretWhileElement(const WhileStatementNode& node);
    void interpretForElement(const ForStatementNode& node);
    void interpretBlockStatement(const BlockStatementNode& block);
    void interpretShow(const ShowStatementNode& stmt);
    void interpretExpression(const ExpressionNode& node);
    void interpretDefine(const DefineStatementNode& stmt);
    void interpretAssignment(const AssignmentStatementNode& stmt);
    void interpretDerive(const DeriveStatementNode& stmt);
    void interpretUnbind(const UnbindStatementNode& stmt);
    void interpretStore(const StoreStatementNode& stmt);
    void interpretIf(const IfStatementNode& stmt);
    void interpretWhile(const WhileStatementNode& stmt);
    void interpretFor(const ForStatementNode& stmt);
    void interpretSubscribe(const SubscribeStatementNode& node);
    void interpretUnsubscribe(const UnsubscribeStatementNode& node);
    void interpretBreak(const BreakStatementNode& node);
    void interpretContinue(const ContinueStatementNode& node);
    void interpretTryExceptThen(const TryExceptThenNode& stmt);
    void interpretReturn(const ReturnStatementNode& stmt);
    void interpretThrow(const ThrowStatementNode& stmt);
    void interpretImport(const ImportStatementNode& stmt);
    void interpretFunctionDeclaration(const FunctionDeclarationNode& outerDecl);
    void interpretClassDeclaration(const ClassDeclarationNode& node);
    void collectAllNestedFunctions(
        const std::vector<std::unique_ptr<ASTNode>>& stmts,
        const std::shared_ptr<JTML::Environment>& closureEnv
    );
    void handleError(const std::string& message);

};
