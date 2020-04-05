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

static std::unordered_map<std::string, unsigned int> videoToProbabilitySeed {
        {"traffic-2k-001", 8},
        {"car-pov-2k-001-shortened", 12},
        {"traffic-4k-000", 15},
        {"traffic-4k-002", 22},
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
    virtual std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject) = 0;
    virtual ~WorkloadGenerator() {};
};

class VRWorkload1Generator : public WorkloadGenerator {
public:
    VRWorkload1Generator(const std::string &video, const std::string &object, unsigned int duration, std::default_random_engine *generator)
        : video_(video), object_(object), numberOfFramesInDuration_(duration * 30),
        generator_(generator), startingFrameDistribution_(0, videoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1)
    { }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject = nullptr) override {
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
        if (outQueryObject)
            *outQueryObject = object_;

        return selection;
    }

private:
    std::string video_;
    std::string object_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
    std::uniform_int_distribution<int> startingFrameDistribution_;
};

class VRWorkload2Generator : public WorkloadGenerator {
public:
    VRWorkload2Generator(const std::string &video, const std::vector<std::string> &objects, unsigned int numberOfQueries, unsigned int duration, std::default_random_engine *generator)
            : video_(video), objects_(objects), totalNumberOfQueries_(numberOfQueries), numberOfFramesInDuration_(duration * 30),
              generator_(generator), startingFrameDistribution_(0, videoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1)
    {
        assert(objects_.size() == 2);
    }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject) override {
        std::string object = queryNum < totalNumberOfQueries_ / 2 ? objects_[0] : objects_[1];
        if (outQueryObject)
            *outQueryObject = object;

        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;
            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    std::make_shared<SingleMetadataElement>("label", object, firstFrame, lastFrame));
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();
        }
        return selection;
    }

private:
    std::string video_;
    std::vector<std::string> objects_;
    unsigned int totalNumberOfQueries_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
    std::uniform_int_distribution<int> startingFrameDistribution_;
};

class VRWorkload3Generator : public WorkloadGenerator {
public:
    VRWorkload3Generator(const std::string &video, const std::vector<std::string> &objects, unsigned int duration, std::default_random_engine *generator)
            : video_(video), objects_(objects), numberOfFramesInDuration_(duration * 30),
              generator_(generator), startingFrameDistribution_(0, videoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1),
              probabilityGenerator_(videoToProbabilitySeed.at(video)), probabilityDistribution_(0.0, 1.0)
    {
        assert(objects_.size() == 2);
    }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject) override {
        std::string object = probabilityDistribution_(probabilityGenerator_) < 0.5 ? objects_[0] : objects_[1];
        if (outQueryObject)
            *outQueryObject = object;

        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;
            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    std::make_shared<SingleMetadataElement>("label", object, firstFrame, lastFrame));
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();
        }
        return selection;
    }

private:
    std::string video_;
    std::vector<std::string> objects_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
    std::uniform_int_distribution<int> startingFrameDistribution_;

    std::default_random_engine probabilityGenerator_;
    std::uniform_real_distribution<float> probabilityDistribution_;
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

static unsigned int NUM_QUERIES = 20;

