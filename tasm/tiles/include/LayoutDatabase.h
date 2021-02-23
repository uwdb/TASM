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

class LayoutDbConfiguration {
public:
    static const std::experimental::filesystem::path &LayoutDbPath() {
        return EnvironmentConfiguration::instance().defaultLayoutDatabasePath();
    }
};

class LayoutDatabase {
public:
    static std::shared_ptr<LayoutDatabase> &instance() {
        if (instance_ == nullptr) {
            instance_ = std::make_shared<LayoutDatabase>(LayoutDatabase());
            instance_->open();
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
private:
    LayoutDatabase() {}
    friend class DatabaseTestFixture;
    FRIEND_TEST(DatabaseTestFixture, testOpenDb);
    void open();
    void createTables();
    void createLayoutTable();
    void createWidthsTable();
    void createHeightsTable();
    void addTileBasicLayout(const std::string &video, unsigned int id, unsigned int firstFrame, unsigned int lastFrame, unsigned int numCols, unsigned int numRows);
    void addTileLayoutWidths(const std::string &video, unsigned int id, const std::vector<unsigned int> &widths);
    void addTileLayoutHeights(const std::string &video, unsigned int id, const std::vector<unsigned int> &heights);
    std::vector<unsigned int> widthsForId(const std::string &video, unsigned int id) const;
    std::vector<unsigned int> heightsForId(const std::string &video, unsigned int id) const;

    sqlite3 *db_;
    static std::shared_ptr<LayoutDatabase> instance_;
};

} // namespace tasm

#endif //TASM_LAYOUTDATABASE_H
