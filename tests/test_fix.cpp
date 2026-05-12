#include "jtml/fix.h"

#include <gtest/gtest.h>

TEST(Fix, AddsFriendlyHeaderForFriendlySource) {
    auto fixed = jtml::fixSource("page\n  h1 \"Hello\"\n", jtml::SyntaxMode::Auto);

    EXPECT_TRUE(fixed.changed);
    EXPECT_EQ(fixed.source, "jtml 2\n\npage\n  h1 \"Hello\"\n");
    ASSERT_FALSE(fixed.changes.empty());
    EXPECT_EQ(fixed.changes[0].code, "JTML_FIX_HEADER");
}

TEST(Fix, ReplacesTabsAndTrimsWhitespace) {
    auto fixed = jtml::fixSource("jtml 2\n\npage\n\th1 \"Hello\"   ", jtml::SyntaxMode::Friendly);

    EXPECT_TRUE(fixed.changed);
    EXPECT_EQ(fixed.source, "jtml 2\n\npage\n  h1 \"Hello\"\n");
}

TEST(Fix, DoesNotAddFriendlyHeaderToClassicSource) {
    auto fixed = jtml::fixSource("define x = 1\\\\\n", jtml::SyntaxMode::Auto);

    EXPECT_FALSE(fixed.source.rfind("jtml 2", 0) == 0);
}
