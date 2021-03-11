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
    ASSERT_SQLITE_OK(sqlite3_close(db_));
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
    // selectLayoutIdsStmt_
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

    // insertLayoutsStmt_
    query = "INSERT INTO layouts (video, id, first_frame, last_frame, num_rows, num_cols) VALUES (?, ?, ?, ?, ?, ?)";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &insertLayoutsStmt_, nullptr));

    // insertWidthsStmt_
    query = "INSERT INTO widths (video, id, width, position) VALUES ( ?, ?, ?, ?)";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &insertWidthsStmt_, nullptr));

    // insertHeightsStmt_
    query = "INSERT INTO heights (video, id, height, position) VALUES (?, ?, ?, ?)";
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &insertHeightsStmt_, nullptr));
}

void LayoutDatabase::destroyStatements() {
    ASSERT_SQLITE_OK(sqlite3_finalize(selectLayoutIdsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectLayoutsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectWidthsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(selectHeightsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(insertLayoutsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(insertWidthsStmt_));
    ASSERT_SQLITE_OK(sqlite3_finalize(insertHeightsStmt_));
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

    return std::make_shared<TileLayout>(TileLayout(numberOfCols, numberOfRows, widthsForId(video, id), heightsForId(video, id)));
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
    std::string query = "SELECT first_frame, last_frame FROM layouts WHERE video = ? AND id = ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, entry->name().c_str(), -1, SQLITE_STATIC));
    ASSERT_SQLITE_OK(sqlite3_bind_int(select, 2, id));

    int result;
    unsigned int firstFrame;
    unsigned int lastFrame;
    if ((result = sqlite3_step(select)) == SQLITE_ROW) {
        firstFrame = sqlite3_column_int(select, 0);
        lastFrame = sqlite3_column_int(select, 1);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(select));
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    std::experimental::filesystem::path directory = TileFiles::directoryForTilesInFrames(entry->path(), id, firstFrame, lastFrame);

    return TileFiles::tileFilename(directory, tileNumber);
}

void LayoutDatabase::addTileLayout(const std::string &video, unsigned int id, unsigned int firstFrame, unsigned int lastFrame, const std::shared_ptr<TileLayout> tileLayout) {
    addTileBasicLayout(video, id, firstFrame, lastFrame, tileLayout->numberOfColumns(), tileLayout->numberOfRows());
    addTileLayoutWidths(video, id, tileLayout->widthsOfColumns());
    addTileLayoutHeights(video, id, tileLayout->heightsOfRows());
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
    // select all ids for this video from layouts
    std::string query = "SELECT id FROM layouts WHERE video = ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));

    std::vector<unsigned int> ids;
    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int(select, 0));
    }

    ASSERT_SQLITE_DONE(result);
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    // for all ids, check that the total height is the same
    assert(ids.size() > 0);

    std::vector<unsigned int> heights = heightsForId(video, ids.at(0));
    unsigned int totalHeight = std::accumulate(heights.begin(), heights.end(), 0);
    for (unsigned int id : ids) {
        heights = heightsForId(video, id);
        assert(totalHeight == std::accumulate(heights.begin(), heights.end(), 0));
    }

    return totalHeight;
}

unsigned int LayoutDatabase::totalWidth(const std::string &video) const {
    // select all ids for this video from layouts
    std::string query = "SELECT id FROM layouts WHERE video = ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));

    std::vector<unsigned int> ids;
    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW) {
        ids.push_back(sqlite3_column_int(select, 0));
    }

    ASSERT_SQLITE_DONE(result);
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    // for all ids, check that the total width is the same

    assert(ids.size() > 0);

    std::vector<unsigned int> widths = widthsForId(video, ids.at(0));
    unsigned int totalWidth = std::accumulate(widths.begin(), widths.end(), 0);
    for (unsigned int id : ids) {
        widths = widthsForId(video, id);
        assert(totalWidth == std::accumulate(widths.begin(), widths.end(), 0));
    }

    return totalWidth;
}


unsigned int LayoutDatabase::largestWidth(const std::string &video) const {
    std::string query = "SELECT MAX(width) FROM widths WHERE video = ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));

    unsigned int largestWidth;
    int result;
    if ((result = sqlite3_step(select)) == SQLITE_ROW) {
        largestWidth = sqlite3_column_int(select, 0);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(select));
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    return largestWidth;
}
unsigned int LayoutDatabase::largestHeight(const std::string &video) const{
    std::string query = "SELECT MAX(height) FROM heights WHERE video = ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));

    unsigned int largestHeight;
    int result;
    if ((result = sqlite3_step(select)) == SQLITE_ROW) {
        largestHeight= sqlite3_column_int(select, 0);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(select));
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    return largestHeight;
}

unsigned int LayoutDatabase::maximumFrame(const std::string &video) const {
    std::string query = "SELECT MAX(last_frame) FROM layouts WHERE video = ?";
    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &select, nullptr));
    ASSERT_SQLITE_OK(sqlite3_bind_text(select, 1, video.c_str(), -1, SQLITE_STATIC));

    unsigned int maximumFrame;
    int result;
    if ((result = sqlite3_step(select)) == SQLITE_ROW) {
        maximumFrame = sqlite3_column_int(select, 0);
    }

    ASSERT_SQLITE_DONE(sqlite3_step(select));
    ASSERT_SQLITE_OK(sqlite3_finalize(select));

    return maximumFrame;
}

} // namespace tasm


