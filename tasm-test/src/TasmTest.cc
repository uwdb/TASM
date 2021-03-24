#include "Tasm.h"
#include "Video.h"
#include <gtest/gtest.h>
#include "sqlite3.h"
#include "LayoutDatabase.h"
#include "EnvironmentConfiguration.h"

using namespace tasm;

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))

void countEntriesInDB(const std::experimental::filesystem::path &dbPath, int (*callback)(void*,int,char**,char**)) {
    sqlite3 *db;
    ASSERT_SQLITE_OK(sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READONLY, NULL));

    std::string select = "select count(*) from labels;";
    ASSERT_SQLITE_OK(sqlite3_exec(db, select.c_str(), callback, nullptr, nullptr));

    ASSERT_SQLITE_OK(sqlite3_close(db));
}

class TasmTestFixture : public testing::Test {
public:
    TasmTestFixture() {}

protected:
    void SetUp() {
        // set test configuration
        std::unordered_map<std::string, std::string> options {
                {EnvironmentConfiguration::DefaultLayoutsDB, "test-layout.db"},
                {EnvironmentConfiguration::CatalogPath, "test-resources"},
                {EnvironmentConfiguration::DefaultLabelsDB, "test-labels.db"}
        };
        auto config = EnvironmentConfiguration::instance(EnvironmentConfiguration(options));
        std::experimental::filesystem::remove_all(config.defaultLayoutDatabasePath());
        std::experimental::filesystem::remove_all(config.catalogPath());
        std::experimental::filesystem::remove_all(config.defaultLabelsDatabasePath());

        // copy resources from tasm-manager-test-resources
        std::experimental::filesystem::copy("test-inputs/tasm-test-resources/test-layout.db", config.defaultLayoutDatabasePath());
        std::experimental::filesystem::copy("test-inputs/tasm-test-resources/test-labels.db", config.defaultLabelsDatabasePath());
        std::experimental::filesystem::copy("test-inputs/tasm-test-resources/test-resources", config.catalogPath(), std::experimental::filesystem::copy_options::recursive);

        LayoutDatabase::instance()->open();
    }
    void TearDown() {
        // remove test resources
        auto config = EnvironmentConfiguration::instance();
        std::experimental::filesystem::remove_all(config.defaultLayoutDatabasePath());
        std::experimental::filesystem::remove_all(config.catalogPath());
        std::experimental::filesystem::remove_all(config.defaultLabelsDatabasePath());
    }
};

TEST_F(TasmTestFixture, DISABLED_createResources) {
    // For reference purposes, not use; SetUp and TearDown need to be commented out for resources to persist
    std::unordered_map<std::string, std::string> options {
            {EnvironmentConfiguration::DefaultLayoutsDB, "test-inputs/tasm-test-resources/test-layout.db"},
            {EnvironmentConfiguration::CatalogPath, "test-inputs/tasm-test-resources/test-resources"},
            {EnvironmentConfiguration::DefaultLabelsDB, "test-inputs/tasm-test-resources/test-labels.db"}
    };
    auto config = EnvironmentConfiguration::instance(EnvironmentConfiguration(options));
    std::experimental::filesystem::remove(config.defaultLayoutDatabasePath());
    std::experimental::filesystem::remove_all(config.catalogPath());
    std::experimental::filesystem::remove(config.defaultLabelsDatabasePath());

    tasm::TASM tasm;
    tasm.storeWithUniformLayout("/home/maureen/red_6sec_2k.mp4", "birdsincage-2x2", 2, 2);
    tasm.storeWithUniformLayout("/home/maureen/red102k.mp4", "red10-2x2", 2, 2);
}

TEST_F(TasmTestFixture, testAddMetadata) {
    std::experimental::filesystem::path path("testLabels.db");
    std::experimental::filesystem::remove(path);

    tasm::TASM tasm(path);
    tasm.addMetadata("video", "label", 0, 0, 0, 0, 0);

    countEntriesInDB(path, [](void *, int numColumns, char **values, char **columnNames) {
        assert(numColumns == 1);
        assert(!strcmp(values[0], "1"));
        return 0;
    });

    tasm.addMetadata("video", "label", 1, 0, 0, 0, 0);

    countEntriesInDB(path, [](void *, int numColumns, char **values, char **columnNames) {
        assert(numColumns == 1);
        assert(!strcmp(values[0], "2"));
        return 0;
    });

    std::experimental::filesystem::remove(path);
}

TEST_F(TasmTestFixture, testScan) {
    tasm::TASM tasm(SemanticIndex::IndexType::InMemory);
    tasm.addMetadata("red10", "red", 0, 0, 0, 100, 100);
}

TEST_F(TasmTestFixture, testSelectDifferentPath) {
    tasm::TASM tasm(SemanticIndex::IndexType::InMemory);
    tasm.addMetadata("red10", "red", 0, 0, 0, 100, 100);
    tasm.select("red10-2x2", "red", "red10");
}

TEST_F(TasmTestFixture, testTileBird) {
    tasm::TASM tasm(SemanticIndex::IndexType::InMemory);
    std::string video("birdsincage");
    std::string label("bird");
    for (int i = 0; i < 10; ++i)
        tasm.addMetadata(video, label, i, 0, 0, 100, 100);

    tasm.storeWithNonUniformLayout("test-inputs/tasm-test-resources/red_6sec_2k.mp4", "birdsincage-not-forced", video, label, false);
    tasm.storeWithNonUniformLayout("test-inputs/tasm-test-resources/red_6sec_2k.mp4", "birdsincage-forced", video, label, true);

    std::experimental::filesystem::remove_all(tasm::files::PathForVideo("birdsincage-not-forced"));
    std::experimental::filesystem::remove_all(tasm::files::PathForVideo("birdsincage-forced"));
}

TEST_F(TasmTestFixture, testSelectBird) {
    tasm::TASM tasm(SemanticIndex::IndexType::InMemory);

    auto selection = tasm.select("birdsincage-2x2", "bird", "birdsincage");
    ImagePtr next;
    auto count = 0u;
    while ((next = selection->next())) {
        ++count;
    }
    // assert(count == 360);
}

TEST_F(TasmTestFixture, testScanBirdsFullFrame) {
    tasm::TASM tasm(SemanticIndex::IndexType::XY, "/home/maureen/home_videos/birds_tasm.db");
    auto selection = tasm.selectFrames("birdsincage-2x2", "bird", 0, 5, "birds");
    auto count = 0u;
    ImagePtr next;
    while ((next = selection->next()))
        ++count;
    std::cout << "Tiled vid: retrieved " << count << " frames" << std::endl;
}
