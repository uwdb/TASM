#include "Catalog.h"
#include "CrackingOperators.h"
#include "Environment.h"
#include "HeuristicOptimizer.h"
#include "Metadata.h"
#include "SelectPixels.h"
#include <gtest/gtest.h>

#include <unordered_map>
#include <climits>

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::catalog;
using namespace lightdb::execution;
using namespace lightdb::optimization;

class CreateLayoutsTestFixture : public testing::Test {
public:
    CreateLayoutsTestFixture()
        : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

private:
    Catalog catalog;
};

struct LayoutInfo {
    LayoutInfo(std::string object, int duration, std::string size)
        : object(object), duration(duration), size(size)
    {}

    std::string object;
    int duration;
    std::string size;
};

std::unordered_map<std::string, std::vector<LayoutInfo>> VIDEO_TO_LAYOUT({
    {"MOT16-01", {LayoutInfo("object", 30, "one"), }},
    {"MOT16-02", {LayoutInfo("object", 30, "small"), }},
    {"MOT16-13", {LayoutInfo("object", 25, "small"), }},
    {"Netflix_BoxingPractice", {LayoutInfo("person", 60, "small"), }},
    {"Netflix_DrivingPOV", {LayoutInfo("car", 60, "small"), }},
    {"Netflix_FoodMarket", {LayoutInfo("person", 60, "small"), }},
    {"Netflix_FoodMarket2", {LayoutInfo("person", 60, "small"), }},
    {"Netflix_Narrator", {LayoutInfo("person", 60, "small"), }},
    {"Netflix_ToddlerFountain", {LayoutInfo("person", 60, "small"), }},
    {"birdsincage", {LayoutInfo("bird", 120, "small"), }},
    {"car-pov-2k-000-shortened", {LayoutInfo("car", 30, "small"), LayoutInfo("pedestrian", 30, "one"), }},
    {"car-pov-2k-001-shortened", {LayoutInfo("car", 30, "small"), LayoutInfo("pedestrian", 30, "one"), }},
    {"cosmos", {LayoutInfo("person", 24, "small"), LayoutInfo("sheep", 24, "one"), }},
    {"crowdrun", {LayoutInfo("person", -1, "one"), }},
    {"elfuente2", {LayoutInfo("person", 30, "one"), }},
    {"elfuente_full", {LayoutInfo("car_truck_boat", 60, "small"), LayoutInfo("person", 60, "small"), }},
    {"market_all", {LayoutInfo("car", 240, "small"), LayoutInfo("person", 60, "small"), LayoutInfo("truck", 240, "small"), }},
    {"meridian", {LayoutInfo("car_truck", 60, "small"), LayoutInfo("person", 60, "small"), }},
    {"narrator", {LayoutInfo("car", 240, "small"), LayoutInfo("person", 60, "small"), }},
    {"oldtown", {LayoutInfo("car", 25, "one"), }},
    {"park_joy_2k", {LayoutInfo("person", 100, "one"), }},
    {"park_joy_4k", {LayoutInfo("person", 50, "one"), }},
    {"red_kayak", {LayoutInfo("surfboard_boat", 30, "one"), }},
    {"river_boat", {LayoutInfo("boat", 60, "small"), LayoutInfo("person", 60, "small"), }},
    {"seeking", {LayoutInfo("person", 75, "one"), }},
    {"square_with_fountain", {LayoutInfo("bicycle", 60, "small"), LayoutInfo("person", 60, "one"), }},
    {"street_night_view", {LayoutInfo("car", 60, "small"), LayoutInfo("person", 60, "small"), }},
    {"tennis", {LayoutInfo("person", 24, "one"), LayoutInfo("sports_ball", 24, "one"), LayoutInfo("tennis_racket", 24, "small"), }},
    {"traffic-2k-001", {LayoutInfo("car", 30, "one"), LayoutInfo("pedestrian", 30, "one"), }},
    {"traffic-4k-000", {LayoutInfo("car", 30, "small"), LayoutInfo("pedestrian", 30, "small"), }},
    {"traffic-4k-002", {LayoutInfo("car", 30, "small"), LayoutInfo("pedestrian", 30, "small"), }},
    {"traffic-4k-002-ds2k", {LayoutInfo("car", 30, "small"), LayoutInfo("pedestrian", 30, "one"), }},
});

TEST_F(CreateLayoutsTestFixture, testWriteTilesWithoutEncoding) {
    for (auto it = VIDEO_TO_LAYOUT.begin(); it != VIDEO_TO_LAYOUT.end(); ++it) {
        std::string video = it->first;
        for (const auto &layoutInfo : it->second) {
            auto crackingStrategy =
                    layoutInfo.size == "small" ? CrackingStrategy::SmallTiles : CrackingStrategy::GroupingExtent;
            std::string crackingStr = layoutInfo.size == "small" ? "-smalltiles-" : "-grouping-extent-";
            int duration = layoutInfo.duration == -1 ? INT_MAX : layoutInfo.duration;
            std::string durationStr =
                    layoutInfo.duration == -1 ? "entire-video" : "duration" + std::to_string(layoutInfo.duration) +
                                                                 "-no-vid";
            std::string savedName = video + "-cracked-" + layoutInfo.object + crackingStr + durationStr;
            std::cout << "Cracking " << video << " to " << savedName << std::endl;

            MetadataSpecification selection("labels",
                                            std::make_shared<SingleMetadataElement>("label", layoutInfo.object));
            auto input = Scan(video);
            auto cracker = physical::CrackVideoWithoutEncoding(input, video, selection, duration, crackingStrategy,
                                                               savedName);
            cracker.saveTileLayouts();
        }
    }
}

