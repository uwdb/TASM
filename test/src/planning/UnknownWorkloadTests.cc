#include "AssertVideo.h"
#include "DropFrames.h"
#include "HeuristicOptimizer.h"
#include "Greyscale.h"
#include "Display.h"
#include "Metadata.h"
#include "SelectPixels.h"
#include "TestResources.h"
#include "TileUtilities.h"
#include "extension.h"
#include <gtest/gtest.h>

#include "timer.h"
#include <climits>
#include <iostream>
#include <fstream>
#include <limits>
#include <random>
#include <regex>

#include "Distribution.h"
#include "WorkloadCostEstimator.h"
#include "RegretAccumulator.h"
#include "VideoStats.h"

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

static unsigned int NUM_QUERIES = 200;
static unsigned int WORKLOAD_NUM = 3;

static float WINDOW_FRACTION = 0.25;

static std::unordered_map<std::string, unsigned int> videoToProbabilitySeed {
//        {"traffic-2k-001", 8},
//        {"car-pov-2k-001-shortened", 12},
//        {"traffic-4k-000", 15},
//        {"traffic-4k-002", 22},
        {"traffic-2k-001", 8},
        {"car-pov-2k-000-shortened", 11},
        {"car-pov-2k-001-shortened", 12},
        {"traffic-4k-002-ds2k", 14},
        {"traffic-4k-000", 15},
        {"traffic-4k-002", 22},
        {"narrator", 5},
        {"market_all", 4},
        {"square_with_fountain", 3},
        {"street_night_view", 2},
        {"river_boat", 1},
        {"Netflix_BoxingPractice", 3},
        {"Netflix_FoodMarket2", 4},
        {"seeking", 6},
        {"tennis", 5},
        {"birdsincage", 3},
        {"crowdrun", 7},
        {"elfuente1", 9},
        {"elfuente2", 9},
        {"oldtown", 1},
        {"red_kayak", 1},
        {"touchdown_pass", 1},
        {"park_joy_2k", 1},
        {"park_joy_4k", 1},
        {"Netflix_ToddlerFountain", 1},
        {"Netflix_Narrator", 1},
        {"Netflix_FoodMarket", 1},
        {"Netflix_DrivingPOV", 1},
};

static std::unordered_map<std::string, unsigned int> videoToMaxKNNTile {
        {"traffic-2k-001", 850},
        {"car-pov-2k-001-shortened", 283},
        {"traffic-4k-000", 683},
        {"traffic-4k-002", 883},
        {"street_night_view", 7},
        {"square_with_fountain", 5},
        {"river_boat", 4},
        {"market_all", 6},
        {"narrator", 0},
        {"birdsincage", 5},
        {"crowdrun", 2},
        {"elfuente1", 2},
        {"elfuente2", 2},
        {"oldtown", 4},
        {"tennis", 2},
        {"seeking", 0},
        {"red_kayak", 0},
        {"touchdown_pass", 3},
        {"park_joy_2k", 0},
        {"park_joy_4k", 0},
        {"Netflix_ToddlerFountain", 0},
        {"Netflix_Narrator", 2},
        {"Netflix_FoodMarket", 0},
        {"Netflix_FoodMarket2", 2},
        {"Netflix_DrivingPOV", 2},
        {"Netflix_BoxingPractice", 2},
};

static std::unordered_map<std::string, unsigned int> videoToMaxAllObjTile {
        {"traffic-2k-001", 0},
        {"car-pov-2k-001-shortened", 0},
        {"traffic-4k-000", 0},
        {"traffic-4k-002", 0},
        {"street_night_view", 19},
        {"square_with_fountain", 8},
        {"river_boat", 28},
        {"market_all", 37},
        {"narrator", 20},
        {"birdsincage", 2},
        {"crowdrun", 1},
        {"elfuente1", 0},
        {"elfuente2", 5},
        {"oldtown", 5},
        {"tennis", 3},
        {"seeking", 2},
        {"red_kayak", 16},
        {"touchdown_pass", 18},
        {"park_joy_2k", 8},
        {"park_joy_4k", 9},
        {"Netflix_ToddlerFountain", 19},
        {"Netflix_Narrator", 4},
        {"Netflix_FoodMarket", 9},
        {"Netflix_FoodMarket2", 4},
        {"Netflix_DrivingPOV", 18},
        {"Netflix_BoxingPractice", 4},
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
//        {"traffic-2k-001", {60, 300}},
//        {"car-pov-2k-001-shortened", {60, 190}},
//        {"traffic-4k-000", {60, 300}},
//        {"traffic-4k-002", {60, 300}},
//
        {"traffic-2k-001", {60}},
        {"car-pov-2k-000-shortened", {60}},
        {"car-pov-2k-001-shortened", {60}},
        {"traffic-4k-002-ds2k", {60}},
        {"traffic-4k-000", {60}},
        {"traffic-4k-002", {60}},
        {"narrator", {1}},
        {"market_all", {1}},
        {"square_with_fountain", {1}},
        {"street_night_view", {1}},
        {"river_boat", {1}},
        {"Netflix_BoxingPractice", {1}},
        {"Netflix_FoodMarket2", {1}},
        {"seeking", {1}},
        {"tennis", {1}},
        {"birdsincage", {1}},
        {"crowdrun", {1}},
        {"elfuente1", {1}},
        {"elfuente2", {1}},
        {"oldtown", {1}},
        {"red_kayak", {1}},
        {"touchdown_pass", {1}},
        {"park_joy_2k", {1}},
        {"park_joy_4k", {1}},
        {"Netflix_ToddlerFountain", {1}},
        {"Netflix_Narrator", {1}},
        {"Netflix_FoodMarket", {1}},
        {"Netflix_DrivingPOV", {1}}
};

std::unordered_map<std::string, unsigned int> videoToStartForWindow{
//        {"traffic-2k-001", 27001},
//        {"car-pov-2k-001-shortened", 17102},
//        {"traffic-4k-000", 27001},
//        {"traffic-4k-002", 27001},
//        {"traffic-2k-001", 30},
//        {"car-pov-2k-000-shortened", 30},
//        {"car-pov-2k-001-shortened", 30},
//        {"traffic-4k-002-ds2k", 30},
//        {"traffic-4k-000", 30},
//        {"traffic-4k-002", 30},
//        {"narrator", 10},
//        {"market_all", 20},
//        {"square_with_fountain", 0},
//        {"street_night_view", 40},
//        {"river_boat", 70},
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
        : startingFrameDistribution_(0, VideoToNumFrames.at(video) - (duration * VideoToFramerate.at(video)) - 1)
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
        auto maxFrame = VideoToNumFrames.at(video) - (duration * VideoToFramerate.at(video)) - 1;
//        MetadataSpecification selection("labels",
//                std::make_shared<AllMetadataElement>("label", 0, maxFrame));
//        metadata::MetadataManager metadataManager(video, selection);
//        orderedFrames_ = metadataManager.framesForMetadataOrderedByNumObjects();
//        distribution_ = std::make_unique<ZipfDistribution>(orderedFrames_.size(), 1.0);
        distribution_ = std::make_unique<ZipfDistribution>(maxFrame, 1.0);
    }

    int nextFrame(std::default_random_engine &generator) override {
//        return orderedFrames_[(*distribution_)(generator)];
        return (*distribution_)(generator);
    }

private:
    std::vector<int> orderedFrames_;
    std::unique_ptr<ZipfDistribution> distribution_;
};

class WindowUniformFrameGenerator : public FrameGenerator {
public:
    WindowUniformFrameGenerator(const std::string &video, unsigned int duration, double windowFraction)
    {
//        int firstFrame = VideoToNumFrames.at(video) * videoToStartForWindow.at(video) / 100.0;
        int numFrames = VideoToNumFrames.at(video) * windowFraction;
        int lastFrame = numFrames - (duration * VideoToFramerate.at(video)) - 1;

        startingFrameDistribution_ = std::make_unique<std::uniform_int_distribution<int>>(0, lastFrame);
    }

    int nextFrame(std::default_random_engine &generator) override {
        return (*startingFrameDistribution_)(generator);
    }

private:
    std::unique_ptr<std::uniform_int_distribution<int>> startingFrameDistribution_;
};

