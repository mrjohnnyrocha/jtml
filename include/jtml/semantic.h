#pragma once

#include "jtml/attribute_classifier.h"
#include "jtml/ast.h"

#include <memory>
#include <string>
#include <vector>

namespace jtml {

struct AttributeKindCounts {
    int literal = 0;
    int boolean = 0;
    int reactive = 0;
    int event = 0;
    int special = 0;
    int passthrough = 0;
};

struct SemanticEdge {
    std::string from;
    std::string to;
    std::string kind;
};

struct SemanticUiModifier {
    std::string primitive;
    std::string modifier;
    std::string value;
};

struct SemanticUiUse {
    std::string primitive;
    std::string tagName;
    bool hasTitle = false;
    bool hasAriaLabel = false;
    bool hasLabel = false;
    bool hasControl = false;
    bool hasActionBinding = false;
    bool hasNavigationTarget = false;
    bool hasTabChild = false;
    bool hasDismissAction = false;
};

struct SemanticRoute {
    std::string path;
    std::string component;
    std::vector<std::string> params;
    std::vector<std::string> loads;
};

struct SemanticFetch {
    std::string name;
    std::string url;
    std::string method;
    std::string bodyExpr;
    std::string refreshAction;
    std::string cache;
    std::string credentials;
    std::string timeoutMs;
    std::string retryCount;
    std::string stalePolicy;
    std::string group;
    std::string cacheKeyExpr;
    std::string revalidateMs;
    bool dedupe = false;
    bool background = false;
    bool lazy = false;
};

struct SemanticImport {
    std::string specifier;
    std::string kind;
    std::vector<std::string> names;
    bool reExport = false;
};

struct SemanticProperty {
    std::string name;
    std::string value;
};

struct SemanticComponentDefinition {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> localState;
    std::vector<std::string> localDerived;
    std::vector<std::string> localActions;
    std::vector<std::string> localEffects;
    std::vector<std::string> eventBindings;
    std::string bodyHex;
    bool hasSlot = false;
    int bodyNodeCount = 0;
    int rootTemplateNodeCount = 0;
    int slotCount = 0;
    int sourceLine = 0;
};

struct SemanticComponentInstance {
    std::string id;
    std::string component;
    int instanceId = 0;
    std::string role;
    std::vector<SemanticProperty> params;
    std::vector<SemanticProperty> locals;
    int sourceLine = 0;
};

struct SemanticProgram {
    std::vector<std::string> moduleFiles;
    std::vector<std::string> state;
    std::vector<std::string> constants;
    std::vector<std::string> derived;
    std::vector<std::string> actions;
    std::vector<std::string> components;
    std::vector<SemanticComponentDefinition> componentDefinitions;
    std::vector<SemanticComponentInstance> componentInstances;
    std::vector<std::string> routes;
    std::vector<SemanticRoute> routeRecords;
    std::vector<std::string> fetches;
    std::vector<SemanticFetch> fetchRecords;
    std::vector<std::string> stores;
    std::vector<std::string> effects;
    std::vector<std::string> imports;
    std::vector<SemanticImport> importRecords;
    std::vector<std::string> externs;
    std::vector<std::string> uiPrimitives;
    std::vector<SemanticUiUse> uiUses;
    std::vector<SemanticUiModifier> uiModifiers;
    std::vector<SemanticEdge> dependencies;
    AttributeKindCounts attributes;
    int rawStyleAttributeCount = 0;
    int semanticPrimitiveRawStyleCount = 0;
    int styleBlocks = 0;
    int rawCssBlocks = 0;
    int rawHtmlBlocks = 0;
    int authorThemeTokenCount = 0;
    int themeTokenCount = 0;
    int timelineCount = 0;
};

struct SemanticActionProfile {
    std::string name;
    std::vector<std::string> writes;
    std::vector<std::string> reads;
    std::vector<std::string> triggers;
    bool hasVisibleEffect = false;
};

struct SemanticUsageWarning {
    std::string code;
    std::string message;
};

struct SemanticUsageReport {
    std::vector<std::string> observedState;
    std::vector<std::string> deadState;
    std::vector<std::string> zombieState;
    std::vector<std::string> observedDerived;
    std::vector<std::string> unusedDerived;
    std::vector<std::string> boundActions;
    std::vector<std::string> unboundActions;
    std::vector<std::string> unproductiveActions;
    std::vector<SemanticActionProfile> actionProfiles;
    std::vector<SemanticUsageWarning> warnings;
};

SemanticProgram analyzeSemanticProgram(
    const std::vector<std::unique_ptr<ASTNode>>& program,
    const std::string& originalSource = "");

SemanticUsageReport analyzeSemanticUsage(const SemanticProgram& semantic);

} // namespace jtml
