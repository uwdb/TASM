#include "LayoutDatabase.h"

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))
#define ASSERT_SQLITE_DONE(i) (assert(i == SQLITE_DONE))

namespace tasm {

std::shared_ptr<LayoutDatabase> LayoutDatabase::instance_ = nullptr;

void LayoutDatabase::open() {
    std::experimental::filesystem::path dbPath = EnvironmentConfiguration::instance().defaultLayoutDatabasePath();
    if (!std::experimental::filesystem::exists(dbPath)) {
        ASSERT_SQLITE_OK(sqlite3_open_v2(dbPath.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));
        createTables();
    } else {
        ASSERT_SQLITE_OK(sqlite3_open_v2(dbPath.c_str(), &db_, SQLITE_OPEN_READWRITE, NULL));
    }
    initializeStatements();
}

void LayoutDatabase::closeDatabase() {
    int result = sqlite3_close(db_);
    ASSERT_SQLITE_OK(result);
}

void LayoutDatabase::createTables() {
    createLayoutTable();
    createWidthsTable();
    createHeightsTable();
}

void LayoutDatabase::createLayoutTable() {
    const char *createTable = "CREATE TABLE layouts ("\
                        "video TEXT NOT NULL,"\
                        "id INT NOT NULL,"\
                        "first_frame INT NOT NULL,"\
                        "last_frame INT NOT NULL,"\
                        "num_rows INT NOT NULL,"\
                        "num_cols INT NOT NULL,"\
                        "PRIMARY KEY (video, id));";

    char *error = nullptr;
    int result = sqlite3_exec(db_, createTable, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating layouts table: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_free(error);
    }

    const char *createIndex = "CREATE INDEX layout_index ON layouts (video, first_frame, last_frame)";
    result = sqlite3_exec(db_, createIndex, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating layouts index: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_free(error);
    }
}

void LayoutDatabase::createWidthsTable() {
    const char *createWidthTable = "CREATE TABLE widths ("\
                            "video TEXT NOT NULL,"\
                            "id INT NOT NULL,"\
                            "width INT NOT NULL,"\
                            "position INT NOT NULL,"\
                            "FOREIGN KEY (video, id) REFERENCES layouts(video, id))";
    char *error = nullptr;
    int result = sqlite3_exec(db_, createWidthTable, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating layout width table: " << sqlite3_errmsg(db_) << std::endl;;
        sqlite3_free(error);
    }

    const char *createIndex = "CREATE INDEX widths_index ON widths(video, id)";
    result = sqlite3_exec(db_, createIndex, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating widths index: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_free(error);
    }
}

void LayoutDatabase::createHeightsTable() {
    const char *createHeightTable = "CREATE TABLE heights ("\
                            "video TEXT NOT NULL,"\
                            "id INT NOT NULL,"\
                            "height INT NOT NULL,"\
                            "position INT NOT NULL,"\
                            "FOREIGN KEY (video, id) REFERENCES layouts(video, id))";
    char *error = nullptr;
    int result = sqlite3_exec(db_, createHeightTable, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating layout height table: " << sqlite3_errmsg(db_) << std::endl;;
        sqlite3_free(error);
    }

    const char *createIndex = "CREATE INDEX heights_index ON heights(video, id)";
    result = sqlite3_exec(db_, createIndex, NULL, NULL, &error);
    if (result != SQLITE_OK) {
        std::cerr << "Error creating heights index: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_free(error);
    }
}

void LayoutDatabase::initializeStatements() {
    // selectLayoutIdsStmt
    std::string query = "SELECT id FROM layouts WHERE video = ? AND ? BETWEEN first_frame AND last_frame";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectLayoutIdsStmt_, nullptr));

    // selectLayoutsStmt
    query = "SELECT num_rows, num_cols FROM layouts WHERE video = ? AND id = ?";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectLayoutsStmt_, nullptr));

    // selectWidthsStmt
    query = "SELECT width FROM widths WHERE video = ? AND id = ? ORDER BY position ASC";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectWidthsStmt_, nullptr));

    // selectHeightsStmt
    query = "SELECT height FROM heights WHERE video = ? AND id = ? ORDER BY position ASC";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectHeightsStmt_, nullptr));

    // selectFirstLastFrameStmt
    query = "SELECT first_frame, last_frame FROM layouts WHERE video = ? AND id = ?";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectFirstLastFrameStmt_, nullptr));

    // selectIdStmt
    query = "SELECT id FROM layouts WHERE video = ? LIMIT 1";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectIdStmt_, nullptr));

    // insertLayoutsStmt
    query = "INSERT INTO layouts (video, id, first_frame, last_frame, num_rows, num_cols) VALUES (?, ?, ?, ?, ?, ?)";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &insertLayoutsStmt_, nullptr));

    // insertWidthsStmt
    query = "INSERT INTO widths (video, id, width, position) VALUES ( ?, ?, ?, ?)";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &insertWidthsStmt_, nullptr));

    // insertHeightsStmt
    query = "INSERT INTO heights (video, id, height, position) VALUES (?, ?, ?, ?)";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &insertHeightsStmt_, nullptr));

    // selectTotalWidthStmt
    query = "SELECT sum(width) FROM widths WHERE video = ? AND id = ?";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectTotalWidthStmt_, nullptr));

    // selectTotalHeightStmt
    query = "SELECT sum(height) FROM heights WHERE video = ? AND id = ?";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectTotalHeightStmt_, nullptr));

    // selectMaxWidthStmt
    query = "SELECT MAX(width) FROM widths WHERE video = ?";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectMaxWidthStmt_, nullptr));

    // selectMaxHeightStmt
    query = "SELECT MAX(height) FROM heights WHERE video = ?";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectMaxHeightStmt_, nullptr));

    // selectMaxFrameStmt
    query = "SELECT MAX(last_frame) FROM layouts WHERE video = ?";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &selectMaxFrameStmt_, nullptr));
}

