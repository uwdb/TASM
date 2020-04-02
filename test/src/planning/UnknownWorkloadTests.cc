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

class UnknownWorkloadTestFixture : public testing::Test {
public:
    UnknownWorkloadTestFixture()
            : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

protected:
    Catalog catalog;
};

std::unordered_map<std::string, std::vector<unsigned int>> videoToQueryDurations{
        {"traffic-2k-001", {60, 300}},
        {"car-pov-2k-001-shortened", {60, 190}},
        {"traffic-4k-000", {60, 300}},
        {"traffic-4k-002", {60, 300}},
};

std::unordered_map<std::string, unsigned int> videoToNumFrames{
        {"traffic-2k-001", 27001},
        {"car-pov-2k-001-shortened", 17102},
        {"traffic-4k-000", 27001},
        {"traffic-4k-002", 27001},
};

TEST_F(UnknownWorkloadTestFixture, testDelete) {
    std::string path("/home/maureen/test_dir");
    std::filesystem::remove_all(path);
}

TEST_F(UnknownWorkloadTestFixture, testFailure) {
    std::string video("traffic-4k-000");
    auto firstFrame = 1;
    auto lastFrame = 60;
    std::string object("car");

    PixelMetadataSpecification selection("labels",
            std::make_shared<SingleMetadataElement>("label", object, firstFrame, lastFrame));
    auto metadataManager = std::make_shared<metadata::MetadataManager>(video, selection);
    std::cout << "Num-frames: " << metadataManager->framesForMetadata().size() << std::endl;

//    auto input = Scan(video);
//    input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
//    Coordinator().execute(input.Select(selection));
}

// Workload 1: Select a single object
// TileStrategy: No tiles.
TEST_F(UnknownWorkloadTestFixture, testWorkload1NoTiles) {
    std::vector<std::string> videos{
        "traffic-2k-001",
        "car-pov-2k-001-shortened",
        "traffic-4k-000",
        "traffic-4k-002",
    };

    unsigned int numQueries = 20;
    unsigned int framerate = 30;
    std::string object("car");

    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        for (auto duration : videoToQueryDurations.at(video)) {
            unsigned int numberOfFramesInDuration = duration * framerate;

            // Create a generator for where to start queries.
            unsigned int lastPossibleFrame = videoToNumFrames.at(video) - numberOfFramesInDuration - 1;
            std::uniform_int_distribution<int> startingFrameDistribution(0, lastPossibleFrame);
            for (auto i = 0u; i < numQueries; ++i) {
                int firstFrame, lastFrame;
                std::unique_ptr<PixelMetadataSpecification> selection;
                bool hasObjects = false;
                while (!hasObjects) {
                    firstFrame = startingFrameDistribution(generator);
                    lastFrame = firstFrame + numberOfFramesInDuration;
                    selection = std::make_unique<PixelMetadataSpecification>(
                            "labels",
                         std::make_shared<SingleMetadataElement>("label", object, firstFrame, lastFrame));
                    auto metadataManager = std::make_unique<metadata::MetadataManager>(video, *selection);
                    hasObjects = metadataManager->framesForMetadata().size();
                }

                std::cout << "Video " << video << std::endl;
                std::cout << "Tile-strategy none" << std::endl;
                std::cout << "Query-object " << object << std::endl;
                std::cout << "Uses-only-one-tile 1" << std::endl;
                std::cout << "Selection-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << firstFrame << std::endl;
                std::cout << "Last-frame " << lastFrame << std::endl;
                auto input = Scan(video);
                input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
                Coordinator().execute(input.Select(*selection));
            }
        }
    }
}

static void removeTiledVideo(const std::string &tiledName) {
    static std::string basePath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/";
    std::filesystem::remove_all(basePath + tiledName);
}

static std::string tileAroundAllObjects(const std::string &video, unsigned int duration = 30) {
    auto metadataElement = std::make_shared<AllMetadataElement>();
    MetadataSpecification metadataSpecification("labels", metadataElement);
    std::string savedName = video + "-cracked-all_objects-smalltiles-duration-" + std::to_string(duration);
    removeTiledVideo(savedName);

    std::cout << "Cracking " << video << " to " << savedName << std::endl;

    std::cout << "Video " << video << std::endl;
    std::cout << "Iteration " << -1 << std::endl;
    std::cout << "Initial-crack " << "all_objects" << std::endl;

    auto input = Scan(video);
    Coordinator().execute(
            input.StoreCracked(
                    savedName, video, &metadataSpecification, duration, CrackingStrategy::SmallTiles));

    return savedName;
}

TEST_F(UnknownWorkloadTestFixture, testWorkload1TileAroundAll) {
    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    unsigned int numQueries = 20;
    unsigned int framerate = 30;
    std::string object("car");

    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        // First, tile around all of the objects in the video.
        auto catalogName = tileAroundAllObjects(video);

        for (auto duration : videoToQueryDurations.at(video)) {
            unsigned int numberOfFramesInDuration = duration * framerate;

            // Create a generator for where to start queries.
            unsigned int lastPossibleFrame = videoToNumFrames.at(video) - numberOfFramesInDuration - 1;
            std::uniform_int_distribution<int> startingFrameDistribution(0, lastPossibleFrame);
            for (auto i = 0u; i < numQueries; ++i) {
                int firstFrame, lastFrame;
                std::unique_ptr<PixelMetadataSpecification> selection;
                bool hasObjects = false;
                while (!hasObjects) {
                    firstFrame = startingFrameDistribution(generator);
                    lastFrame = firstFrame + numberOfFramesInDuration;
                    selection = std::make_unique<PixelMetadataSpecification>(
                            "labels",
                            std::make_shared<SingleMetadataElement>("label", object, firstFrame, lastFrame));
                    auto metadataManager = std::make_unique<metadata::MetadataManager>(video, *selection);
                    hasObjects = metadataManager->framesForMetadata().size();
                }

                std::cout << "Video " << video << std::endl;
                std::cout << "Cracking-object " << "all_objects" << std::endl;
                std::cout << "Tile-strategy small-dur-30" << std::endl;
                std::cout << "Query-object " << object << std::endl;
                std::cout << "Uses-only-one-tile 0" << std::endl;
                std::cout << "Selection-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << firstFrame << std::endl;
                std::cout << "Last-frame " << lastFrame << std::endl;
                auto input = ScanMultiTiled(catalogName, false);
                Coordinator().execute(input.Select(*selection));
            }
        }
    }
}

