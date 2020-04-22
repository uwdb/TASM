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

#include "Distribution.h"
#include "WorkloadCostEstimator.h"

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

static unsigned int NUM_QUERIES = 20;

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

static std::unordered_map<std::string, unsigned int> videoToMaxKNNTile {
        {"traffic-2k-001", 850},
        {"car-pov-2k-001-shortened", 283},
        {"traffic-4k-000", 683},
        {"traffic-4k-002", 883},
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

class FrameGenerator {
public:
    virtual int nextFrame(std::default_random_engine &generator) = 0;
};

class UniformFrameGenerator : public FrameGenerator {
public:
    UniformFrameGenerator(const std::string &video, unsigned int duration)
        : startingFrameDistribution_(0, videoToNumFrames.at(video) - (duration * 30) - 1)
    {}

    int nextFrame(std::default_random_engine &generator) override {
        return startingFrameDistribution_(generator);
    }

private:
    std::uniform_int_distribution<int> startingFrameDistribution_;
};

class ZipfFrameGenerator : public FrameGenerator {
public:
    ZipfFrameGenerator(const std::string &video, unsigned int duration) {
        auto maxFrame = videoToNumFrames.at(video) - (duration * 30) - 1;
        MetadataSpecification selection("labels",
                std::make_shared<AllMetadataElement>("label", 0, maxFrame));
        metadata::MetadataManager metadataManager(video, selection);
        orderedFrames_ = metadataManager.framesForMetadataOrderedByNumObjects();
        distribution_ = std::make_unique<ZipfDistribution>(orderedFrames_.size(), 1.0);
    }

    int nextFrame(std::default_random_engine &generator) override {
        return orderedFrames_[(*distribution_)(generator)];
    }

private:
    std::vector<int> orderedFrames_;
    std::unique_ptr<ZipfDistribution> distribution_;
};

class VRWorkload1Generator : public WorkloadGenerator {
public:
    VRWorkload1Generator(const std::string &video, const std::string &object, unsigned int duration, std::default_random_engine *generator,
            std::unique_ptr<FrameGenerator> startingFrameDistribution)
            : video_(video), object_(object), numberOfFramesInDuration_(duration * 30),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution))  //startingFrameDistribution_(0, videoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1)
    { }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject = nullptr) override {
        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_->nextFrame(*generator_);
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
//    std::uniform_int_distribution<int> startingFrameDistribution_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;
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

class VRWorkload4Generator : public WorkloadGenerator {
    // objects[0] is the primary query object.
    // objects[1] is the alternate query object.
public:
    VRWorkload4Generator(const std::string &video, const std::vector<std::string> &objects, unsigned int numberOfQueries, unsigned int duration, std::default_random_engine *generator)
            : video_(video), objects_(objects), totalNumberOfQueries_(numberOfQueries), numberOfFramesInDuration_(duration * 30),
              generator_(generator), startingFrameDistribution_(0, videoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1)
    {
        assert(objects_.size() == 2);

        alternateQueryObjectNumber_ = totalNumberOfQueries_ / 2;
    }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject) override {
        std::string object = queryNum == alternateQueryObjectNumber_ ? objects_[1] : objects_[0];
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
    unsigned int alternateQueryObjectNumber_;
};

enum class Distribution {
    Uniform,
    Zipf,
};
static std::unique_ptr<WorkloadGenerator> GetGenerator(unsigned int workloadNum, const std::string &video, unsigned int duration,
        std::default_random_engine *generator, Distribution distribution = Distribution::Uniform) {
    std::string object("car");
    std::vector<std::string> objects{"car", "pedestrian"};

    std::unique_ptr<FrameGenerator> frameGenerator;
    switch (distribution) {
        case Distribution::Uniform:
            frameGenerator.reset(new UniformFrameGenerator(video, duration));
            break;
        case Distribution::Zipf:
            frameGenerator.reset(new ZipfFrameGenerator(video, duration));
            break;
        default:
            assert(false);
    };

    if (workloadNum == 1) {
        return std::make_unique<VRWorkload1Generator>(video, object, duration, generator, std::move(frameGenerator));
    } else if (workloadNum == 2) {
        return std::make_unique<VRWorkload2Generator>(video, objects, NUM_QUERIES, duration, generator);
    } else if (workloadNum == 3) {
        return std::make_unique<VRWorkload3Generator>(video, objects, duration, generator);
    } else if (workloadNum == 4) {
        return std::make_unique<VRWorkload4Generator>(video, objects, NUM_QUERIES, duration, generator);
    } else {
        assert(false);
    }
}

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

static void DeleteTilesPastNum(const std::string &catalogEntryName, unsigned int maxOriginalTileNum) {
    static std::string basePath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/";
    auto catalogPath = basePath + catalogEntryName;

    for (auto &dir : std::filesystem::directory_iterator(catalogPath)) {
        if (!dir.is_directory())
            continue;

        auto dirName = dir.path().filename().string();
        auto lastSep = dirName.rfind("-");
        auto tileVersion = std::stoul(dirName.substr(lastSep + 1));
        if (tileVersion > maxOriginalTileNum)
            std::filesystem::remove_all(dir.path());
    }
}

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
            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);

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
            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);

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
//            "car-pov-2k-001-shortened",
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

            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
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

            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
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

            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
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
                    auto retileStrategy = logical::RetileStrategy::RetileIfDifferent;
                    auto retileOp = ScanAndRetile(
                            catalogName,
                            *selection,
                            framerate,
                            CrackingStrategy::SmallTiles,
                            retileStrategy);
                    Coordinator().execute(retileOp);
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryIfLayoutIsVeryDifferentStartWithKNNTiles) {
    std::cout << "\nWorkload-strategy tile-query-duration-if-different-init-KNN-tiles" << std::endl;

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
        std::string catalogName = video + "-cracked-KNN-smalltiles-duration30";
        for (auto duration : videoToQueryDurations.at(video)) {
            std::default_random_engine generator(videoToProbabilitySeed.at(video));
            // Delete tiles from previous runs.
            DeleteTilesPastNum(catalogName, videoToMaxKNNTile.at(video));

//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
//            VRWorkload3Generator queryGenerator(video, {"car", "pedestrian"}, duration, &generator);
            VRWorkload2Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
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
                    auto retileStrategy = logical::RetileStrategy::RetileIfDifferent;
                    auto retileOp = ScanAndRetile(
                            catalogName,
                            *selection,
                            framerate,
                            CrackingStrategy::SmallTiles,
                            retileStrategy);
                    Coordinator().execute(retileOp);
                }
            }
        }
    }
}