void LayoutDatabase::destroyStatements() {
    ASSERT_SQLITE_OK(sqlite3_finalize(selectLayoutIdsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectLayoutsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectWidthsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectHeightsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(insertLayoutsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(insertWidthsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(insertHeightsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectFirstLastFrameStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectIdStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectTotalWidthStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectTotalHeightStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectMaxWidthStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectMaxHeightStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectMaxFrameStmt_));
}

std::vector<int> LayoutDatabase::tileLayoutIdsForFrame(const std::string &video, unsigned int frameNumber) const {
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectLayoutIdsStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(selectLayoutIdsStmt_, 2, frameNumber));

    int result;
    std::vector<int> layoutIds;
    while ((result = sqlite3_step(selectLayoutIdsStmt_)) == SQLITE_ROW) {
        layoutIds.push_back(sqlite3_column_int(selectLayoutIdsStmt_, 0));
    }

    ASSERT_SQLITE_DONE(result);
    ASSERT_SQLITE_OK(sqlite3_reset(selectLayoutIdsStmt_));

    assert(!layoutIds.empty());
    return layoutIds;
}

std::shared_ptr<TileLayout> LayoutDatabase::tileLayoutForId(const std::string &video, int id) const {
    // select number of rows and cols
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectLayoutsStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(selectLayoutsStmt_, 2, id));

    int result;
    unsigned int numberOfRows = 0;
    unsigned int numberOfCols = 0;
    if ((result = sqlite3_step(selectLayoutsStmt_)) == SQLITE_ROW) {
        numberOfRows = sqlite3_column_int(selectLayoutsStmt_, 0);
        numberOfCols = sqlite3_column_int(selectLayoutsStmt_, 1);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(selectLayoutsStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectLayoutsStmt_));

    return std::make_shared<TileLayout>(numberOfCols, numberOfRows, widthsForId(video, id), heightsForId(video, id));
}

std::vector<unsigned int> LayoutDatabase::widthsForId(const std::string &video, unsigned int id) const {
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectWidthsStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(selectWidthsStmt_, 2, id));

    int result;
    std::vector<unsigned int> widthsOfColumns;
    while ((result = sqlite3_step(selectWidthsStmt_)) == SQLITE_ROW) {
        widthsOfColumns.push_back(sqlite3_column_int(selectWidthsStmt_, 0));
    }

    ASSERT_SQLITE_DONE(result);
    ASSERT_SQLITE_OK(sqlite3_reset(selectWidthsStmt_));

    return widthsOfColumns;
}

