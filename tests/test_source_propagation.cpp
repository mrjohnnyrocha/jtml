#include <gtest/gtest.h>
#include "jtml/friendly.h"
#include "jtml/parser.h"
#include "jtml/expression_ir.h"
#include "jtml/runtime_plan.h"
#include "cli/util.h"

namespace {
std::vector<std::unique_ptr<ASTNode>> parse(const jtml::MappedSource& source, bool friendly) {
    jtml::cli::CompilationSource input;
    input.source = source;
    input.syntax = friendly ? jtml::SyntaxMode::Friendly : jtml::SyntaxMode::Classic;
    auto normalized = jtml::cli::normalizeCompilationSource(input);
    Lexer lexer(normalized.text, normalized.provenance);
    Parser parser(lexer.tokenize());
    auto program = parser.parseProgram();
    EXPECT_TRUE(lexer.getErrors().empty());
    EXPECT_TRUE(parser.getErrors().empty());
    return program;
}
void expectOrigin(const SourceSpan& span, const std::string& file, size_t line, size_t column, bool exact) {
    ASSERT_TRUE(span.original);
    EXPECT_EQ(span.original->source->file, file);
    EXPECT_EQ(span.original->source->coordinates(span.original->begin), std::make_pair(line, column));
    EXPECT_EQ(span.originalExact, exact);
}
}

TEST(SourcePropagation, ClassicLiteralUsesOriginalLexemeBytesAndCloneRetainsOrigin) {
    // Decoded text contains a newline and emoji. Its source extent includes the
    // quote/escape bytes, independently of the decoded token's text length.
    const std::string source = "define value = \"a\\n😀\"\\\\\n";
    auto program = parse(jtml::MappedSource::original("literal.jtml", source), false);
    ASSERT_EQ(program.size(), 1);
    const auto& definition = static_cast<const DefineStatementNode&>(*program[0]);
    expectOrigin(definition.expression->sourceSpan, "literal.jtml", 1, 16, true);
    EXPECT_EQ(definition.expression->sourceSpan.original->source->text.substr(
        definition.expression->sourceSpan.original->begin,
        definition.expression->sourceSpan.original->end - definition.expression->sourceSpan.original->begin), "\"a\\n😀\"");
    auto clone = definition.clone();
    auto& cloned = static_cast<DefineStatementNode&>(*clone);
    expectOrigin(cloned.sourceSpan, "literal.jtml", 1, 1, true);
    expectOrigin(cloned.expression->sourceSpan, "literal.jtml", 1, 16, true);
    const auto ir = jtml::buildExpressionIr(cloned.expression.get());
    expectOrigin(ir->sourceSpan, "literal.jtml", 1, 16, true);
}

TEST(SourcePropagation, FriendlyGeneratedNodesHaveAttributionRatherThanExactOriginalTokens) {
    auto program = parse(jtml::MappedSource::original("friendly.jtml", "jtml 2\n\nlet value = 1 + 2\n"), true);
    ASSERT_EQ(program.size(), 1);
    const auto& definition = static_cast<const DefineStatementNode&>(*program[0]);
    expectOrigin(definition.sourceSpan, "friendly.jtml", 3, 1, false);
    auto clone = definition.expression->clone();
    auto ir = jtml::buildExpressionIr(clone.get());
    expectOrigin(ir->sourceSpan, "friendly.jtml", 3, 1, false);
    EXPECT_EQ(ir->sourceSpan.provenance, definition.sourceSpan.provenance);
}

TEST(SourcePropagation, RuntimeBodyRetainsHistoricalOriginalWriteAfterAstClone) {
    const auto file = (std::filesystem::path(JTML_SOURCE_DIR) /
        "tests/fixtures/production/taskboard-rejections/borrowed-alias.jtml").string();
    auto program = jtml::cli::parseProgramFromFile(file);
    std::vector<std::unique_ptr<ASTNode>> cloned;
    for (const auto& node : program) cloned.push_back(node->clone());
    const auto semantic = jtml::analyzeSemanticProgram(cloned, jtml::cli::readFile(file));
    auto plan = jtml::buildRuntimePlan(cloned, semantic);
    bool found = false;
    for (const auto& component : plan.componentDefinitions) {
        for (const auto& node : component.bodyPlan) {
            if (node.text == "alias.inner.score = 1") {
                found = true;
                expectOrigin(node.sourceSpan, file, 9, 5, true);
            }
        }
    }
    EXPECT_TRUE(found);
}

TEST(SourcePropagation, SyntheticInputAndDiscontinuousRangesCannotAcquireExactOrigins) {
    auto input = jtml::MappedSource::original("user.jtml", "jtml 2\nlet value = 1\n");
    input.appendSynthetic("let generated = 2\n");
    auto program = parse(input, true);
    ASSERT_EQ(program.size(), 2);
    expectOrigin(program[0]->sourceSpan, "user.jtml", 2, 1, false);
    EXPECT_FALSE(program[1]->sourceSpan.original);
    auto mixed = jtml::MappedSource::original("one", "define ");
    mixed.append(jtml::MappedSource::original("two", "value = 1\\\\\n"));
    program = parse(mixed, false);
    ASSERT_EQ(program.size(), 1);
    EXPECT_FALSE(program[0]->sourceSpan.original);
}

TEST(SourcePropagation, RepeatedNestedExpressionsRetainDistinctRangesThroughCloneAndIr) {
    auto program = parse(jtml::MappedSource::original("nested.jtml",
        "define result = items[index + 1].value + items[index + 1].value\\\\\n"), false);
    ASSERT_EQ(program.size(), 1);
    auto clone = program[0]->clone();
    auto& definition = static_cast<DefineStatementNode&>(*clone);
    auto ir = jtml::buildExpressionIr(definition.expression.get());
    ASSERT_EQ(ir->children.size(), 2);
    expectOrigin(ir->children[0]->sourceSpan, "nested.jtml", 1, 17, true);
    expectOrigin(ir->children[1]->sourceSpan, "nested.jtml", 1, 42, true);
    ASSERT_EQ(ir->children[0]->children.size(), 1);
    const auto& subscript = ir->children[0]->children[0];
    ASSERT_EQ(subscript->children.size(), 2);
    expectOrigin(subscript->children[1]->sourceSpan, "nested.jtml", 1, 23, true);
}

TEST(SourcePropagation, InterpolationDerivedOperationsNeverClaimExactAuthoredExpressions) {
    auto program = parse(jtml::MappedSource::original("interpolation.jtml",
        "define value = \"Hello {person.name}\"\\\\\n"), false);
    ASSERT_EQ(program.size(), 1);
    auto& definition = static_cast<DefineStatementNode&>(*program[0]);
    auto ir = jtml::buildExpressionIr(definition.expression.get());
    expectOrigin(ir->sourceSpan, "interpolation.jtml", 1, 16, false);
    ASSERT_EQ(ir->children.size(), 2);
    expectOrigin(ir->children[1]->sourceSpan, "interpolation.jtml", 1, 16, false);
}
