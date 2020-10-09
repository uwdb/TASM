#include "AssertVideo.h"
#include "DropFrames.h"
#include "HeuristicOptimizer.h"
#include "Greyscale.h"
#include "Display.h"
#include "Metadata.h"
#include "SelectPixels.h"
#include "TestResources.h"
#include "extension.h"
#include <gtest/gtest.h>

#include "timer.h"
#include <climits>
#include <iostream>
#include <random>
#include <regex>

#include "WorkloadCostEstimator.h"

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class BlazeItTestFixture : public testing::Test {
public:
    BlazeItTestFixture()
            : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

protected:
    Catalog catalog;
};

std::string SmallTilesName(const std::string &video, const std::string &label, int duration) {
    return video + "-cracked-" + label + "-smalltiles-duration" + std::to_string(duration);
}

std::string LargeTilesName(const std::string &video, const std::string &label, int duration) {
    return video + "-cracked-" + label + "-grouping-extent-duration" + std::to_string(duration);
}

std::string ROITilesName(const std::string &video) {
    return video + "-cracked-ROI";
}

// Ingest video.
TEST_F(BlazeItTestFixture, testScanAndSave) {
    auto input = Load("/home/maureen/blazeit_stuff/data/svideo/venice-grand-canal/merged_2018-01-17_short/merged_2018-01-17-short.mp4",
                      Volume::limits(),
                      GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Store("venice-grand-canal-2018-01-17"));
}

// Test decoding untiled video.
TEST_F(BlazeItTestFixture, testScanAndSink) {
    auto input = Scan("venice-grand-canal-2018-01-17");
    Coordinator().execute(input.Sink());
}

// Tile around KNN-background detections.
TEST_F(BlazeItTestFixture, testCrackAroundForegroundObjects) {
    std::string video("venice-grand-canal-2018-01-17");
    unsigned int baseFramerate = 60;
    std::string label("KNN");
    MetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));

    {
        // Small tiles.
        auto input = Scan(video);
        std::string savedName = SmallTilesName(video, label, baseFramerate);
        Coordinator().execute(input.StoreCracked(savedName, video, &selection, baseFramerate, CrackingStrategy::SmallTiles));
    }
    {
        // Large tiles.
        auto input = Scan(video);
        std::string savedName = LargeTilesName(video, label, baseFramerate);
        Coordinator().execute(input.StoreCracked(savedName, video, &selection, baseFramerate, CrackingStrategy::GroupingExtent));
    }
}

// Tile around ROI.
TEST_F(BlazeItTestFixture, testTileAroundROI) {
    std::string video("venice-grand-canal-2018-01-17");
    auto input = Scan(video);
    ROI roi(0, 490, 1300, 935);
    Coordinator().execute(input.StoreCrackedROI(ROITilesName(video), roi));
}

// Measure decode time for un-tiled, small tiles, large tiles.
TEST_F(BlazeItTestFixture, testDecodeTilesWithForegroundObjects) {
    std::string video("venice-grand-canal-2018-01-17");
    unsigned int baseFramerate = 60;
    std::string label("KNN");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));

    // Untiled video.
    {
        std::cout << "Reading from untiled video" << std::endl;
        auto input = Scan(video);
        input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
        Coordinator().execute(input.Select(selection));
    }

    // Small tiles.
    {
        std::cout << "Reading from small tiles" << std::endl;
        auto input = ScanMultiTiled(SmallTilesName(video, label, baseFramerate));
        Coordinator().execute(input.Select(selection));
    }

    // Large tiles.
    {
        std::cout << "Reading from large tiles" << std::endl;
        auto input = ScanMultiTiled(LargeTilesName(video, label, baseFramerate));
        Coordinator().execute(input.Select(selection));
    }
}

// Measure decode time for un-tiled, small tiles, large tiles.
TEST_F(BlazeItTestFixture, testDecodeTilesWithROI) {
    std::string video("venice-grand-canal-2018-01-17");
    std::string label("ROI");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));

    // Untiled video.
    {
        std::cout << "Reading from untiled video" << std::endl;
        auto input = Scan(video);
        input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
        Coordinator().execute(input.Select(selection));
    }

    // ROI tiles.
    {
        std::cout << "Reading from ROI tiles" << std::endl;
        auto input = ScanMultiTiled(ROITilesName(video));
        Coordinator().execute(input.Select(selection));
    }
}