std::vector<unsigned int> LayoutDatabase::heightsForId(const std::string &video, unsigned int id) const {
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectHeightsStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(selectHeightsStmt_, 2, id));

    int result;
    std::vector<unsigned int> heightsOfRows;
    while ((result = sqlite3_step(selectHeightsStmt_)) == SQLITE_ROW) {
        heightsOfRows.push_back(sqlite3_column_int(selectHeightsStmt_, 0));
    }

    ASSERT_SQLITE_DONE(result);
    ASSERT_SQLITE_OK(sqlite3_reset(selectHeightsStmt_));

    return heightsOfRows;
}

std::experimental::filesystem::path LayoutDatabase::locationOfTileForId(const std::shared_ptr<TiledEntry> entry, unsigned int tileNumber, int id) const {
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectFirstLastFrameStmt_, 1, entry->name().c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(selectFirstLastFrameStmt_, 2, id));

    int result;
    unsigned int firstFrame;
    unsigned int lastFrame;
    if ((result = sqlite3_step(selectFirstLastFrameStmt_)) == SQLITE_ROW) {
        firstFrame = sqlite3_column_int(selectFirstLastFrameStmt_, 0);
        lastFrame = sqlite3_column_int(selectFirstLastFrameStmt_, 1);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(selectFirstLastFrameStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectFirstLastFrameStmt_));

    std::experimental::filesystem::path directory = TileFiles::directoryForTilesInFrames(entry->path(), id, firstFrame, lastFrame);

    return TileFiles::tileFilename(directory, tileNumber);
}

void LayoutDatabase::addTileLayout(const std::string &video, unsigned int id, unsigned int firstFrame, unsigned int lastFrame, const std::shared_ptr<TileLayout> tileLayout) {
    sqlite3_exec(db_, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    addTileBasicLayout(video, id, firstFrame, lastFrame, tileLayout->numberOfColumns(), tileLayout->numberOfRows());
    addTileLayoutWidths(video, id, tileLayout->widthsOfColumns());
    addTileLayoutHeights(video, id, tileLayout->heightsOfRows());
    sqlite3_exec(db_, "END TRANSACTION;", NULL, NULL, NULL);
}

void LayoutDatabase::addTileBasicLayout(const std::string &video, unsigned int id, unsigned int firstFrame, unsigned int lastFrame, unsigned int numCols, unsigned int numRows) {
    ASSERT_SQLITE_OK(sqlite3_bind_text(insertLayoutsStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertLayoutsStmt_, 2, id));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertLayoutsStmt_, 3, firstFrame));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertLayoutsStmt_, 4, lastFrame));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertLayoutsStmt_, 5, numRows));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertLayoutsStmt_, 6, numCols));

    int result = sqlite3_step(insertLayoutsStmt_);
    ASSERT_SQLITE_DONE(result);
    ASSERT_SQLITE_OK(sqlite3_reset(insertLayoutsStmt_));
}

