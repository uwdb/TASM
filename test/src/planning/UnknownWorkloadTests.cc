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

static std::unordered_map<std::string, std::pair<unsigned int, unsigned int>> videoToDimensions{
        {"traffic-2k-001", {1920, 1080}},
        {"car-pov-2k-001-shortened", {1920, 1080}},
        {"traffic-4k-000", {3840, 1980}},
        {"traffic-4k-002", {3840, 1980}}
};

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

class WorkloadGenerator {
public:
    virtual std::shared_ptr<PixelMetadataSpecification> getNextQuery() = 0;
    virtual ~WorkloadGenerator() {};
};

class VRWorkload1Generator : public WorkloadGenerator {
public:
    VRWorkload1Generator(const std::string &video, const std::string &object, unsigned int duration, std::default_random_engine *generator)
        : video_(video), object_(object), numberOfFramesInDuration_(duration * 30),
        generator_(generator), startingFrameDistribution_(0, videoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1)
    { }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery() override {
        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;
            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    std::make_shared<SingleMetadataElement>("label", object_, firstFrame, lastFrame));
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();
        }
        return selection;
    }

private:
    std::string video_;
    std::string object_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
    std::uniform_int_distribution<int> startingFrameDistribution_;
};

static void DeleteTiles(const std::string &catalogEntryName) {
    static std::string basePath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/";
    auto catalogPath = basePath + catalogEntryName;
    for (auto &dir : std::filesystem::directory_iterator(catalogPath)) {
        if (!dir.is_directory())
            continue;

        auto dirName = dir.path().filename().string();
        auto lastSep = dirName.rfind("-");
        auto tileVersion = std::stoul(dirName.substr(lastSep + 1));
        if (!tileVersion)
            continue;
        else
            std::filesystem::remove_all(dir.path());
    }
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

static void RemoveTiledVideo(const std::string &tiledName) {
    static std::string basePath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/";
    std::filesystem::remove_all(basePath + tiledName);
}

static std::string tileAroundAllObjects(const std::string &video, unsigned int duration = 30) {
    auto metadataElement = std::make_shared<AllMetadataElement>();
    MetadataSpecification metadataSpecification("labels", metadataElement);
    std::string savedName = video + "-cracked-all_objects-smalltiles-duration-" + std::to_string(duration);
    RemoveTiledVideo(savedName);

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

TEST_F(UnknownWorkloadTestFixture, testRetile4k) {
    Coordinator().execute(Scan("traffic-4k-002").PrepareForCracking("traffic-4k-002-cracked"));
}

TEST_F(UnknownWorkloadTestFixture, testPrepareForRetiling) {
    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    for (const auto &video : videos) {
        Coordinator().execute(Scan(video).PrepareForCracking(video + "-cracked"));
    }
}

//static bool UsesOneLayout(const std::string &video, std::shared_ptr<PixelMetadataSpecification> selection, unsigned int framerate) {
//    // This assumes that the layout exists, which isn't true when progressively cracking.
//    auto &dimensions = videoToDimensions.at(video);
//    auto metadataManager = std::make_shared<metadata::MetadataManager>(video, *selection);
//    auto tileConfig = std::make_shared<tiles::GroupingTileConfigurationProvider>(
//            framerate,
//            metadataManager,
//            dimensions.first,
//            dimensions.second);
//
//    Workload workload(video, {*selection}, {1});
//    WorkloadCostEstimator costEstimator(tileConfig, workload, framerate, 0, 0, 0);
//    return !costEstimator.queryRequiresMultipleLayouts(0);
//}

TEST_F(UnknownWorkloadTestFixture, testFailureCase) {
    std::string catalogName("car-pov-2k-001-shortened-cracked");
    std::string object("car");

    MetadataSpecification entireVideoSpecification("labels", std::make_shared<SingleMetadataElement>("label", object));
    {
        auto retileOp = ScanAndRetile(
                catalogName,
                entireVideoSpecification,
                30,
                CrackingStrategy::SmallTiles);
        Coordinator().execute(retileOp);
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkload1TileAroundQuery) {
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
        std::string catalogName = video + "-cracked";
        for (auto duration : videoToQueryDurations.at(video)) {
            // Delete tiles from previous runs.
            DeleteTiles(catalogName);

            VRWorkload1Generator queryGenerator(video, object, duration, &generator);
            for (auto i = 0u; i < numQueries; ++i) {
                auto selection = queryGenerator.getNextQuery();

                // Step 1: Run query.
                std::cout << "Video " << video << std::endl;
                std::cout << "Query-object " << object << std::endl;
                std::cout << "Uses-only-one-tile 0" << std::endl;
                std::cout << "Selection-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;
                {
                    auto input = ScanMultiTiled(catalogName, false);
                    Coordinator().execute(input.Select(*selection));
                }

                // Step 2: Crack around objects in query.
                std::cout << "Video " << video << std::endl;
                std::cout << "Cracking-around-object " << object << std::endl;
                std::cout << "Cracking-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;

                {
                    auto retileOp = ScanAndRetile(
                            catalogName,
                            *selection,
                            framerate,
                            CrackingStrategy::SmallTiles);
                    Coordinator().execute(retileOp);
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkload1TileAroundQueryObjectInEntireVideo) {
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
        std::string catalogName = video + "-cracked";
        for (auto duration : videoToQueryDurations.at(video)) {
            // Delete tiles from previous runs.
            DeleteTiles(catalogName);

            VRWorkload1Generator queryGenerator(video, object, duration, &generator);
            for (auto i = 0u; i < numQueries; ++i) {
                auto selection = queryGenerator.getNextQuery();

                // Step 1: Run query.
                std::cout << "Video " << video << std::endl;
                std::cout << "Query-object " << object << std::endl;
                std::cout << "Uses-only-one-tile 0" << std::endl;
                std::cout << "Selection-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;
                {
                    auto input = ScanMultiTiled(catalogName, false);
                    Coordinator().execute(input.Select(*selection));
                }

                // Step 2: Crack around objects in query in entire video.
                MetadataSpecification entireVideoSpecification("labels", std::make_shared<SingleMetadataElement>("label", object));

                std::cout << "Video " << video << std::endl;
                std::cout << "Cracking-around-object " << object << std::endl;
                std::cout << "Cracking-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;

                {
                    auto retileOp = ScanAndRetile(
                            catalogName,
                            entireVideoSpecification,
                            framerate,
                            CrackingStrategy::SmallTiles);
                    Coordinator().execute(retileOp);
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkload1TileAroundQueryIfLayoutIsVeryDifferent) {
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
        std::string catalogName = video + "-cracked";
        for (auto duration : videoToQueryDurations.at(video)) {
            // Delete tiles from previous runs.
            DeleteTiles(catalogName);

            VRWorkload1Generator queryGenerator(video, object, duration, &generator);
            for (auto i = 0u; i < numQueries; ++i) {
                auto selection = queryGenerator.getNextQuery();

                // Step 1: Run query.
                std::cout << "Video " << video << std::endl;
                std::cout << "Query-object " << object << std::endl;
                std::cout << "Uses-only-one-tile 0" << std::endl;
                std::cout << "Selection-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;
                {
                    auto input = ScanMultiTiled(catalogName, false);
                    Coordinator().execute(input.Select(*selection));
                }

                // Step 2: Crack around objects in query.
                std::cout << "Video " << video << std::endl;
                std::cout << "Cracking-around-object " << object << std::endl;
                std::cout << "Cracking-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;

                {
                    bool shouldRetileOnlyIfLayoutIsVeryDifferent = true;
                    auto retileOp = ScanAndRetile(
                            catalogName,
                            *selection,
                            framerate,
                            CrackingStrategy::SmallTiles,
                            shouldRetileOnlyIfLayoutIsVeryDifferent);
                    Coordinator().execute(retileOp);
                }
            }
        }
    }
}