static std::string combineStrings(const std::vector<std::string> &strings, std::string connector = "_") {
    std::string combined = "";
    auto lastIndex = strings.size() - 1;
    for (auto i = 0u; i < strings.size(); ++i) {
        combined += strings[i];
        if (i < lastIndex)
            combined += connector;
    }
    return combined;
}

static std::shared_ptr<MetadataElement> MetadataElementForObjects(const std::vector<std::string> &objects, unsigned int firstFrame = 0, unsigned int lastFrame = UINT32_MAX) {
    std::vector<std::shared_ptr<MetadataElement>> componentElements(objects.size());
    std::transform(objects.begin(), objects.end(), componentElements.begin(),
                   [&](auto obj) {
                       return std::make_shared<SingleMetadataElement>("label", obj, firstFrame, lastFrame);
                   });
    return componentElements.size() == 1 ? componentElements.front() : std::make_shared<OrMetadataElement>(componentElements);
}

class VRWorkload1Generator : public WorkloadGenerator {
public:
    VRWorkload1Generator(const std::string &video, const std::vector<std::string> &objects, unsigned int duration, std::default_random_engine *generator,
            std::unique_ptr<FrameGenerator> startingFrameDistribution)
            : video_(video), objects_(objects), numberOfFramesInDuration_(duration * VideoToFramerate.at(video)),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution))  //startingFrameDistribution_(0, VideoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1)
    { }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject = nullptr) override {
        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_->nextFrame(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;

            auto metadataElement = MetadataElementForObjects(objects_, firstFrame, lastFrame);

            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    metadataElement);
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();
        }
        if (outQueryObject)
            *outQueryObject = combineStrings(objects_);

        return selection;
    }

private:
    std::string video_;
    std::vector<std::string> objects_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
//    std::uniform_int_distribution<int> startingFrameDistribution_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;
};

class VRWorkload2Generator : public WorkloadGenerator {
public:
    VRWorkload2Generator(const std::string &video, const std::vector<std::vector<std::string>> &objects, unsigned int numberOfQueries, unsigned int duration,
            std::default_random_engine *generator, std::unique_ptr<FrameGenerator> startingFrameDistribution)
            : video_(video), objects_(objects), totalNumberOfQueries_(numberOfQueries), numberOfFramesInDuration_(duration * VideoToFramerate.at(video)),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution))
    {
        assert(objects_.size() == 2);
    }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject) override {
        unsigned int index;
        if (queryNum < totalNumberOfQueries_ / 3)
            index = 0;
        else if (queryNum < 2 * totalNumberOfQueries_ / 3)
            index = 1;
        else
            index = 0;
        std::vector<std::string> objects = objects_[index];
        if (outQueryObject)
            *outQueryObject = combineStrings(objects);

        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_->nextFrame(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;
            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    MetadataElementForObjects(objects, firstFrame, lastFrame));
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();
        }
        return selection;
    }

private:
    std::string video_;
    std::vector<std::vector<std::string>> objects_;
    unsigned int totalNumberOfQueries_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
//    std::uniform_int_distribution<int> startingFrameDistribution_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;
};

class VRWorkload3Generator : public WorkloadGenerator {
public:
    VRWorkload3Generator(const std::string &video, const std::vector<std::vector<std::string>> &objects, unsigned int duration, std::default_random_engine *generator,
                         std::unique_ptr<FrameGenerator> startingFrameDistribution)
            : video_(video), objects_(objects), numberOfFramesInDuration_(duration * VideoToFramerate.at(video)),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution)),
              probabilityGenerator_(42), probabilityDistribution_(0.0, 1.0)
    {
        assert(objects_.size() > 0 && objects_.size() <= 3);
    }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject) override {
        auto prob = probabilityDistribution_(probabilityGenerator_);
        unsigned int index;

        if (objects_.size() == 1)
            index = 0;
        else if (objects_.size() == 2)
            index = prob < 0.5 ? 0 : 1;
        else if (objects_.size() == 3) {
            if (prob <= 0.33)
                index = 0;
            else if (prob >= 0.67)
                index = 2;
            else
                index = 1;
        } else {
            assert(false);
        }

        const std::vector<std::string> &objects = objects_[index];

        if (outQueryObject)
            *outQueryObject = combineStrings(objects);

        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_->nextFrame(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;

            auto metadataElement = MetadataElementForObjects(objects, firstFrame, lastFrame);

            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    metadataElement);
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();
        }
        return selection;
    }

private:
    std::string video_;
    std::vector<std::vector<std::string>> objects_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
//    std::uniform_int_distribution<int> startingFrameDistribution_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;

    std::default_random_engine probabilityGenerator_;
    std::uniform_real_distribution<float> probabilityDistribution_;
};

class VRWorkload5Generator : public WorkloadGenerator {
public:
    VRWorkload5Generator(const std::string &video, const std::vector<std::vector<std::string>> &objects, unsigned int duration, std::default_random_engine *generator,
                         std::unique_ptr<FrameGenerator> startingFrameDistribution)
            : video_(video), objects_(objects), numberOfFramesInDuration_(duration * VideoToFramerate.at(video)),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution)),
              probabilityGenerator_(42), probabilityDistribution_(0.0, 1.0)
    {
        assert(objects_.size() == 3);
    }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject) override {
        auto probability = probabilityDistribution_(probabilityGenerator_);
        unsigned int index;
        if (probability < 0.475)
            index = 0;
        else if (probability >= 0.475 && probability < 0.95)
            index = 1;
        else
            index = 2;

        const std::vector<std::string> &objects = objects_[index];
        if (outQueryObject)
            *outQueryObject = combineStrings(objects);

        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_->nextFrame(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;

            auto metadataElement = MetadataElementForObjects(objects, firstFrame, lastFrame);

            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    metadataElement);
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();
        }
        return selection;
    }

private:
    std::string video_;
    std::vector<std::vector<std::string>> objects_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
//    std::uniform_int_distribution<int> startingFrameDistribution_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;

    std::default_random_engine probabilityGenerator_;
    std::uniform_real_distribution<float> probabilityDistribution_;
};

class VRWorkload6Generator : public WorkloadGenerator {
public:
    VRWorkload6Generator(const std::string &video, const std::vector<std::vector<std::string>> &objects, unsigned int duration, std::default_random_engine *generator,
                         std::unique_ptr<FrameGenerator> startingFrameDistribution)
            : video_(video), objects_(objects), numberOfFramesInDuration_(duration * VideoToFramerate.at(video)),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution))  //startingFrameDistribution_(0, VideoToNumFrames.at(video_) - numberOfFramesInDuration_ - 1)
    {
        assert(objects_.size() == 2);
    }

    std::shared_ptr<PixelMetadataSpecification> getNextQuery(unsigned int queryNum, std::string *outQueryObject = nullptr) override {
        int firstFrame, lastFrame;
        std::shared_ptr<PixelMetadataSpecification> selection;
        bool hasObjects = false;
        while (!hasObjects) {
            firstFrame = startingFrameDistribution_->nextFrame(*generator_);
            lastFrame = firstFrame + numberOfFramesInDuration_;

            // A little less than 50% because queries in the middle are favored towards object 0.
            auto objects = firstFrame < 0.44 * WINDOW_FRACTION * VideoToNumFrames.at(video_) ? objects_[0] : objects_[1];

            auto metadataElement = MetadataElementForObjects(objects, firstFrame, lastFrame);

            selection = std::make_shared<PixelMetadataSpecification>(
                    "labels",
                    metadataElement);
            auto metadataManager = std::make_unique<metadata::MetadataManager>(video_, *selection);
            hasObjects = metadataManager->framesForMetadata().size();

            if (outQueryObject)
                *outQueryObject = combineStrings(objects);
        }

        return selection;
    }

private:
    std::string video_;
    std::vector<std::vector<std::string>> objects_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
//    std::uniform_int_distribution<int> startingFrameDistribution_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;
};

