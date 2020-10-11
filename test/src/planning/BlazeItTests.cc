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
    std::vector<std::string> dates{"2018-01-19", "2018-01-20"};
    for (const auto &date : dates) {
        auto input = Load("/home/maureen/blazeit_stuff/data/svideo/venice-grand-canal/merged_" + date + "/merged_" + date + "-short.mp4",
                          Volume::limits(),
                          GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
        std::string savePath = "venice-grand-canal-" + date;
        Coordinator().execute(input.Store(savePath));
    }
}

// Test decoding untiled video.
TEST_F(BlazeItTestFixture, testScanAndSink) {
    auto input = Scan("venice-grand-canal-2018-01-17");
    Coordinator().execute(input.Sink());
}

// Tile around KNN-background detections.
TEST_F(BlazeItTestFixture, testCrackAroundForegroundObjects) {
    std::vector<std::string> videos {"venice-grand-canal-2018-01-19", "venice-grand-canal-2018-01-20"};
    unsigned int baseFramerate = 60;
    std::string label("KNN");
    MetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    for (const auto &video : videos) {
        {
            // Small tiles.
            auto input = Scan(video);
            std::string savedName = SmallTilesName(video, label, baseFramerate);
            Coordinator().execute(
                    input.StoreCracked(savedName, video, &selection, baseFramerate, CrackingStrategy::SmallTiles));
        }
        {
            // Large tiles.
            auto input = Scan(video);
            std::string savedName = LargeTilesName(video, label, baseFramerate);
            Coordinator().execute(
                    input.StoreCracked(savedName, video, &selection, baseFramerate, CrackingStrategy::GroupingExtent));
        }
    }
}

// Tile around ROI.
TEST_F(BlazeItTestFixture, testTileAroundROI) {
    std::vector<std::string> videos {"venice-grand-canal-2018-01-19", "venice-grand-canal-2018-01-20"};
    ROI roi(0, 490, 1300, 935);
    for (const auto &video : videos) {
        auto input = Scan(video);
        Coordinator().execute(input.StoreCrackedROI(ROITilesName(video), roi));
    }
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

/* Decode & preprocess benchmarks */
TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay1) {
    auto input = Scan("venice-grand-canal-2018-01-17");
    Coordinator().execute(input.Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay2) {
    auto input = Scan("venice-grand-canal-2018-01-19");
    Coordinator().execute(input.Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay3) {
    auto input = Scan("venice-grand-canal-2018-01-20");
    Coordinator().execute(input.Predicate());
}

/* Decode & process KNN-big tiles */
TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay1_Foreground) {
    std::string label("KNN");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = LargeTilesName("venice-grand-canal-2018-01-17", label, 60);
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay2_Foreground) {
    std::string label("KNN");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = LargeTilesName("venice-grand-canal-2018-01-19", label, 60);
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay3_Foreground) {
    std::string label("KNN");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = LargeTilesName("venice-grand-canal-2018-01-20", label, 60);
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

/* Decode & process KNN-small tiles */
TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay1_Foreground_Small) {
    std::string label("KNN");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = SmallTilesName("venice-grand-canal-2018-01-17", label, 60);
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay2_Foreground_Small) {
    std::string label("KNN");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = SmallTilesName("venice-grand-canal-2018-01-19", label, 60);
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay3_Foreground_Small) {
    std::string label("KNN");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = SmallTilesName("venice-grand-canal-2018-01-20", label, 60);
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

/* Decode & process ROI tiles */
TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay1_ROI) {
    std::string label("ROI");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = ROITilesName("venice-grand-canal-2018-01-17");
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay2_ROI) {
    std::string label("ROI");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = ROITilesName("venice-grand-canal-2018-01-19");
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}

TEST_F(BlazeItTestFixture, testDecodeAndPreprocessDay3_ROI) {
    std::string label("ROI");
    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", label));
    auto videoName = ROITilesName("venice-grand-canal-2018-01-20");
    std::cout << "Reading from " << videoName << std::endl;
    auto input = ScanMultiTiled(videoName);
    Coordinator().execute(input.Select(selection).Predicate());
}
