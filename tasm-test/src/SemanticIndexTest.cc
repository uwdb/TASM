#include "SemanticIndex.h"
#include <gtest/gtest.h>

using namespace tasm;

class SemanticIndexTestFixture : public testing::Test {
public:
    SemanticIndexTestFixture() {}
};

TEST_F(SemanticIndexTestFixture, testCreateDB) {
    std::filesystem::path labels = "testLabels.db";

    // Delete DB at path.
    std::filesystem::remove(labels);
    SemanticIndexSQLite semanticIndex(labels);
    std::filesystem::remove(labels);
}