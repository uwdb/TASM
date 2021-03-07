#include "LayoutDatabase.h"
#include "Files.h"
#include <gtest/gtest.h>

namespace tasm {

    class DatabaseTestFixture : public ::testing::Test {
    public:
        DatabaseTestFixture() {}

    protected:
        void SetUp() {
            LayoutDatabase::instance()->open();
        }
        void TearDown() {
            std::experimental::filesystem::remove(EnvironmentConfiguration::instance().defaultLayoutDatabasePath());
        }
    };

    TEST_F(DatabaseTestFixture, testOpenDb) {
        std::shared_ptr<LayoutDatabase> layoutDatabase = LayoutDatabase::instance();

        std::experimental::filesystem::path layouts = EnvironmentConfiguration::instance().defaultLayoutDatabasePath();
        assert(std::experimental::filesystem::exists(layouts));
        std::experimental::filesystem::remove(layouts);

        layoutDatabase->open();
        assert(std::experimental::filesystem::exists(layouts));
    }

    TEST_F(DatabaseTestFixture, testAddSelectLayout) {
        std::shared_ptr<LayoutDatabase> layoutDatabase = LayoutDatabase::instance();

        std::string video = "video";
        unsigned int id = 0;

        std::vector<unsigned int> heights{1, 2, 3};
        std::vector<unsigned int> widths{1, 2, 3, 4};
        unsigned int numColumns = widths.size();
        unsigned int numRows = heights.size();
        std::shared_ptr<TileLayout> tileLayout = std::make_shared<TileLayout>(numColumns, numRows, widths, heights);

        layoutDatabase->addTileLayout(video, id, 40, 50, tileLayout);

        std::shared_ptr<TileLayout> selectedLayout = layoutDatabase->tileLayoutForId(video, id);
        assert(selectedLayout->numberOfColumns() == numColumns);
        assert(selectedLayout->numberOfRows() == numRows);
        assert(selectedLayout->widthsOfColumns() == widths);
        assert(selectedLayout->heightsOfRows() == heights);
        assert(*selectedLayout == *tileLayout);
    }

    TEST_F(DatabaseTestFixture, testSelectLayoutIds) {
        std::shared_ptr<LayoutDatabase> layoutDatabase = LayoutDatabase::instance();

        std::string video = "video";

        std::vector<unsigned int> heights{1, 2, 3};
        std::vector<unsigned int> widths{1, 2, 3, 4};
        unsigned int numColumns = widths.size();
        unsigned int numRows = heights.size();
        std::shared_ptr<TileLayout> tileLayout = std::make_shared<TileLayout>(numColumns, numRows, widths, heights);

        std::vector<int> layoutIds{0, 2, 4, 7};

        for (auto &id : layoutIds) {
            layoutDatabase->addTileLayout(video, id, id, id + 10, tileLayout);
        }

        // add an id that won't be selected, since it doesn't include frame 10
        layoutDatabase->addTileLayout(video, 10, 11, 12, tileLayout);

        std::vector<int> foundLayoutIds = layoutDatabase->tileLayoutIdsForFrame(video, 10);
        std::sort(layoutIds.begin(), layoutIds.end());
        std::sort(foundLayoutIds.begin(), foundLayoutIds.end());
        assert(layoutIds == foundLayoutIds);
    }

    TEST_F(DatabaseTestFixture, testTotalWidthHeight) {
        std::shared_ptr<LayoutDatabase> layoutDatabase = LayoutDatabase::instance();

        std::string name = "name";
        std::vector<unsigned int> heights;
        std::vector<unsigned int> widths;
        unsigned int maxHeight = 10;
        unsigned int maxWidth = 7;
        unsigned int totalHeight = 0;
        unsigned int totalWidth = 0;

        for (unsigned int i = 0; i <= maxHeight; ++i) {
            heights.push_back(i);
            totalHeight += i;
        }

        for (unsigned int i = 0; i <= maxWidth; ++i) {
            widths.push_back(i);
            totalWidth += i;
        }

        int tileNumber = 4;
        unsigned int id = 2;
        unsigned int firstFrame = 40;
        unsigned int lastFrame = 59;
        unsigned int numColumns = widths.size();
        unsigned int numRows = heights.size();
        std::shared_ptr<TileLayout> tileLayout = std::make_shared<TileLayout>(numColumns, numRows, widths, heights);

        layoutDatabase->addTileLayout(name, id, firstFrame, lastFrame, tileLayout);

        assert(layoutDatabase->totalWidth(name) == totalWidth);
        assert(layoutDatabase->totalHeight(name) == totalHeight);
        assert(layoutDatabase->largestWidth(name) == maxWidth);
        assert(layoutDatabase->largestHeight(name) == maxHeight);
        assert(layoutDatabase->maximumFrame(name) == lastFrame);
    }
} // namespace tasm