class VRWorkload4Generator : public WorkloadGenerator {
    // objects[0] is the primary query object.
    // objects[1] is the alternate query object.
public:
    VRWorkload4Generator(const std::string &video, const std::vector<std::string> &objects, unsigned int numberOfQueries, unsigned int duration, std::default_random_engine *generator,
                         std::unique_ptr<FrameGenerator> startingFrameDistribution)
            : video_(video), objects_(objects), totalNumberOfQueries_(numberOfQueries), numberOfFramesInDuration_(duration * VideoToFramerate.at(video)),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution))
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
            firstFrame = startingFrameDistribution_->nextFrame(*generator_);
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
//    std::uniform_int_distribution<int> startingFrameDistribution_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;
    unsigned int alternateQueryObjectNumber_;
};

enum class Distribution {
    Uniform,
    Zipf,
    Window,
};

static std::unordered_map<std::string, std::unordered_map<int, std::vector<std::vector<std::string>>>> VideoToWorkloadToObjects {
        {"narrator", {{1, {{"person"}}}, {3, {{"person"}, {"car"}}},
                             {7, {{"person"}}},
                             {8, {{"car"}}}}},
        {"square_with_fountain", {{1, {{"person"}}}, {3, {{"person"}, {"bicycle"}}},
                                         {7, {{"person"}}},
                                         {8, {{"bicycle"}}}}},
        {"street_night_view", {{1, {{"person"}}}, {3, {{"person"}, {"car"}}},
                                      {7, {{"person"}}},
                                      {8, {{"car"}}}}},
        {"river_boat", {{1, {{"person"}}}, {3, {{"person"}, {"boat"}}}}},
        {"market_all", {{3, {{"person"}, {"car"}}},
                               {7, {{"person"}}},
                               {8, {{"car"}}}}},
        {"Netflix_BoxingPractice", {{3, {{"person"}}}}},
        {"Netflix_FoodMarket2", {{3, {{"person"}}}}},
        {"seeking", {{3, {{"person"}}}}},
        {"tennis", {{3, {{"person"}, {"sports_ball"}, {"tennis_racket"}}},
                           {7, {{"person"}}},
                           {8, {{"sports_ball"}}},
                           {9, {{"tennis_racket"}}}}},
        {"birdsincage", {{3, {{"bird"}}}}},
        {"crowdrun", {{3, {{"person"}}}}},
        {"elfuente1", {{3, {{"person"}}}}},
        {"elfuente2", {{3, {{"person"}}}}},
        {"oldtown", {{3, {{"car"}}}}},
        {"red_kayak", {{3, {{"boat", "surfboard"}}}}},
        {"touchdown_pass", {{3, {{"person"}}}}},
        {"park_joy_2k", {{3, {{"person"}}}}},
        {"park_joy_4k", {{3, {{"person"}}}}},
        {"Netflix_ToddlerFountain", {{3, {{"person"}}}}},
        {"Netflix_Narrator", {{3, {{"person"}}}}},
        {"Netflix_FoodMarket", {{3, {{"person"}}}}},
        {"Netflix_DrivingPOV", {{3, {{"car", "truck"}}}}},
};


static std::vector<std::string> GetObjectsForWorkload(unsigned int workloadNum, const std::string &video) {
    if (!VideoToWorkloadToObjects.count(video)) {
        if (workloadNum == 1)
            return {"car", "truck", "bus", "motorbike"};
        else if (workloadNum == 3 || workloadNum == 2 || workloadNum == 6)
            return {"car", "truck", "bus", "motorbike", "person"};
        else if (workloadNum == 5)
            return {"car", "truck", "bus", "motorbike", "person", "traffic_light"};

        assert(false);
    }

    std::vector<std::vector<std::string>> &objectsList = VideoToWorkloadToObjects.at(video).at(workloadNum);
    std::vector<std::string> mergedObjects;
    for (const auto &objects : objectsList)
        mergedObjects.insert(mergedObjects.end(), objects.begin(), objects.end());

    return mergedObjects;
}

static std::vector<std::vector<std::string>> GetObjectSetsForWorkload(unsigned int workloadNum) {
    if (workloadNum == 1)
        return std::vector<std::vector<std::string>>{{"car", "truck", "bus", "motorbike"}};
    else if (workloadNum == 3)
        return std::vector<std::vector<std::string>>{{"car", "truck", "bus", "motorbike"}, {"person"}};
    else if (workloadNum == 5)
        return std::vector<std::vector<std::string>>{{"car", "truck", "bus", "motorbike"}, {"person"}, {"traffic_light"}};
}

static Distribution WORKLOAD_DISTRIBUTION = Distribution::Window;

static std::unique_ptr<WorkloadGenerator> GetGenerator(unsigned int workloadNum, const std::string &video, unsigned int duration,
        std::default_random_engine *generator, Distribution distribution = Distribution::Uniform) {
//    std::string object("car");
//    std::vector<std::string> objects{"car", "pedestrian"};

    std::vector<std::vector<std::string>> workloadObjects;
    if (!VideoToWorkloadToObjects.count(video)) {
        if (workloadNum == 1)
            workloadObjects = std::vector<std::vector<std::string>>{{"car", "truck", "bus", "motorbike"}};
        else if (workloadNum == 3 || workloadNum == 2 || workloadNum == 6)
            workloadObjects = std::vector<std::vector<std::string>>{{"car", "truck", "bus", "motorbike"},
                                                                    {"person"}};
        else if (workloadNum == 5)
            workloadObjects = std::vector<std::vector<std::string>>{{"car", "truck", "bus", "motorbike"},
                                                                    {"person"},
                                                                    {"traffic_light"}};
    } else {
        workloadObjects = VideoToWorkloadToObjects.at(video).at(workloadNum);
    }

    WINDOW_FRACTION = workloadNum == 6 ? 0.5 : 0.25;

    std::unique_ptr<FrameGenerator> frameGenerator;
    switch (distribution) {
        case Distribution::Uniform:
            frameGenerator.reset(new UniformFrameGenerator(video, duration));
            break;
        case Distribution::Zipf:
            frameGenerator.reset(new ZipfFrameGenerator(video, duration));
            break;
        case Distribution::Window:
            std::cout << "Window-fraction " << WINDOW_FRACTION << std::endl;
            frameGenerator.reset(new WindowUniformFrameGenerator(video, duration, WINDOW_FRACTION));
            break;
        default:
            assert(false);
    };

    if (workloadNum == 1) {
        assert(workloadObjects.size() == 1);
        return std::make_unique<VRWorkload1Generator>(video, workloadObjects.front(), duration, generator,
                                                      std::move(frameGenerator));
    } else if (workloadNum == 2) {
        return std::make_unique<VRWorkload2Generator>(video, workloadObjects, NUM_QUERIES, duration, generator, std::move(frameGenerator));
    } else if (workloadNum == 3) {
        return std::make_unique<VRWorkload3Generator>(video, workloadObjects, duration, generator,
                                                      std::move(frameGenerator));
    } else if (workloadNum == 4) {
        assert(false);
//        return std::make_unique<VRWorkload4Generator>(video, objects, NUM_QUERIES, duration, generator, std::move(frameGenerator));
    } else if (workloadNum == 5) {
        assert(workloadObjects.size() == 3);
        return std::make_unique<VRWorkload5Generator>(video, workloadObjects, duration, generator,
                                                      std::move(frameGenerator));
    } else if (workloadNum == 6) {
        assert(workloadObjects.size() == 2);
        return std::make_unique<VRWorkload6Generator>(video, workloadObjects, duration, generator, std::move(frameGenerator));
    } else if (workloadNum >= 7 && workloadNum <= 9) {
        assert(workloadObjects.size() == 1);
        return std::make_unique<VRWorkload1Generator>(video, workloadObjects.front(), duration, generator,
                                                      std::move(frameGenerator));
    } else {
            assert(false);
    }
}

