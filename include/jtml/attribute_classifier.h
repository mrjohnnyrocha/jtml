#pragma once

#include "jtml/ast.h"
#include <string>

enum class JtmlAttributeKind {
    Literal,
    Boolean,
    Reactive,
    Event,
    Special,
    Passthrough,
};

struct JtmlAttributeClassification {
    JtmlAttributeKind kind = JtmlAttributeKind::Reactive;
    std::string literalValue;
};

bool isJtmlEventAttribute(const std::string& attrName);
bool jtmlEventCarriesElementValue(const std::string& attrName);
bool jtmlStaticAttributeValue(const ExpressionStatementNode* expr, std::string& value);
JtmlAttributeClassification classifyJtmlAttribute(const JtmlAttribute& attr);

