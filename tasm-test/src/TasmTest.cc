#include "Tasm.h"
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
    std::experimental::filesystem::path path("testLabels.db");
    std::experimental::filesystem::remove(path);

    tasm::TASM tasm(path);
    tasm.addMetadata("red10", "red", 0, 0, 0, 100, 100);

//    tasm.storeWithUniformLayout("/home/maureen/red102k.mp4", "red10-2x2", 2, 2);

    std::experimental::filesystem::remove(path);
}

TEST_F(TasmTestFixture, testSelectDifferentPath) {
    std::experimental::filesystem::path path("testLabels.db");
    std::experimental::filesystem::remove(path);

    tasm::TASM tasm(path);
    tasm.addMetadata("red10", "red", 0, 0, 0, 100, 100);

    tasm.select("red10-2x2", "red", "red10");

    std::experimental::filesystem::remove(path);
}