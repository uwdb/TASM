#include "Tasm.h"
#include "Video.h"
#include <gtest/gtest.h>
#include "sqlite3.h"

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
};

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
    tasm::TASM tasm(true);
    tasm.addMetadata("red10", "red", 0, 0, 0, 100, 100);

//    tasm.storeWithUniformLayout("/home/maureen/red102k.mp4", "red10-2x2", 2, 2);

}

TEST_F(TasmTestFixture, testSelectDifferentPath) {
    tasm::TASM tasm(true);
    tasm.addMetadata("red10", "red", 0, 0, 0, 100, 100);

    tasm.select("red10-2x2", "red", "red10");
}

TEST_F(TasmTestFixture, testTileBird) {
    tasm::TASM tasm(true);
    std::string video("birdsincage");
    std::string label("bird");
    for (int i = 0; i < 10; ++i)
        tasm.addMetadata(video, label, i, 0, 0, 100, 100);

    tasm.storeWithNonUniformLayout("/home/maureen/NFLX_dataset/BirdsInCage_hevc.mp4", "birdsincage-not-forced", video, label, false);
    tasm.storeWithNonUniformLayout("/home/maureen/NFLX_dataset/BirdsInCage_hevc.mp4", "birdsincage-forced", video, label, true);

    std::experimental::filesystem::remove_all(tasm::files::PathForVideo("birdsincage-not-forced"));
    std::experimental::filesystem::remove_all(tasm::files::PathForVideo("birdsincage-forced"));
}

TEST_F(TasmTestFixture, testTileElFuente1) {
    tasm::TASM tasm(true);
    tasm.storeWithNonUniformLayout("/home/maureen/NFLX_dataset/ElFuente1_hevc.mp4", "elfuente1-not-forced", "elfuente1", "person", false);
}

TEST_F(TasmTestFixture, testSelectBird) {
    tasm::TASM tasm(true);
    auto selection = tasm.select("birdsincage-bird", "bird", "birdsincage");
    ImagePtr next;
    auto count = 0u;
    while ((next = selection->next())) {
        ++count;
    }
    // assert(count == 360);
}