class TestRegretAccumulator : public RegretAccumulator {
public:
    TestRegretAccumulator(const std::string &video, unsigned int width, unsigned int height, unsigned int gopLength, const std::vector<std::string> &labels) {
        gopSizeInPixels_ = width * height * gopLength;
        labels_ = labels;
        for (const auto & label : labels) {
            auto metadataManager = std::make_shared<metadata::MetadataManager>(video,
                                                                               MetadataSpecification("labels", std::make_shared<SingleMetadataElement>("label", label)));
            idToConfig_[label] = std::make_shared<tiles::GroupingTileConfigurationProvider>(
                    gopLength,
                    metadataManager,
                    width,
                    height);
        }
    }

    void addRegretToGOP(unsigned int gop, long long int regret, const std::string &layoutIdentifier) override {
        if (!gopToRegret_[gop][layoutIdentifier])
            gopToRegret_[gop][layoutIdentifier] = 0;

        gopToRegret_[gop][layoutIdentifier] += regret;
    }

    bool shouldRetileGOP(unsigned int gop, std::string &layoutIdentifier) override {
        long long int maxRegret = 0;
        std::string labelWithMaxRegret;

        for (auto it = gopToRegret_[gop].begin(); it != gopToRegret_[gop].end(); ++it) {
            if (it->second > maxRegret) {
                maxRegret = it->second;
                labelWithMaxRegret = it->first;
            }
        }
        if (maxRegret >= gopSizeInPixels_ * 0.65) {
            layoutIdentifier = labelWithMaxRegret;
            return true;
        } else {
            return false;
        }
    }

