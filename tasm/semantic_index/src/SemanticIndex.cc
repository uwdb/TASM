#include "SemanticIndex.h"

#include <cassert>
#include <iostream>

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))
#define ASSERT_SQLITE_DONE(i) (assert(i == SQLITE_DONE))

namespace tasm {

void SemanticIndexSQLite::openDatabase(const std::experimental::filesystem::path &dbPath) {
    if (!std::experimental::filesystem::exists(dbPath))
        createDatabase(dbPath);
    else
        ASSERT_SQLITE_OK(sqlite3_open_v2(dbPath.c_str(), &db_, SQLITE_OPEN_READWRITE, NULL));

    return;
}

void SemanticIndexSQLite::createDatabase(const std::experimental::filesystem::path &dbPath) {
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

std::unique_ptr<std::vector<int>> SemanticIndexSQLite::orderedFramesForSelection(
        const std::string &video,
        std::shared_ptr<MetadataSelection> metadataSelection,
        std::shared_ptr<TemporalSelection> temporalSelection) {
    std::string query = "SELECT frame FROM labels WHERE video = ? AND " + metadataSelection->labelConstraints();
    if (temporalSelection)
        query += " AND " + temporalSelection->frameConstraints();
    query += " ORDER BY frame ASC";

    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));

    auto frames = std::make_unique<std::vector<int>>();

    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW) {
        frames->push_back(sqlite3_column_int(select, 0));
    }

    assert(result == SQLITE_DONE);
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    return frames;
}

std::unique_ptr<std::list<Rectangle>> SemanticIndexSQLite::rectanglesForFrame(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int frame, unsigned int maxWidth, unsigned int maxHeight) {
    std::string query = "SELECT frame, x1, y1, x2, y2 FROM labels WHERE video = ? AND " + metadataSelection->labelConstraints() + " AND frame = ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(select, 2, frame));

    return rectanglesForQuery(select, maxWidth, maxHeight);
}

std::unique_ptr<std::list<Rectangle>> SemanticIndexSQLite::rectanglesForFrames(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int firstFrameInclusive, int lastFrameExclusive) {
    std::string query = "SELECT frame, x1, y1, x2, y2 FROM labels WHERE video = ? AND " + metadataSelection->labelConstraints() + " AND frame >= ? AND frame < ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(select, 2, firstFrameInclusive));
    ASSERT_SQLITE_OK(sqlite3_bind_int(select, 3, lastFrameExclusive));

    return rectanglesForQuery(select);
}

std::unique_ptr<std::list<Rectangle>> SemanticIndexSQLite::rectanglesForQuery(sqlite3_stmt *select, unsigned int maxWidth, unsigned int maxHeight) {
    auto rectangles = std::make_unique<std::list<Rectangle>>();
    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW) {
        unsigned int frame = sqlite3_column_int(select, 0);
        unsigned int x1 = sqlite3_column_int(select, 1);
        unsigned int y1 = sqlite3_column_int(select, 2);
        unsigned int x2 = sqlite3_column_int(select, 3);
        unsigned int y2 = sqlite3_column_int(select, 4);

        if (maxWidth)
            x2 = std::min(x2, maxWidth);

        if (maxHeight)
            y2 = std::min(y2, maxHeight);

        rectangles->emplace_back(frame, x1, y1, (x2 - x1), (y2 - y1));
    }

    ASSERT_SQLITE_DONE(result);
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    return rectangles;
}



} // namespace tasm