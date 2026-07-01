#ifndef JTML_TRANSPILER_H
#define JTML_TRANSPILER_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include "jtml/ast.h"      // Where ASTNode, JtmlElementNode, etc. are declared
#include "jtml/runtime_plan.h"

struct TranspiledBinding {
    std::string variableName;
    std::string elementId;
};

/**
 * A Transpiler that converts a JTML AST into HTML+JS placeholders.
 *
 * Key Points:
 *  - Only conditionals/loops/show are allowed in an element node.
 *  - We produce {{varName}} placeholders for expressions,
 *    letting the Interpreter handle the logic.
 */
class JtmlTranspiler {
public:
    explicit JtmlTranspiler();
    std::unordered_map<int, std::unordered_map<std::string, std::string>> nodeDerivedMap;

    /**
     * Transpile a vector of AST nodes into a full HTML page string.
     */
    std::string transpile(const std::vector<std::unique_ptr<ASTNode>>& program);
    void setWebSocketPort(int port);
    void setBrowserLocalRuntime(bool enabled);
    void setExternalRuntimeScript(bool enabled);
    void setDynamicGeneratedUpdateFunctions(bool enabled);
    void setRuntimeProjectPlan(const jtml::RuntimeProjectPlan& plan);
    const std::string* getNodeBinding(const ASTNode& node, const std::string& role) const;
    const TranspiledBinding* getAttributeBinding(const JtmlAttribute& attr) const;

private:
    int uniqueElemId = 0;
    int uniqueVarId  = 0;
    int nodeID = 0;
    int webSocketPort = 8080;
    bool browserLocalRuntime = false;
    bool externalRuntimeScript = false;
    bool dynamicGeneratedUpdateFunctions = false;
    std::optional<jtml::RuntimeProjectPlan> runtimeProjectPlan;

    std::unordered_map<const ASTNode*, std::unordered_map<std::string, std::string>> nodeBindings;
    std::unordered_map<const JtmlAttribute*, TranspiledBinding> attributeBindings;

    // Internal dispatch
    std::string transpileNode(const ASTNode& node, bool insideElement);
    std::string transpileElement(const JtmlElementNode& elem);
    std::string transpileIfTopLevel(const IfStatementNode& node);
    std::string transpileIfInsideElement(const IfStatementNode& node);
    std::string transpileForTopLevel(const ForStatementNode& node);
    std::string transpileForInsideElement(const ForStatementNode& node);
    std::string transpileWhileTopLevel(const WhileStatementNode& node);
    std::string transpileWhileInsideElement(const WhileStatementNode& node);
    std::string transpileShow(const ShowStatementNode& node);


    // Helper for child nodes
    std::string transpileChildren(const std::vector<std::unique_ptr<ASTNode>>& children, bool insideElement);
    std::string generateClientManifest(const std::vector<std::unique_ptr<ASTNode>>& program);

    // Insert minimal <script> for placeholders
    std::string generateScriptBlock();
    bool isVoidElement(const std::string& tagName) const;
    std::string escapeHTML(const std::string& input);
    std::string escapeJS(const std::string& input);
};

#endif // JTML_TRANSPILER_H