// Workload 1: Select a single object
// TileStrategy: No tiles.
TEST_F(UnknownWorkloadTestFixture, testWorkloadNoTiles) {
    std::cout << "\nWorkload-strategy no-tiles" << std::endl;

    std::vector<std::string> videos{
//        "traffic-2k-001",
//        "car-pov-2k-000-shortened",
//        "car-pov-2k-001-shortened",
//        "traffic-4k-002-ds2k",
//        "traffic-4k-000",
//        "traffic-4k-002",
        "street_night_view",
        "square_with_fountain",
        "river_boat",
        "market_all",
        "narrator",
        "birdsincage",
        "crowdrun",
        "elfuente1",
        "elfuente2",
        "oldtown",
        "tennis",
        "seeking",
        "red_kayak",
        "touchdown_pass",
        "park_joy_2k",
        "park_joy_4k",
        "Netflix_ToddlerFountain",
        "Netflix_Narrator",
        "Netflix_FoodMarket",
        "Netflix_FoodMarket2",
        "Netflix_DrivingPOV",
        "Netflix_BoxingPractice",
    };

//    std::vector<int> workloads{3};
//    std::vector<Distribution> distributions{Distribution::Uniform}; // , Distribution::Zipf, Distribution::Window

    std::unordered_map<int, std::vector<Distribution>> workloadToDistributions{
//            {1, {Distribution::Uniform}},
//            {2, {Distribution::Zipf}},
//            {3, {Distribution::Zipf, Distribution::Window}},
//            {5, {Distribution::Zipf}},
//            {6, {Distribution::Window}},
            {3, {Distribution::Uniform}},
//            {7, {Distribution::Uniform}},
//            {8, {Distribution::Uniform}},
//            {9, {Distribution::Uniform}},
    };

//    std::string object("car");

//    std::default_random_engine generator(7);
    for (auto it = workloadToDistributions.begin(); it != workloadToDistributions.end(); ++it) {
        auto workloadNum = it->first;
        std::cout << "Workload " << workloadNum << std::endl;

        for (auto distribution : it->second) {
            std::cout << "Distribution: " << (int) distribution << std::endl;
            for (const auto &video : videos) {
                if (workloadNum == 5 && video == "traffic-4k-000")
                    continue;

                auto workloadObjects = GetObjectsForWorkload(workloadNum, video);
                std::cout << "Workload-objects " << combineStrings(workloadObjects) << std::endl;

                for (auto duration : videoToQueryDurations.at(video)) {
                    std::default_random_engine generator(videoToProbabilitySeed.at(video));
                    auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);

                    for (auto i = 0u; i < NUM_QUERIES; ++i) {
                        std::string object;
                        auto selection = queryGenerator->getNextQuery(i, &object);

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
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundWorkloadObjects) {
    std::cout << "\nWorkload-strategy pre-tile-around-workload-objects" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-000-shortened",
            "car-pov-2k-001-shortened",
            "traffic-4k-002-ds2k",
            "traffic-4k-000",
            "traffic-4k-002",
//            "narrator",
//            "market_all",
//            "square_with_fountain",
//            "street_night_view",
//            "river_boat",
    };

    std::vector<int> workloads{3};
    std::vector<Distribution> distributions{Distribution::Uniform, Distribution::Zipf, Distribution::Window}; //, Distribution::Window

//    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (auto workloadNum : workloads) {
        std::cout << "Workload " << workloadNum << std::endl;

        for (auto distribution : distributions) {
            std::cout << "Distribution: " << (int) distribution << std::endl;

            for (const auto &video : videos) {
                auto workloadObjects = combineStrings(GetObjectsForWorkload(workloadNum, video));
                std::cout << "Workload-objects " << workloadObjects << std::endl;

                // First, tile around all of the objects in the video.
//        auto catalogName = tileAroundAllObjects(video);
                auto catalogName = video + "-cracked-" + workloadObjects + "-smalltiles-duration" + std::to_string(VideoToFramerate.at(video)) + "-yolo";

                for (auto duration : videoToQueryDurations.at(video)) {
                    std::default_random_engine generator(videoToProbabilitySeed.at(video));
//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
                    auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);

                    for (auto i = 0u; i < NUM_QUERIES; ++i) {
                        std::string object;
                        auto selection = queryGenerator->getNextQuery(i, &object);

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
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundAllDetectedObjects) {
    std::cout << "\nWorkload-strategy pre-tile-around-all-objects" << std::endl;

    std::vector<std::string> videos{
//            "traffic-2k-001",
//            "car-pov-2k-000-shortened",
//            "car-pov-2k-001-shortened",
//            "traffic-4k-002-ds2k",
//            "traffic-4k-000",
//            "traffic-4k-002",
//            "narrator",
//            "market_all",
//            "square_with_fountain",
//            "street_night_view",
//            "river_boat",
//            "square_with_fountain",
//            "tennis",
//            "seeking",
//            "narrator",
//            "market_all",
//            "street_night_view",
//            "Netflix_BoxingPractice",
//            "Netflix_FoodMarket2",
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "square_with_fountain"

//            "street_night_view",
//            "square_with_fountain",
//            "river_boat",
//            "market_all",
//            "narrator",
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
            "oldtown",
            "tennis",
            "seeking",
            "red_kayak",
            "touchdown_pass",
            "park_joy_2k",
            "park_joy_4k",
            "Netflix_ToddlerFountain",
            "Netflix_Narrator",
            "Netflix_FoodMarket",
            "Netflix_FoodMarket2",
            "Netflix_DrivingPOV",
            "Netflix_BoxingPractice",
    };

//    std::vector<int> workloads{3}; // 1,
//    std::vector<Distribution> distributions{Distribution::Uniform}; // Distribution::Uniform, Distribution::Zipf,

    std::unordered_map<int, std::vector<Distribution>> workloadToDistributions{
//            {1, {Distribution::Uniform}},
//            {2, {Distribution::Zipf}},
//            {3, {Distribution::Zipf, Distribution::Window}},
//            {5, {Distribution::Zipf}},
//            {6, {Distribution::Window}},
            {3, {Distribution::Uniform}},
//            {7, {Distribution::Uniform}},
//            {8, {Distribution::Uniform}},
//            {9, {Distribution::Uniform}},
    };

//    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (auto it = workloadToDistributions.begin(); it != workloadToDistributions.end(); ++it) {
        auto workloadNum = it->first;
        std::cout << "Workload " << workloadNum << std::endl;

        for (auto distribution : it->second) {
            std::cout << "Distribution: " << (int) distribution << std::endl;

            for (const auto &video : videos) {
                if (workloadNum == 5 && video == "traffic-4k-000")
                    continue;

                auto workloadObjects = combineStrings(GetObjectsForWorkload(workloadNum, video));
                std::cout << "Workload-objects " << workloadObjects << std::endl;

                // First, tile around all of the objects in the video.
//        auto catalogName = tileAroundAllObjects(video);
                auto catalogName = video + "-cracked-" + "all_objects" + "-smalltiles-duration" + std::to_string(VideoToFramerate.at(video)); // + "-yolo";

                for (auto duration : videoToQueryDurations.at(video)) {
                    std::default_random_engine generator(videoToProbabilitySeed.at(video));
//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
                    auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);

                    for (auto i = 0u; i < NUM_QUERIES; ++i) {
                        std::string object;
                        auto selection = queryGenerator->getNextQuery(i, &object);

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
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundKNNDetections) {
    std::cout << "\nWorkload-strategy pre-tile-around-KNN-detections" << std::endl;

    std::vector<std::string> videos{
//            "traffic-2k-001",
//            "car-pov-2k-000-shortened",
//            "car-pov-2k-001-shortened",
//            "traffic-4k-002-ds2k",
//            "traffic-4k-000",
//            "traffic-4k-002",
//            "narrator",
//            "market_all",
//            "square_with_fountain",
//            "street_night_view",
//            "river_boat",
//            "square_with_fountain",
//            "tennis",
//            "seeking",
//            "narrator",
//            "market_all",
//            "street_night_view",
//            "Netflix_BoxingPractice",
//            "Netflix_FoodMarket2",
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "square_with_fountain"
//
//            "street_night_view",
//            "square_with_fountain",
//            "river_boat",
//            "market_all",
//            "narrator",
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "oldtown",
//            "tennis",
//            "seeking",
//            "red_kayak",
//            "touchdown_pass",
//            "park_joy_2k",
//            "park_joy_4k",
//            "Netflix_ToddlerFountain",
            "Netflix_Narrator",
            "Netflix_FoodMarket",
            "Netflix_FoodMarket2",
            "Netflix_DrivingPOV",
            "Netflix_BoxingPractice",
    };

//    std::vector<int> workloads{3}; // 1,
//    std::vector<Distribution> distributions{Distribution::Uniform}; // Distribution::Uniform, Distribution::Zipf,

    std::unordered_map<int, std::vector<Distribution>> workloadToDistributions{
//            {1, {Distribution::Uniform}},
//            {2, {Distribution::Zipf}},
//            {3, {Distribution::Zipf, Distribution::Window}},
//            {5, {Distribution::Zipf}},
//            {6, {Distribution::Window}},
            {3, {Distribution::Uniform}},
//            {7, {Distribution::Uniform}},
//            {8, {Distribution::Uniform}},
//            {9, {Distribution::Uniform}},
    };

//    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (auto it = workloadToDistributions.begin(); it != workloadToDistributions.end(); ++it) {
        auto workloadNum = it->first;
        std::cout << "Workload " << workloadNum << std::endl;

        for (auto distribution : it->second) {
            std::cout << "Distribution: " << (int) distribution << std::endl;

            for (const auto &video : videos) {
                if (workloadNum == 5 && video == "traffic-4k-000")
                    continue;

                auto workloadObjects = combineStrings(GetObjectsForWorkload(workloadNum, video));
                std::cout << "Workload-objects " << workloadObjects << std::endl;

                // First, tile around all of the objects in the video.
//        auto catalogName = tileAroundAllObjects(video);
                auto catalogName = video + "-cracked-" + "KNN" + "-smalltiles-duration" + std::to_string(VideoToFramerate.at(video)); // + "-yolo";

                for (auto duration : videoToQueryDurations.at(video)) {
                    std::default_random_engine generator(videoToProbabilitySeed.at(video));
//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
                    auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);

                    for (auto i = 0u; i < NUM_QUERIES; ++i) {
                        std::string object;
                        auto selection = queryGenerator->getNextQuery(i, &object);

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

    std::vector<int> workloads{1, 2, 3};
//    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (auto workloadNum : workloads) {
        std::cout << "Workload " << workloadNum << std::endl;
        for (const auto &video : videos) {
            // First, tile around all of the objects in the video.
//        auto catalogName = tileAroundAllObjects(video);
            auto catalogName = video + "-cracked-all_objects-smalltiles-duration-30";

            for (auto duration : videoToQueryDurations.at(video)) {
                if (duration > 60)
                    continue;

                std::default_random_engine generator(videoToProbabilitySeed.at(video));
//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
                auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, WORKLOAD_DISTRIBUTION);
                std::cout << "Distribution: " << (int) WORKLOAD_DISTRIBUTION << std::endl;

                for (auto i = 0u; i < NUM_QUERIES; ++i) {
                    std::string object;
                    auto selection = queryGenerator->getNextQuery(i, &object);

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
}

TEST_F(UnknownWorkloadTestFixture, testRetile4k) {
    Coordinator().execute(Scan("car-pov-2k-000-shortened").PrepareForCracking("car-pov-2k-000-shortened-cracked-2", 30));
}

TEST_F(UnknownWorkloadTestFixture, testPrepareForRetiling) {
    std::vector<std::string> videos{
//            "traffic-2k-001",
//            "car-pov-2k-001-shortened",
//            "traffic-4k-000",
//            "traffic-4k-002",
//        "car-pov-2k-000-shortened",
//        "traffic-4k-002-ds2k",
//        "narrator",
//        "market_all",
//        "square_with_fountain",
//        "street_night_view",
//        "river_boat",
//        "birdsincage"
//        "Netflix_BoxingPractice",
//        "Netflix_FoodMarket2",
//        "seeking",
//        "tennis",
        "birdsincage",
        "crowdrun",
        "elfuente1",
        "elfuente2"
    };

    for (const auto &video : videos) {
        Coordinator().execute(Scan(video).PrepareForCracking(video + "-cracked", VideoToFramerate.at(video)));
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

    std::vector<int> workloads{1, 2, 3};

    unsigned int framerate = 30;
//    std::string object("car");

//    std::default_random_engine generator(7);
    for (auto workloadNum : workloads) {
        std::cout << "Workload " << workloadNum << std::endl;
        for (const auto &video : videos) {
            std::string catalogName = video + "-cracked";
            for (auto duration : videoToQueryDurations.at(video)) {
                if (duration > 60)
                    continue;

                std::default_random_engine generator(videoToProbabilitySeed.at(video));

                // Delete tiles from previous runs.
                DeleteTiles(catalogName);

//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
                auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, WORKLOAD_DISTRIBUTION);
                std::cout << "Distribution: " << int(WORKLOAD_DISTRIBUTION) << std::endl;
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
                                CrackingStrategy::SmallTiles);
                        Coordinator().execute(retileOp);
                    }
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAllObjectsAroundQuery) {
    std::cout << "\nWorkload-strategy tile-query-duration-around-workload-objects" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-000-shortened",
            "car-pov-2k-001-shortened",
            "traffic-4k-002-ds2k",
            "traffic-4k-000",
            "traffic-4k-002",
//            "narrator",
//            "market_all",
//            "square_with_fountain",
//            "street_night_view",
//            "river_boat",
    };

//    unsigned int framerate = 30;
//    std::string object("car");
//    std::vector<std::string> queriedObjects{"car", "pedestrian"};

    std::vector<int> workloads{1, 3};
    std::vector<Distribution> distributions{Distribution::Uniform, Distribution::Zipf, Distribution::Window};

    for (auto workloadNum : workloads) {
        std::cout << "Workload " << workloadNum << std::endl;

        for (auto distribution : distributions) {
            std::cout << "Distribution: " << int(distribution) << std::endl;

            for (const auto &video : videos) {
                auto workloadObjects = GetObjectsForWorkload(workloadNum, video);
                std::cout << "Workload-objects " << combineStrings(workloadObjects) << std::endl;

                std::string catalogName = video + "-cracked";

                for (auto duration : videoToQueryDurations.at(video)) {
                    std::default_random_engine generator(videoToProbabilitySeed.at(video));

                    // Delete tiles from previous runs.
                    DeleteTiles(catalogName);
                    ResetTileNum(catalogName, 0);

//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
                    auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);
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
                        auto metadataElement = MetadataElementForObjects(workloadObjects, selection->firstFrame(),
                                                                         selection->lastFrame());
                        PixelMetadataSpecification queriedObjectsSelection("labels", metadataElement);

                        std::cout << "Video " << video << std::endl;
                        std::cout << "Cracking-around-objects " << object << std::endl;
                        std::cout << "Cracking-duration " << duration << std::endl;
                        std::cout << "Iteration " << i << std::endl;
                        std::cout << "First-frame " << selection->firstFrame() << std::endl;
                        std::cout << "Last-frame " << selection->lastFrame() << std::endl;

                        {
                            auto retileOp = ScanAndRetile(
                                    catalogName,
                                    queriedObjectsSelection,
                                    VideoToFramerate.at(video),
                                    CrackingStrategy::SmallTiles);
                            Coordinator().execute(retileOp);
                        }
                    }
                }

                // Delete tiles after run.
                DeleteTiles(catalogName);
                ResetTileNum(catalogName, 0);
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

//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
            auto queryGenerator = GetGenerator(WORKLOAD_NUM, video, duration, &generator, WORKLOAD_DISTRIBUTION);
            std::cout << "Distribution: " << (int)WORKLOAD_DISTRIBUTION << std::endl;
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

TEST_F(UnknownWorkloadTestFixture, testFailureCase) {
    std::string video("Netflix_BoxingPractice");
    std::string catalogName = video + "-cracked";
    DeleteTiles(catalogName);
    ResetTileNum(catalogName, 0);

    PixelMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", "person", 155, 215));

    std::cout << "Select from tiled" << std::endl;
    for (int i = 0; i < 3; ++i){
        auto input = ScanMultiTiled(catalogName, false);
        Coordinator().execute(input.Select(selection));
    }

    std::cout << "Select from not tiled" << std::endl;
    for (int i = 0; i < 3; ++i) {
        auto input = Scan(video);
        input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
        Coordinator().execute(input.Select(selection));
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

//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
            auto queryGenerator = GetGenerator(WORKLOAD_NUM, video, duration, &generator, WORKLOAD_DISTRIBUTION);
            std::cout << "Distribution: " << (int)WORKLOAD_DISTRIBUTION << std::endl;
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
//            VRWorkload2Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
            auto queryGenerator = GetGenerator(2, video, duration, &generator);
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

TEST_F(UnknownWorkloadTestFixture, testWorkloadStartWithKNNTiles) {

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    unsigned int framerate = 30;
//    std::string object("car");

    std::vector<int> workloads{1, 2, 3};

    for (auto workloadNum : workloads) {
//        std::default_random_engine workload1Generator(7);

        std::cout << "Workload " << workloadNum << std::endl;
        for (const auto &video : videos) {
            std::string catalogName = video + "-cracked-KNN-smalltiles-duration30";
            for (auto duration : videoToQueryDurations.at(video)) {
                if (duration > 60)
                    continue;

                std::default_random_engine generator( videoToProbabilitySeed.at(video));
                // Delete tiles from previous runs.
//            DeleteTilesPastNum(catalogName, videoToMaxKNNTile.at(video));

//            VRWorkload4Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
//            VRWorkload3Generator queryGenerator(video, {"car", "pedestrian"}, duration, &generator);
//            VRWorkload2Generator queryGenerator(video, {"car", "pedestrian"}, NUM_QUERIES, duration, &generator);
                auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, WORKLOAD_DISTRIBUTION);
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
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadStartWithKNNTilesTileAroundQueryIfDifferent) {
    std::cout << "\nWorkload-strategy tile-init-KNN-tiles-tile-query-if-different-0.6" << std::endl;

    std::vector<std::string> videos{
            "traffic-2k-001",
            "car-pov-2k-001-shortened",
            "traffic-4k-000",
            "traffic-4k-002",
    };

    unsigned int framerate = 30;
//    std::string object("car");

    std::vector<int> workloads{1, 2, 3};

    for (auto workloadNum : workloads) {
//        std::default_random_engine workload1Generator(7);

        std::cout << "Workload " << workloadNum << std::endl;
        for (const auto &video : videos) {
            std::string catalogName = video + "-cracked-KNN-smalltiles-duration30";
            for (auto duration : videoToQueryDurations.at(video)) {
                if (duration > 60)
                    continue;

                std::default_random_engine generator(videoToProbabilitySeed.at(video));
                // Delete tiles from previous runs.
                DeleteTilesPastNum(catalogName, videoToMaxKNNTile.at(video));
                ResetTileNum(catalogName, videoToMaxKNNTile.at(video));

                auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, WORKLOAD_DISTRIBUTION);
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
}

class TestTileAroundMoreObjectsManager : public TileAroundMoreObjectsManager {
public:
    TestTileAroundMoreObjectsManager(const std::string &video, unsigned int width, unsigned int height, unsigned int gopLength)
        : video_(video), width_(width), height_(height), gopLength_(gopLength) {}

    std::shared_ptr<tiles::TileConfigurationProvider> configurationProviderForGOPWithQuery(unsigned int gop, const std::vector<std::string> &queryObjects) override {
        bool layoutIsAroundAllQueryObjects = true;
        std::unordered_set<std::string> missingObjects;
        for (const auto &object : queryObjects) {
            if (!gopToLayoutObjects_[gop].count(object)) {
                layoutIsAroundAllQueryObjects = false;
                missingObjects.insert(object);
            }
        }
        if (layoutIsAroundAllQueryObjects)
            return {};

        gopToLayoutObjects_[gop].insert(missingObjects.begin(), missingObjects.end());
        auto metadataElement = MetadataElementForObjects(std::vector<std::string>(gopToLayoutObjects_[gop].begin(), gopToLayoutObjects_[gop].end()));
        auto metadataManager = std::make_shared<metadata::MetadataManager>(video_, MetadataSpecification("labels", metadataElement));
        return std::make_shared<tiles::GroupingTileConfigurationProvider>(
                gopLength_,
                metadataManager,
                width_,
                height_);
    }

private:
    std::string video_;
    unsigned int width_;
    unsigned int height_;
    unsigned int gopLength_;
    std::unordered_map<unsigned int, std::unordered_set<std::string>> gopToLayoutObjects_;
};

TEST_F(UnknownWorkloadTestFixture, testTileAroundMoreObjects) {
    std::string video("traffic-2k-001");
    auto catalog = video + "-cracked";
    DeleteTiles(catalog);
    ResetTileNum(catalog, 0);

    std::vector<std::vector<std::string>> objects{{"car", "truck"}, {"person"}};
    unsigned int width = 1920;
    unsigned int height = 1080;
    unsigned int gopLength = 30;

    auto tileManager = std::make_shared<TestTileAroundMoreObjectsManager>(video, width, height, gopLength);
    {
        PixelMetadataSpecification selection("labels", MetadataElementForObjects({"car", "truck"}, 0, 90));
        auto retileOp = ScanAndRetile(
                catalog,
                selection,
                30,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileAroundMoreObjects,
                {}, tileManager);
        Coordinator().execute(retileOp);
    }

    {
        PixelMetadataSpecification selection("labels", MetadataElementForObjects({"person"}, 0, 90));
        auto retileOp = ScanAndRetile(
                catalog,
                selection,
                30,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileAroundMoreObjects,
                {}, tileManager);
        Coordinator().execute(retileOp);
    }

    {
        PixelMetadataSpecification selection("labels", MetadataElementForObjects({"person"}, 0, 90));
        auto retileOp = ScanAndRetile(
                catalog,
                selection,
                30,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileAroundMoreObjects,
                {}, tileManager);
        Coordinator().execute(retileOp);
    }
}

TEST_F(UnknownWorkloadTestFixture, testRegretAccumulator) {
    std::string video("traffic-2k-001");
    auto catalog = video + "-cracked";
    DeleteTiles(catalog);
    ResetTileNum(catalog, 0);

    std::vector<std::vector<std::string>> objects{{"car", "truck"}, {"person"}};
    unsigned int width = 1920;
    unsigned int height = 1080;
    unsigned int gopLength = 30;

    auto regretACcumulator = std::make_shared<TASMRegretAccumulator>(video, width, height, gopLength, 1);

    {
        PixelMetadataSpecification selection("labels", MetadataElementForObjects({"car", "truck"}, 0, 90));
        auto retileOp = ScanAndRetile(
                catalog,
                selection,
                30,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileBasedOnRegret,
                regretACcumulator);
        Coordinator().execute(retileOp);
    }

    {
        PixelMetadataSpecification selection("labels", MetadataElementForObjects({"person"}, 0, 90));
        auto retileOp = ScanAndRetile(
                catalog,
                selection,
                30,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileBasedOnRegret,
                regretACcumulator);
        Coordinator().execute(retileOp);
    }

//    {
//        PixelMetadataSpecification selection("labels", MetadataElementForObjects({"person"}, 0, 90));
//        auto retileOp = ScanAndRetile(
//                catalog,
//                selection,
//                30,
//                CrackingStrategy::SmallTiles,
//                RetileStrategy::RetileBasedOnRegret,
//                regretACcumulator);
//        Coordinator().execute(retileOp);
//    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryAfterAccumulatingRegret) {
    std::cout << "\nWorkload-strategy tile-query-duration-if-regret-accumulates" << std::endl;

    std::vector<std::string> videos{
//            "traffic-2k-001",
//            "car-pov-2k-000-shortened",
//            "car-pov-2k-001-shortened",
//            "traffic-4k-002-ds2k",
//            "traffic-4k-000",
//            "traffic-4k-002",
//        "square_with_fountain",
//        "river_boat",

//            "tennis",
//            "seeking",
            "narrator",
            "market_all",
            "street_night_view",
//            "Netflix_BoxingPractice",
//            "Netflix_FoodMarket2",
//
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "square_with_fountain"
    };

//    std::vector<int> workloads{3};
//    std::vector<Distribution> distributions{Distribution::Uniform}; // ,, Distribution::Zipf, Distribution::Window
    std::unordered_map<int, std::vector<Distribution>> workloadToDistributions{
//            {1, {Distribution::Uniform}},
//            {2, {Distribution::Zipf}},
//            {3, {Distribution::Zipf, Distribution::Window}},
//            {5, {Distribution::Zipf}},
//            {6, {Distribution::Window}},
            {3, {Distribution::Uniform}},
            {7, {Distribution::Uniform}},
            {8, {Distribution::Uniform}},
//            {9, {Distribution::Uniform}},
    };

    std::vector<double> thresholds = {1};
    for (auto threshold : thresholds) {
        std::cout << "Threshold: " << threshold << std::endl;

        for (auto it = workloadToDistributions.begin(); it != workloadToDistributions.end(); ++it) {
            auto workloadNum = it->first;
            std::cout << "Workload " << workloadNum << std::endl;

            for (auto distribution : it->second) {
                std::cout << "Distribution: " << (int) distribution << std::endl;
                for (const auto &video : videos) {
                    if (workloadNum == 5 && video == "traffic-4k-000")
                        continue;

                    auto videoDimensions = VideoToDimensions.at(video);
                    std::string catalogName = video + "-cracked";
                    for (auto duration : videoToQueryDurations.at(video)) {

                        auto regretAccumulator = std::make_shared<TASMRegretAccumulator>(
                                video,
                                videoDimensions.first, videoDimensions.second, VideoToFramerate.at(video),
                                threshold);

                        std::default_random_engine generator(videoToProbabilitySeed.at(video));

                        // Delete tiles from previous runs.
                        DeleteTiles(catalogName);
                        ResetTileNum(catalogName, 0);

                        auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);
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
                                        VideoToFramerate.at(video),
                                        CrackingStrategy::SmallTiles,
                                        RetileStrategy::RetileBasedOnRegret,
                                        regretAccumulator);
                                Coordinator().execute(retileOp);
                            }
                        }

                        // Delete tiles from previous runs.
                        DeleteTiles(catalogName);
                        ResetTileNum(catalogName, 0);
                    }
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryAfterAccumulatingRegretStartWithKNN) {
    std::cout << "\nWorkload-strategy start-with-KNN-tile-query-duration-if-regret-accumulates" << std::endl;

    std::vector<std::string> videos{
//            "traffic-2k-001",
//            "car-pov-2k-000-shortened",
//            "car-pov-2k-001-shortened",
//            "traffic-4k-002-ds2k",
//            "traffic-4k-000",
//            "traffic-4k-002",
//        "square_with_fountain",
//        "river_boat",

//            "tennis",
//            "seeking",
//            "narrator",
//            "market_all",
//            "street_night_view",
//            "Netflix_BoxingPractice",
//            "Netflix_FoodMarket2",
//
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "square_with_fountain"

//            "street_night_view",
//            "square_with_fountain",
//            "river_boat",
//            "market_all",
//            "narrator",
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "oldtown",
//            "tennis",
//            "seeking",
//            "red_kayak",
//            "touchdown_pass",
//            "park_joy_2k",
//            "park_joy_4k",
            "Netflix_ToddlerFountain",
            "Netflix_Narrator",
            "Netflix_FoodMarket",
            "Netflix_FoodMarket2",
            "Netflix_DrivingPOV",
            "Netflix_BoxingPractice",
    };

//    std::vector<int> workloads{3};
//    std::vector<Distribution> distributions{Distribution::Uniform}; // ,, Distribution::Zipf, Distribution::Window
    std::unordered_map<int, std::vector<Distribution>> workloadToDistributions{
//            {1, {Distribution::Uniform}},
//            {2, {Distribution::Zipf}},
//            {3, {Distribution::Zipf, Distribution::Window}},
//            {5, {Distribution::Zipf}},
//            {6, {Distribution::Window}},
            {3, {Distribution::Uniform}},
//            {7, {Distribution::Uniform}},
//            {8, {Distribution::Uniform}},
//            {9, {Distribution::Uniform}},
    };

    std::vector<double> thresholds = {1};
    for (auto threshold : thresholds) {
        std::cout << "Threshold: " << threshold << std::endl;

        for (auto it = workloadToDistributions.begin(); it != workloadToDistributions.end(); ++it) {
            auto workloadNum = it->first;
            std::cout << "Workload " << workloadNum << std::endl;

            for (auto distribution : it->second) {
                std::cout << "Distribution: " << (int) distribution << std::endl;
                for (const auto &video : videos) {
                    if (workloadNum == 5 && video == "traffic-4k-000")
                        continue;

                    auto videoDimensions = VideoToDimensions.at(video);
//                    std::string catalogName = video + "-cracked";
                    auto catalogName = video + "-cracked-" + "KNN" + "-smalltiles-duration" + std::to_string(VideoToFramerate.at(video));
                    for (auto duration : videoToQueryDurations.at(video)) {

                        auto regretAccumulator = std::make_shared<TASMRegretAccumulator>(
                                video,
                                videoDimensions.first, videoDimensions.second, VideoToFramerate.at(video),
                                threshold);

                        std::default_random_engine generator(videoToProbabilitySeed.at(video));

                        // Delete tiles from previous runs.
                        DeleteTilesPastNum(catalogName, videoToMaxKNNTile.at(video));
                        ResetTileNum(catalogName, videoToMaxKNNTile.at(video));

                        auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);
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
                                        VideoToFramerate.at(video),
                                        CrackingStrategy::SmallTiles,
                                        RetileStrategy::RetileBasedOnRegret,
                                        regretAccumulator);
                                Coordinator().execute(retileOp);
                            }
                        }

                        // Delete tiles from previous runs.
                        DeleteTilesPastNum(catalogName, videoToMaxKNNTile.at(video));
                        ResetTileNum(catalogName, videoToMaxKNNTile.at(video));
                    }
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryAfterAccumulatingRegretStartWithAllObj) {
    std::cout << "\nWorkload-strategy start-with-all-obj-tile-query-duration-if-regret-accumulates" << std::endl;

    std::vector<std::string> videos{
//            "traffic-2k-001",
//            "car-pov-2k-000-shortened",
//            "car-pov-2k-001-shortened",
//            "traffic-4k-002-ds2k",
//            "traffic-4k-000",
//            "traffic-4k-002",
//        "square_with_fountain",
//        "river_boat",

//            "tennis",
//            "seeking",
//            "narrator",
//            "market_all",
//            "street_night_view",
//            "Netflix_BoxingPractice",
//            "Netflix_FoodMarket2",
//
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "square_with_fountain"

            "street_night_view",
            "square_with_fountain",
            "river_boat",
            "market_all",
            "narrator",
            "birdsincage",
            "crowdrun",
            "elfuente1",
            "elfuente2",
            "oldtown",
            "tennis",
            "seeking",
            "red_kayak",
            "touchdown_pass",
            "park_joy_2k",
            "park_joy_4k",
            "Netflix_ToddlerFountain",
            "Netflix_Narrator",
            "Netflix_FoodMarket",
            "Netflix_FoodMarket2",
            "Netflix_DrivingPOV",
            "Netflix_BoxingPractice",
    };

//    std::vector<int> workloads{3};
//    std::vector<Distribution> distributions{Distribution::Uniform}; // ,, Distribution::Zipf, Distribution::Window
    std::unordered_map<int, std::vector<Distribution>> workloadToDistributions{
//            {1, {Distribution::Uniform}},
//            {2, {Distribution::Zipf}},
//            {3, {Distribution::Zipf, Distribution::Window}},
//            {5, {Distribution::Zipf}},
//            {6, {Distribution::Window}},
            {3, {Distribution::Uniform}},
//            {7, {Distribution::Uniform}},
//            {8, {Distribution::Uniform}},
//            {9, {Distribution::Uniform}},
    };

    std::vector<double> thresholds = {1};
    for (auto threshold : thresholds) {
        std::cout << "Threshold: " << threshold << std::endl;

        for (auto it = workloadToDistributions.begin(); it != workloadToDistributions.end(); ++it) {
            auto workloadNum = it->first;
            std::cout << "Workload " << workloadNum << std::endl;

            for (auto distribution : it->second) {
                std::cout << "Distribution: " << (int) distribution << std::endl;
                for (const auto &video : videos) {
                    if (workloadNum == 5 && video == "traffic-4k-000")
                        continue;

                    auto videoDimensions = VideoToDimensions.at(video);
//                    std::string catalogName = video + "-cracked";
                    auto catalogName = video + "-cracked-" + "all_objects" + "-smalltiles-duration" + std::to_string(VideoToFramerate.at(video));
                    for (auto duration : videoToQueryDurations.at(video)) {

                        auto regretAccumulator = std::make_shared<TASMRegretAccumulator>(
                                video,
                                videoDimensions.first, videoDimensions.second, VideoToFramerate.at(video),
                                threshold);

                        std::default_random_engine generator(videoToProbabilitySeed.at(video));

                        // Delete tiles from previous runs.
                        DeleteTilesPastNum(catalogName, videoToMaxAllObjTile.at(video));
                        ResetTileNum(catalogName, videoToMaxAllObjTile.at(video));

                        auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);
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
                                        VideoToFramerate.at(video),
                                        CrackingStrategy::SmallTiles,
                                        RetileStrategy::RetileBasedOnRegret,
                                        regretAccumulator);
                                Coordinator().execute(retileOp);
                            }
                        }

                        // Delete tiles from previous runs.
                        DeleteTilesPastNum(catalogName, videoToMaxAllObjTile.at(video));
                        ResetTileNum(catalogName, videoToMaxAllObjTile.at(video));
                    }
                }
            }
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, testWorkloadTileAroundQueryAroundMoreObjects) {
    std::cout << "\nWorkload-strategy tile-query-duration-around-more-objects" << std::endl;

    std::vector<std::string> videos{
//            "traffic-2k-001",
//            "car-pov-2k-000-shortened",
//            "car-pov-2k-001-shortened",
//            "traffic-4k-002-ds2k",
//            "traffic-4k-000",
//            "traffic-4k-002",
//        "square_with_fountain",
//        "river_boat",

//            "tennis",
//            "seeking",
            "narrator",
            "market_all",
            "street_night_view",
//            "Netflix_BoxingPractice",
//            "Netflix_FoodMarket2",
//
//            "birdsincage",
//            "crowdrun",
//            "elfuente1",
//            "elfuente2",
//            "square_with_fountain"
    };

//    std::vector<int> workloads{3};
//    std::vector<Distribution> distributions{Distribution::Uniform}; // ,Distribution::Uniform, Distribution::Zipf,

    std::unordered_map<int, std::vector<Distribution>> workloadToDistributions{
//            {1, {Distribution::Uniform}},
//            {2, {Distribution::Zipf}},
//            {3, {Distribution::Zipf, Distribution::Window}},
//            {5, {Distribution::Zipf}},
//            {6, {Distribution::Window}},
            {3, {Distribution::Uniform}},
            {7, {Distribution::Uniform}},
            {8, {Distribution::Uniform}},
//            {9, {Distribution::Uniform}},
    };

    for (auto it = workloadToDistributions.begin(); it != workloadToDistributions.end(); ++it) {
        auto workloadNum = it->first;
        std::cout << "Workload " << workloadNum << std::endl;

        for (auto distribution : it->second) {
            std::cout << "Distribution: " << (int) distribution << std::endl;
            for (const auto &video : videos) {
                if (workloadNum == 5 && video == "traffic-4k-000")
                    continue;

                auto videoDimensions = VideoToDimensions.at(video);
                std::string catalogName = video + "-cracked";
                for (auto duration : videoToQueryDurations.at(video)) {

                    auto reTileManager = std::make_shared<TestTileAroundMoreObjectsManager>(
                            video,
                            videoDimensions.first, videoDimensions.second, VideoToFramerate.at(video));

                    std::default_random_engine generator(videoToProbabilitySeed.at(video));

                    // Delete tiles from previous runs.
                    DeleteTiles(catalogName);
                    ResetTileNum(catalogName, 0);

                    auto queryGenerator = GetGenerator(workloadNum, video, duration, &generator, distribution);
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
                                    VideoToFramerate.at(video),
                                    CrackingStrategy::SmallTiles,
                                    RetileStrategy::RetileAroundMoreObjects,
                                    {}, reTileManager);
                            Coordinator().execute(retileOp);
                        }
                    }
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

TEST_F(UnknownWorkloadTestFixture, testTilingTimes) {

    PixelMetadataSpecification selection("labels", MetadataElementForObjects({"car", "truck", "bus", "motorbike"}, 9628, 11428));
//    {
//        for (int i = 0; i < 3; ++i) {
//            auto input = Scan("car-pov-2k-000-shortened");
//            input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
//            Coordinator().execute(input.Select(selection));
//        }
//    }

//    {
//        for (int i = 0; i < 3; ++i) {
//            auto input = ScanMultiTiled("car-pov-2k-000-shortened-cracked-car_truck_bus_motorbike-smalltiles-duration30-yolo", false);
//            Coordinator().execute(input.Select(selection));
//        }
//    }

    {
//        std::string catalogName = "car-pov-2k-000-shortened-cracked";
//        DeleteTiles(catalogName);
//        ResetTileNum(catalogName, 0);
        for (int i = 0; i < 3; ++i) {
            auto input = ScanMultiTiled("car-pov-2k-000-shortened-cracked-2", false);
            Coordinator().execute(input.Select(selection));
        }
    }
}

TEST_F(UnknownWorkloadTestFixture, tileNecessaryGOPs) {
    std::string catalogName = "car-pov-2k-000-shortened-cracked-2";
    DeleteTiles(catalogName);
    ResetTileNum(catalogName, 0);

    std::vector<int> gops{243, 242, 241, 240, 239, 238, 233, 232, 234, 235, 236, 237, 260,
    259, 258, 257, 256, 255, 254, 253, 252, 251, 250, 249, 248, 247,
    246, 225, 224, 223, 222, 221, 220, 219, 218, 217, 216, 212, 213,
    214, 215, 226, 227, 228, 229, 230, 231, 244, 245, 270, 269, 268,
    267, 266, 265, 264, 263, 262, 261, 186, 185, 184, 183, 157, 156,
    155, 154, 153, 152, 148, 145, 144, 143, 142, 141, 140, 211, 210,
    209, 208, 182, 181, 180, 179, 178, 172, 171, 173, 174, 175, 176,
    187, 188, 189, 190, 191, 192, 193, 194, 196, 197, 198, 199, 201,
    202, 203, 204, 205, 206, 207, 272, 271, 177, 200,  79,  78,  77,
    76,  75,  74,  58,  59,  63,  64,  66,  67,  68,  69,  70,  71,
    72,  73, 151, 150, 149, 147, 146, 139, 390, 389, 388, 387, 380,
    379, 378, 376, 375, 374, 373, 372, 324, 321, 280, 276, 275, 274,
    273, 283, 284, 285, 286, 287, 288, 309, 308, 307, 306, 305, 298,
    297, 296, 295, 294, 277, 278, 279, 281, 282, 165, 164, 163, 162,
    161, 158, 115, 114, 113, 112, 111, 106, 105, 107, 108, 109, 110,
    121, 122, 123, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135,
    136, 137, 138, 159, 160, 166, 167, 168, 169, 170, 408, 407, 371,
    370, 369, 406, 368, 405, 367, 366, 365, 364, 358, 361, 362, 363,
    377, 381, 386,  65,  62,  61,  60,  57,  56,  55,  54,  53,  52,
    51,  50,  49,  48,  47,  46,  45,  44,  43,  42,  41,  35,  38,
    39,  40, 125, 124,  98,  97, 103, 104, 116, 117, 118, 119, 120,
    319, 318, 317, 316, 315, 314, 313, 312, 311, 310, 304, 303, 302,
    301};

    std::vector<std::string> objects{"car", "truck", "bus", "motorbike"};
    for (auto gop : gops) {
        PixelMetadataSpecification selection("labels", MetadataElementForObjects(objects, gop * 30, (gop+1) * 30));
        auto retileOp = ScanAndRetile(
                catalogName,
                selection,
                30,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileAlways);
        Coordinator().execute(retileOp);
    }
}