// Workload 1: Select a single object
// TileStrategy: No tiles.
TEST_F(UnknownWorkloadTestFixture, testWorkloadNoTiles) {
    std::cout << "\nWorkload-strategy no-tiles" << std::endl;

    std::vector<std::string> videos{
        "traffic-2k-001",
        "car-pov-2k-001-shortened",
        "traffic-4k-000",
        "traffic-4k-002",
    };

//    std::string object("car");

//    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        for (auto duration : videoToQueryDurations.at(video)) {
            std::default_random_engine generator(videoToProbabilitySeed.at(video));
            VRWorkload3Generator queryGenerator(video, {"car", "pedestrian"}, duration, &generator);

            for (auto i = 0u; i < NUM_QUERIES; ++i) {
                std::string object;
                auto selection = queryGenerator.getNextQuery(i, &object);

                std::cout << "Video " << video << std::endl;
                std::cout << "Tile-strategy none" << std::endl;
                std::cout << "Query-object " << object << std::endl;
                std::cout << "Uses-only-one-tile 1" << std::endl;
                std::cout << "Selection-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;
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

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundAll) {
    std::cout << "\nWorkload-strategy tile-around-all-objects" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

//    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        // First, tile around all of the objects in the video.
//        auto catalogName = tileAroundAllObjects(video);
        auto catalogName = video + "-cracked-all_objects-smalltiles-duration-30";

        for (auto duration : videoToQueryDurations.at(video)) {
            std::default_random_engine generator(videoToProbabilitySeed.at(video));
            VRWorkload3Generator queryGenerator(video, {"car", "pedestrian"}, duration, &generator);

            for (auto i = 0u; i < NUM_QUERIES; ++i) {
                std::string object;
                auto selection = queryGenerator.getNextQuery(i, &object);

                std::cout << "Video " << video << std::endl;
                std::cout << "Cracking-object " << "all_objects" << std::endl;
                std::cout << "Tile-strategy small-dur-30" << std::endl;
                std::cout << "Query-object " << object << std::endl;
                std::cout << "Uses-only-one-tile 0" << std::endl;
                std::cout << "Selection-duration " << duration << std::endl;
                std::cout << "Iteration " << i << std::endl;
                std::cout << "First-frame " << selection->firstFrame() << std::endl;
                std::cout << "Last-frame " << selection->lastFrame() << std::endl;
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

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQuery) {
    std::cout << "\nWorkload-strategy tile-query-duration" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        std::string catalogName = video + "-cracked";
        for (auto duration : videoToQueryDurations.at(video)) {
            std::default_random_engine generator(videoToProbabilitySeed.at(video));

            // Delete tiles from previous runs.
            DeleteTiles(catalogName);

            VRWorkload3Generator queryGenerator(video, {"car", "pedestrian"}, duration, &generator);
            for (auto i = 0u; i < NUM_QUERIES; ++i) {
                std::string object;
                auto selection = queryGenerator.getNextQuery(i, &object);

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

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryObjectInEntireVideo) {
    std::cout << "\nWorkload-strategy tile-entire-vid-after-each-query" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        std::string catalogName = video + "-cracked";
        for (auto duration : videoToQueryDurations.at(video)) {
            std::default_random_engine generator(videoToProbabilitySeed.at(video));

            // Delete tiles from previous runs.
            DeleteTiles(catalogName);

            VRWorkload3Generator queryGenerator(video, {"car", "pedestrian"}, duration, &generator);
            for (auto i = 0u; i < NUM_QUERIES; ++i) {
                std::string object;
                auto selection = queryGenerator.getNextQuery(i, &object);

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

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryIfLayoutIsVeryDifferent) {
    std::cout << "\nWorkload-strategy tile-query-duration-if-different" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        std::string catalogName = video + "-cracked";
        for (auto duration : videoToQueryDurations.at(video)) {
            std::default_random_engine generator(videoToProbabilitySeed.at(video));
            // Delete tiles from previous runs.
            DeleteTiles(catalogName);

            VRWorkload3Generator queryGenerator(video, {"car", "pedestrian"}, duration, &generator);
            for (auto i = 0u; i < NUM_QUERIES; ++i) {
                std::string object;
                auto selection = queryGenerator.getNextQuery(i, &object);

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

TEST_F(UnknownWorkloadTestFixture, testHello1) {
    sleep(30);
    std::cout << "Hello 1" << std::endl;
}

TEST_F(UnknownWorkloadTestFixture, testHello2) {
    sleep(15);
    std::cout << "Hello 3" << std::endl;
}

TEST_F(UnknownWorkloadTestFixture, testRandomProbab) {
    std::default_random_engine probabilityGenerator(8);
    std::uniform_real_distribution<float> probabilityDistribution(0.0, 1.0);

    for (int i = 0; i < 50; ++i) {
        std::cout << probabilityDistribution(probabilityGenerator) << std::endl;
    }
}