void LayoutDatabase::addTileLayoutWidths(const std::string &video, unsigned int id, const std::vector<unsigned int> &widths) {
    // should only be called within a transaction
    int position = 0;
    for (const auto& width : widths) {
        ASSERT_SQLITE_OK(sqlite3_bind_text(insertWidthsStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
        ASSERT_SQLITE_OK(sqlite3_bind_int(insertWidthsStmt_, 2, id));
        ASSERT_SQLITE_OK(sqlite3_bind_int(insertWidthsStmt_, 3, width));
        ASSERT_SQLITE_OK(sqlite3_bind_int(insertWidthsStmt_, 4, position));
        ASSERT_SQLITE_DONE(sqlite3_step(insertWidthsStmt_));
        ASSERT_SQLITE_OK(sqlite3_reset(insertWidthsStmt_));
        ++position;
    }
}

void LayoutDatabase::addTileLayoutHeights(const std::string &video, unsigned int id, const std::vector<unsigned int> &heights) {
    // should only be called within a transaction
    int position = 0;
    for (const auto& height : heights) {
        ASSERT_SQLITE_OK(sqlite3_bind_text(insertHeightsStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
        ASSERT_SQLITE_OK(sqlite3_bind_int(insertHeightsStmt_, 2, id));
        ASSERT_SQLITE_OK(sqlite3_bind_int(insertHeightsStmt_, 3, height));
        ASSERT_SQLITE_OK(sqlite3_bind_int(insertHeightsStmt_, 4, position));
        ASSERT_SQLITE_DONE(sqlite3_step(insertHeightsStmt_));
        ASSERT_SQLITE_OK(sqlite3_reset(insertHeightsStmt_));
        ++position;
    }
}

unsigned int LayoutDatabase::totalHeight(const std::string &video) const {
    // select an id for this video
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectIdStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    assert(sqlite3_step(selectIdStmt_) == SQLITE_ROW);
    unsigned int id = sqlite3_column_int(selectIdStmt_, 0);
    ASSERT_SQLITE_DONE(sqlite3_step(selectIdStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectIdStmt_));

    ASSERT_SQLITE_OK(sqlite3_bind_text(selectTotalHeightStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(selectTotalHeightStmt_, 2, id));
    assert(sqlite3_step(selectTotalHeightStmt_) == SQLITE_ROW);
    unsigned int totalHeight = sqlite3_column_int(selectTotalHeightStmt_, 0);

    ASSERT_SQLITE_DONE(sqlite3_step(selectTotalHeightStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectTotalHeightStmt_));

    return totalHeight;
}

unsigned int LayoutDatabase::totalWidth(const std::string &video) const {
    // select an id for this video
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectIdStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    assert(sqlite3_step(selectIdStmt_) == SQLITE_ROW);
    unsigned int id = sqlite3_column_int(selectIdStmt_, 0);
    ASSERT_SQLITE_DONE(sqlite3_step(selectIdStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectIdStmt_));

    ASSERT_SQLITE_OK(sqlite3_bind_text(selectTotalWidthStmt_, 1, video.c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(selectTotalWidthStmt_, 2, id));
    assert(sqlite3_step(selectTotalWidthStmt_) == SQLITE_ROW);
    unsigned int totalWidth = sqlite3_column_int(selectTotalWidthStmt_, 0);

    ASSERT_SQLITE_DONE(sqlite3_step(selectTotalWidthStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectTotalWidthStmt_));

    return totalWidth;
}

unsigned int LayoutDatabase::largestWidth(const std::string &video) const {
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectMaxWidthStmt_, 1, video.c_str(), -1, SQLITE_STATIC));

    unsigned int largestWidth;
    int result;
    if ((result = sqlite3_step(selectMaxWidthStmt_)) == SQLITE_ROW) {
        largestWidth = sqlite3_column_int(selectMaxWidthStmt_, 0);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(selectMaxWidthStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectMaxWidthStmt_));

    return largestWidth;
}
unsigned int LayoutDatabase::largestHeight(const std::string &video) const{
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectMaxHeightStmt_, 1, video.c_str(), -1, SQLITE_STATIC));

    unsigned int largestHeight;
    int result;
    if ((result = sqlite3_step(selectMaxHeightStmt_)) == SQLITE_ROW) {
        largestHeight= sqlite3_column_int(selectMaxHeightStmt_, 0);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(selectMaxHeightStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectMaxHeightStmt_));

    return largestHeight;
}

unsigned int LayoutDatabase::maximumFrame(const std::string &video) const {
    ASSERT_SQLITE_OK(sqlite3_bind_text(selectMaxFrameStmt_, 1, video.c_str(), -1, SQLITE_STATIC));

    unsigned int maximumFrame;
    int result;
    if ((result = sqlite3_step(selectMaxFrameStmt_)) == SQLITE_ROW) {
        maximumFrame = sqlite3_column_int(selectMaxFrameStmt_, 0);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(selectMaxFrameStmt_));
    ASSERT_SQLITE_OK(sqlite3_reset(selectMaxFrameStmt_));

    return maximumFrame;
}

} // namespace tasm


