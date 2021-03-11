#ifndef TASM_LAYOUTDATABASE_H
#define TASM_LAYOUTDATABASE_H

#include <experimental/filesystem>
#include "sqlite3.h"
#include "TileLayout.h"
#include "Video.h"
#include "Files.h"
#include "EnvironmentConfiguration.h"
#include <gtest/gtest.h>

namespace tasm {

class LayoutDatabase {
public:
    static std::shared_ptr<LayoutDatabase> instance() {
        if (!instance_) {
            LayoutDatabase database = LayoutDatabase();
            database.open();
            instance_ = std::make_shared<LayoutDatabase>(database);
        }
        return instance_;
    }

    std::vector<int> tileLayoutIdsForFrame(const std::string &video, unsigned int frameNumber) const;
    std::shared_ptr<TileLayout> tileLayoutForId(const std::string &video, int id) const;
    std::experimental::filesystem::path locationOfTileForId(const std::shared_ptr<TiledEntry> entry, unsigned int tileNumber, int id) const;
    void addTileLayout(const std::string &video, unsigned int id, unsigned int firstFrame, unsigned int lastFrame, const std::shared_ptr<TileLayout> tileLayout);

    unsigned int totalWidth(const std::string &video) const;
    unsigned int totalHeight(const std::string &video) const;
    unsigned int largestWidth(const std::string &video) const;
    unsigned int largestHeight(const std::string &video) const;
    unsigned int maximumFrame(const std::string &video) const;
    void open();

    ~LayoutDatabase() {
        destroyStatements();
        closeDatabase();
    }

private:
    LayoutDatabase() {}
    void createTables();
    void createLayoutTable();
    void createWidthsTable();
    void createHeightsTable();
    void addTileBasicLayout(const std::string &video, unsigned int id, unsigned int firstFrame, unsigned int lastFrame, unsigned int numCols, unsigned int numRows);
    void addTileLayoutWidths(const std::string &video, unsigned int id, const std::vector<unsigned int> &widths);
    void addTileLayoutHeights(const std::string &video, unsigned int id, const std::vector<unsigned int> &heights);
    std::vector<unsigned int> widthsForId(const std::string &video, unsigned int id) const;
    std::vector<unsigned int> heightsForId(const std::string &video, unsigned int id) const;

    void initializeStatements();
    void destroyStatements();
    void closeDatabase();

    sqlite3 *db_;
    static std::shared_ptr<LayoutDatabase> instance_;

    // Statements
    sqlite3_stmt *selectLayoutIdsStmt_;
    sqlite3_stmt *selectLayoutsStmt_;
    sqlite3_stmt *selectWidthsStmt_;
    sqlite3_stmt *selectHeightsStmt_;
    sqlite3_stmt *insertLayoutsStmt_;
    sqlite3_stmt *insertWidthsStmt_;
    sqlite3_stmt *insertHeightsStmt_;
};

} // namespace tasm

#endif //TASM_LAYOUTDATABASE_H
