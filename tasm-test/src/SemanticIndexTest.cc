#include "SemanticIndex.h"
#include <gtest/gtest.h>

#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include <cassert>
#include <experimental/filesystem>

using namespace tasm;

class SemanticIndexTestFixture : public testing::Test {
public:
    SemanticIndexTestFixture() {}
};

TEST_F(SemanticIndexTestFixture, testCreateDB) {
    std::experimental::filesystem::path labels = "testLabels.db";

    // Delete DB at path.
    std::experimental::filesystem::remove(labels);
    SemanticIndexSQLite semanticIndex(labels);
    std::experimental::filesystem::remove(labels);
}

TEST_F(SemanticIndexTestFixture, testSelectOrderedFrames) {
    SemanticIndexSQLiteInMemory semanticIndex;

    std::string video("video");
    std::string label("fish");
    for (int i = 0; i < 10; ++i) {
        semanticIndex.addMetadata(video, label, i, 0, 0, 0, 0);
    }
    for (int i = 8; i < 20; ++i) {
        semanticIndex.addMetadata(video, "cat", i, 0, 0, 0, 0);
    }

    // Select all labels of fish.
    std::shared_ptr<MetadataSelection> selectFish(new SingleMetadataSelection("fish"));
    auto fishFrames = semanticIndex.orderedFramesForSelection(video, selectFish, std::shared_ptr<TemporalSelection>());
    std::vector<int> expectedFrames{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    assert(*fishFrames == expectedFrames);

    std::shared_ptr<TemporalSelection> singleFrameSelect(new EqualTemporalSelection(5));
    fishFrames = semanticIndex.orderedFramesForSelection(video, selectFish, singleFrameSelect);
    expectedFrames = std::vector<int>{5};
    assert(*fishFrames == expectedFrames);

    std::shared_ptr<TemporalSelection> rangeSelect(new RangeTemporalSelection(3, 9));
    fishFrames = semanticIndex.orderedFramesForSelection(video, selectFish, rangeSelect);
    expectedFrames = std::vector<int>{3, 4, 5, 6, 7, 8};
    assert(*fishFrames == expectedFrames);
}

TEST_F(SemanticIndexTestFixture, testSelectRectanglesForFrame) {
    SemanticIndexSQLiteInMemory semanticIndex;

    std::string video("video");
    std::string label("fish");
    for (int i = 1; i < 10; ++i) {
        semanticIndex.addMetadata(video, label, i, 0, 0, 0, 0);
        semanticIndex.addMetadata(video, label, i, i, 0, 0, 0);
    }
    for (int i = 0; i < 20; ++i) {
        semanticIndex.addMetadata(video, "cat", i, 0, 0, 0, 0);
    }

    // Select all labels of fish.
    std::shared_ptr<MetadataSelection> selectFish(new SingleMetadataSelection("fish"));
    auto fishFrames = semanticIndex.rectanglesForFrame(video, selectFish, 1);
    assert(fishFrames->size() == 2);
    // Ordering isn't guaranteed.
    assert(fishFrames->front() == Rectangle(1, 0, 0, 0, 0));
    assert(fishFrames->back() == Rectangle(1, 1, 0, 0, 0));
}
