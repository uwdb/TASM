#include "SemanticIndex.h"
#include <gtest/gtest.h>

#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include <cassert>
#include <experimental/filesystem>
#include <unordered_set>

using namespace tasm;

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))

class SemanticIndexTestFixture : public testing::Test {
public:
    SemanticIndexTestFixture() {}
};

TEST_F(SemanticIndexTestFixture, testCreateDB) {
    std::experimental::filesystem::path labels = "testLabels.db";

    // Delete DB at path.
    std::experimental::filesystem::remove(labels);
    auto semanticIndex = SemanticIndexFactory::create(SemanticIndex::IndexType::XY, labels);
    std::experimental::filesystem::remove(labels);
}

TEST_F(SemanticIndexTestFixture, testSelectOrderedFrames) {
    auto semanticIndex = SemanticIndexFactory::createInMemory();

    std::string video("video");
    std::string label("fish");
    for (int i = 0; i < 10; ++i) {
        semanticIndex->addMetadata(video, label, i, 0, 0, 0, 0);
    }
    for (int i = 8; i < 20; ++i) {
        semanticIndex->addMetadata(video, "cat", i, 0, 0, 0, 0);
    }

    // Select all labels of fish.
    std::shared_ptr<MetadataSelection> selectFish(new SingleMetadataSelection("fish"));
    auto fishFrames = semanticIndex->orderedFramesForSelection(video, selectFish, std::shared_ptr<TemporalSelection>());
    std::vector<int> expectedFrames{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    assert(*fishFrames == expectedFrames);

    std::shared_ptr<TemporalSelection> singleFrameSelect(new EqualTemporalSelection(5));
    fishFrames = semanticIndex->orderedFramesForSelection(video, selectFish, singleFrameSelect);
    expectedFrames = std::vector<int>{5};
    assert(*fishFrames == expectedFrames);

    std::shared_ptr<TemporalSelection> rangeSelect(new RangeTemporalSelection(3, 9));
    fishFrames = semanticIndex->orderedFramesForSelection(video, selectFish, rangeSelect);
    expectedFrames = std::vector<int>{3, 4, 5, 6, 7, 8};
    assert(*fishFrames == expectedFrames);
}

TEST_F(SemanticIndexTestFixture, testSelectRectanglesForFrame) {
    auto semanticIndex = SemanticIndexFactory::createInMemory();

    std::string video("video");
    std::string label("fish");
    for (int i = 1; i < 10; ++i) {
        semanticIndex->addMetadata(video, label, i, 0, 0, 0, 0);
        semanticIndex->addMetadata(video, label, i, i, 0, 0, 0);
    }
    for (int i = 0; i < 20; ++i) {
        semanticIndex->addMetadata(video, "cat", i, 0, 0, 0, 0);
    }

    // Select all labels of fish.
    std::shared_ptr<MetadataSelection> selectFish(new SingleMetadataSelection("fish"));
    auto fishFrames = semanticIndex->rectanglesForFrame(video, selectFish, 1);
    assert(fishFrames->size() == 2);
    // Ordering isn't guaranteed.
    assert(fishFrames->front() == Rectangle(1, 0, 0, 0, 0));
    assert(fishFrames->back() == Rectangle(1, 1, 0, 0, 0));
}

std::unordered_set<std::string> InspectSchema(const std::experimental::filesystem::path &dbPath) {
    sqlite3 *db;
    ASSERT_SQLITE_OK(sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, NULL));

    std::string select = "pragma table_info(labels);";
    std::unordered_set<std::string> columnNames;
    ASSERT_SQLITE_OK(sqlite3_exec(db, select.c_str(), [](void *set, int, char **values, char**) {
            reinterpret_cast<std::unordered_set<std::string> *>(set)->insert(values[1]);
            return 0;
        }, &columnNames, nullptr));

    ASSERT_SQLITE_OK(sqlite3_close(db));
    return columnNames;
}

TEST_F(SemanticIndexTestFixture, testCreateIndexTypes) {
    std::experimental::filesystem::path dbPath = "semantic_index_test.db";
    EnvironmentConfiguration::instance(EnvironmentConfiguration({
        {EnvironmentConfiguration::DefaultLabelsDB, dbPath},
    }));

    // Set up: remove db if it exists.
    std::experimental::filesystem::remove(dbPath);

    // Create an in-memory DB.
    // The dbPath shouldn't exist.
    assert(!std::experimental::filesystem::exists(dbPath));
    auto inMemoryIndex = SemanticIndexFactory::createInMemory();
    assert(!std::experimental::filesystem::exists(dbPath));

    // Create a XY db.
    auto onDiskIndex = SemanticIndexFactory::create(SemanticIndex::IndexType::XY, dbPath);
    assert(std::experimental::filesystem::exists(dbPath));
    std::unordered_set<std::string> expectedSchema{"video", "label", "frame", "x1", "y1", "x2", "y2"};
    std::unordered_set<std::string> seenSchema = InspectSchema(dbPath);
    assert(expectedSchema == seenSchema);
    std::experimental::filesystem::remove(dbPath);

    // Create a WH db.
    onDiskIndex = SemanticIndexFactory::create(SemanticIndex::IndexType::LegacyWH, dbPath);
    assert(std::experimental::filesystem::exists(dbPath));
    expectedSchema = {"label", "frame", "x", "y", "width", "height"};
    seenSchema = InspectSchema(dbPath);
    assert(expectedSchema == seenSchema);
    std::experimental::filesystem::remove(dbPath);
}