    void resetRegretForGOP(unsigned int gop) override {
        for (auto it = gopToRegret_[gop].begin(); it != gopToRegret_[gop].end(); ++it)
            it->second = 0;
    }

    std::shared_ptr<tiles::TileConfigurationProvider> configurationProviderForIdentifier(const std::string &identifier)  override {
        return idToConfig_.at(identifier);
    }

    const std::vector<std::string> layoutIdentifiers() override {
        return labels_;
    }

private:
    std::vector<std::string> labels_;
    std::unordered_map<std::string, std::shared_ptr<tiles::TileConfigurationProvider>> idToConfig_;
    long long int gopSizeInPixels_;
    std::unordered_map<unsigned int, std::unordered_map<std::string, long long int>> gopToRegret_;
};

TEST_F(UnknownWorkloadTestFixture, testRegretAccumulator) {
    std::string video("traffic-2k-001");
    std::vector<std::string> objects{"car", "pedestrian"};
    unsigned int width = 1920;
    unsigned int height = 1080;
    unsigned int gopLength = 30;

    TestRegretAccumulator acc(video, width, height, gopLength, objects);

    std::string retileObj;

    assert(!acc.shouldRetileGOP(0, retileObj));
    assert(!acc.shouldRetileGOP(10, retileObj));

    // GOP pixels: 62,208,000
    // half that, 31104000
    long long int halfGOP = 31104000;

    acc.addRegretToGOP(0, halfGOP - 1, "car");
    assert(!acc.shouldRetileGOP(0, retileObj));

    acc.addRegretToGOP(0, halfGOP - 1, "pedestrian");
    assert(!acc.shouldRetileGOP(0, retileObj));

    acc.addRegretToGOP(0, 10, "car");
    assert(acc.shouldRetileGOP(0, retileObj));
    assert(retileObj == "car");
    assert(!acc.shouldRetileGOP(10, retileObj));
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryAfterAccumulatingRegret) {
    std::cout << "\nWorkload-strategy tile-query-duration-if-regret-accumulates" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    unsigned int framerate = 30;

//    std::default_random_engine generator(7);
    for (const auto &video : videos) {
        auto videoDimensions = videoToDimensions.at(video);
        std::string catalogName = video + "-cracked";
        for (auto duration : videoToQueryDurations.at(video)) {

            auto regretAccumulator = std::make_shared<TestRegretAccumulator>(
                    video,
                    videoDimensions.first, videoDimensions.second, framerate,
                    std::vector<std::string>{"car", "pedestrian"});

            std::default_random_engine generator(videoToProbabilitySeed.at(video));

            // Delete tiles from previous runs.
            DeleteTiles(catalogName);

            auto queryGenerator = GetGenerator(1, video, duration, &generator);
            for (auto i = 0u; i < NUM_QUERIES; ++i) {
                std::string object;
                auto selection = queryGenerator->getNextQuery(i, &object);

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
                            CrackingStrategy::SmallTiles,
                            RetileStrategy::RetileBasedOnRegret,
                            regretAccumulator);
                    Coordinator().execute(retileOp);
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testZipfDistribution) {
    std::default_random_engine generator(5);

    ZipfDistribution distribution(1000, 1.0);
    for (int i = 0; i < 2000; ++i) {
        auto r = distribution(generator);
        std::cout << r << std::endl;
    }
}

TEST_F(UnknownWorkloadTestFixture, testZipfDistribution2) {
    std::default_random_engine generator(5);
    UniformFrameGenerator frameGen("traffic-2k-001", 300);

    for (int i = 0; i < 2000; ++i) {
        auto r = frameGen.nextFrame(generator);
        std::cout << r << std::endl;
    }
}
