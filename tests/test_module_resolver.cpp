#include "jtml/module_resolver.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path makeTempRoot(const std::string& name) {
    auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void writeFile(const std::filesystem::path& path, const std::string& source) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << source;
}

} // namespace

TEST(ModuleResolver, ResolvesBarePackageFromNearestJtmlModules) {
    auto root = makeTempRoot("jtml-module-resolver-package");
    writeFile(root / "jtml_modules" / "ui" / "index.jtml", "jtml 2\n");
    writeFile(root / "app" / "pages" / "home.jtml", "jtml 2\n");

    auto resolved = jtml::resolveJtmlModulePath(
        "ui",
        root / "app" / "pages" / "home.jtml");

    EXPECT_EQ(std::filesystem::weakly_canonical(root / "jtml_modules" / "ui" / "index.jtml"),
              resolved);
}

TEST(ModuleResolver, KeepsRelativeImportsRelativeToImporter) {
    auto root = makeTempRoot("jtml-module-resolver-relative");
    writeFile(root / "app" / "components" / "card.jtml", "jtml 2\n");
    writeFile(root / "app" / "index.jtml", "jtml 2\n");

    auto resolved = jtml::resolveJtmlModulePath(
        "./components/card.jtml",
        root / "app" / "index.jtml");

    EXPECT_EQ(std::filesystem::weakly_canonical(root / "app" / "components" / "card.jtml"),
              resolved);
}
