#include "SemanticIndex.h"

#include <cassert>
#include <iostream>

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))
#define ASSERT_SQLITE_DONE(i) (assert(i == SQLITE_DONE))

namespace tasm {

void SemanticIndexSQLite::openDatabase(const std::filesystem::path &dbPath) {
    if (!std::filesystem::exists(dbPath))
        createDatabase(dbPath);
    else
        ASSERT_SQLITE_OK(sqlite3_open_v2(dbPath.c_str(), &db_, SQLITE_OPEN_READWRITE, NULL));

    return;
}

void SemanticIndexSQLite::createDatabase(const std::filesystem::path &dbPath) {
    ASSERT_SQLITE_OK(sqlite3_open_v2(dbPath.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));

    const char *createTable = "CREATE TABLE labels (" \
                                "video text not null, " \
                                "label text not null, " \
                                "frame int not null, " \
                                "x1 int not null, " \
                                "y1 int not null, " \
                                "x2 int not null, " \
                                "y2 int not null);";

    char *error = nullptr;
    auto result = sqlite3_exec(db_, createTable, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating table" << std::endl;
        sqlite3_free(error);
    }

    // Create index on video, label, frame.
    const char *createIndex = "CREATE INDEX video_index ON labels (video, label, frame)";
    result = sqlite3_exec(db_, createIndex, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating index" << std::endl;
        sqlite3_free(error);
    }
}

void SemanticIndexSQLite::closeDatabase() {
    ASSERT_SQLITE_OK(sqlite3_close(db_));
}

void SemanticIndexSQLite::initializeStatements() {
    // addMetadataStmt_
    std::string query = "INSERT INTO labels (video, label, frame, x1, y1, x2, y2) VALUES (?, ?, ?, ?, ?, ?, ?)";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &addMetadataStmt_, nullptr));
}

void SemanticIndexSQLite::destroyStatements() {
    ASSERT_SQLITE_OK(sqlite3_finalize(addMetadataStmt_));
}

void SemanticIndexSQLite::addMetadata(
        const std::string &video,
        const std::string &label,
        unsigned int frame,
        unsigned int x1,
        unsigned int y1,
        unsigned int x2,
        unsigned int y2) {

    ASSERT_SQLITE_OK(sqlite3_bind_text(addMetadataStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_text(addMetadataStmt_, 2, label.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(addMetadataStmt_, 3, frame));
    ASSERT_SQLITE_OK(sqlite3_bind_int(addMetadataStmt_, 4, x1));
    ASSERT_SQLITE_OK(sqlite3_bind_int(addMetadataStmt_, 5, y1));
    ASSERT_SQLITE_OK(sqlite3_bind_int(addMetadataStmt_, 6, x2));
    ASSERT_SQLITE_OK(sqlite3_bind_int(addMetadataStmt_, 7, y2));

    ASSERT_SQLITE_DONE(sqlite3_step(addMetadataStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(addMetadataStmt_));
}

} // namespace tasm