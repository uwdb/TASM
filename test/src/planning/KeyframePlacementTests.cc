#include "KeyframeFinder.h"
#include "HeuristicOptimizer.h"
#include "Metadata.h"
#include "SelectPixels.h"
#include "TestResources.h"
#include "extension.h"
#include <gtest/gtest.h>

#include "timer.h"
#include <iostream>
#include <random>
#include <regex>
#include <unordered_set>

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class KeyframePlacementTestFixture : public testing::Test {
public:
    KeyframePlacementTestFixture()
            : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

protected:
    Catalog catalog;
};

static std::vector<std::pair<int, int>> convertFramesToRanges(const std::vector<int> &orderedFrames) {
    std::vector<std::pair<int, int>> startEndRanges;
    auto it = orderedFrames.begin();
    auto firstValueInRange = *it++;
    auto currentValueInRange = firstValueInRange;
    while (it != orderedFrames.end()) {
        if (*it == currentValueInRange + 1) {
            ++currentValueInRange;
            ++it;
        } else {
            startEndRanges.emplace_back<std::pair<int, int>>(std::make_pair(firstValueInRange, currentValueInRange));
            firstValueInRange = *it++;
            currentValueInRange = firstValueInRange;
        }
    }
    // Get the last range.
    startEndRanges.emplace_back<std::pair<int, int>>(std::make_pair(firstValueInRange, currentValueInRange));
    return startEndRanges;
}

TEST_F(KeyframePlacementTestFixture, testEncodeVideosWithGOPs) {
    auto input = Scan("nature");
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store("nature2"));
}

TEST_F(KeyframePlacementTestFixture, testEncodeManyVideosWithGOPs) {
    std::vector<std::string> videos{
//        "traffic-2k-001",
        "traffic-4k-002-ds2k",
        "car-pov-2k-001-shortened",
        "traffic-4k-000",
    };

    std::vector<unsigned int> gopLengths{30, 60, 120, 480, 1800, 5400, static_cast<unsigned int>(-1)};
    for (const auto &video : videos) {
        for (auto gopLength : gopLengths) {
            auto storedVideoName = video + "-gop_" + std::to_string(gopLength);
            std::cout << "\nEncoding " << storedVideoName << std::endl;
            auto input = Scan(video);
            Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, gopLength}}).Store(storedVideoName));
        }
    }
}

TEST_F(KeyframePlacementTestFixture, testEncodeNatureVideoWithGOPs) {
    std::string video = "nature";
    // 23 fps, 3578 frames
    std::vector<unsigned int> gopLengths{23, 46, 92, 368, 690, 1380, static_cast<unsigned int>(-1)};
    for (auto gopLength : gopLengths) {
        auto storedVideoName = video + "-gop_" + std::to_string(gopLength);
        std::cout << "\nEncoding " << storedVideoName << std::endl;
        auto input = Scan(video);
        Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, gopLength}}).Store(storedVideoName));
    }
}

TEST_F(KeyframePlacementTestFixture, testEncodeVideoWithSpecificKeyframes) {
    auto input = Scan("birdsincage");
    std::unordered_set<int> keyframes{0, 10};
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, keyframes},
                                                       {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store("birdsincage_keyframes"));
}

/* RUN QUERIES ON EVEN GOPs */
TEST_F(KeyframePlacementTestFixture, testNatureQueriesOverEvenGOPs) {
    std::string object = "bird";
    FrameMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", object));
    std::vector<unsigned int> gopLengths{23, 46, 92, 368, 690, 1380, static_cast<unsigned int>(-1)};
    for (auto gopLength : gopLengths) {
        for (int i = 0; i < 4; ++i) {
            std::cout << "\ngop-length " << gopLength << std::endl;
            std::cout << "\nselection-object " << object << std::endl;
            std::string inputVideo = "nature-gop_" + std::to_string(gopLength);
            std::cout << "\ncatalog-video " << inputVideo << std::endl;
            std::cout << "\ninput-video nature" << std::endl;

            auto input = Scan(inputVideo);
            input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
            Coordinator().execute(input.Select(selection).Sink());
        }
    }
}

/* RUN RANDOM QUERIES ON NATURE VIDEO */
TEST_F(KeyframePlacementTestFixture, testRandomQueriesOverNature) {
    std::default_random_engine generator(3);

    std::string video("nature");
    unsigned int durationOfVideoInSeconds = 155;
    unsigned int thirdOfDuration = durationOfVideoInSeconds / 3;
    unsigned int framerate = 23;
    std::vector<unsigned int> selectionDurationInSeconds{1, 5, 10, 20, 30};
    for (auto duration : selectionDurationInSeconds) {
        for (auto r = 0u; r < 3; ++r) {
            std::uniform_int_distribution<int> firstThirdDistribution(0, (thirdOfDuration - duration) * framerate);
            std::uniform_int_distribution<int> secondThirdDistribution(thirdOfDuration * framerate,
                                                                       (2 * thirdOfDuration - duration) * framerate);
            std::uniform_int_distribution<int> thirdThirdDistribution(2 * thirdOfDuration * framerate,
                                                                      (durationOfVideoInSeconds - duration) * framerate);

            auto startOfFirstRange = firstThirdDistribution(generator);
            auto startOfSecondRange = secondThirdDistribution(generator);
            auto startOfThirdRange = thirdThirdDistribution(generator);

            std::vector<int> frames;
            frames.reserve(duration * framerate * 3);
            std::vector<int> firstFrames{startOfFirstRange, startOfSecondRange, startOfThirdRange};
            for (auto firstFrame : firstFrames) {
                for (auto i = 0u; i < duration * framerate; ++i)
                    frames.push_back(firstFrame + i);
            }
            auto frameSelection = std::make_shared<FrameSpecification>(frames);

            std::string rangeStarts = std::to_string(startOfFirstRange) + "_" + std::to_string(startOfSecondRange) + "_" + std::to_string(startOfThirdRange);
            std::string customVideoName = video + "-customGOP_noopt_dur" + std::to_string(duration) + "_" + rangeStarts;
            if (!Catalog::instance().exists(customVideoName)) {
                auto startEndRanges = convertFramesToRanges(frames);
                auto numFramesInNature = 3578;
                auto maxNumberOfKeyframes = 155;
                KeyframeFinder finder(numFramesInNature, maxNumberOfKeyframes, {startEndRanges});
                auto keyframes = finder.getKeyframes(numFramesInNature, maxNumberOfKeyframes);
                std::unordered_set<int> uniqueKeyframes(keyframes.begin(), keyframes.end());

                auto input = Scan(video);
                Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, uniqueKeyframes},
                                                                   {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store(customVideoName));
            }

//            std::vector<unsigned int> gopLengths{23, 46, 92, 368, 690, 1380, static_cast<unsigned int>(-1)};
//            for (auto gopLength : gopLengths) {
                for (int i = 0; i < 3; ++i) {
                    std::cout << "\ngop-length " << rangeStarts << std::endl;
                    std::cout << "selection-duration " << duration << std::endl;
                    std::cout << "starting-frames " << startOfFirstRange << "_" << startOfSecondRange << "_" << startOfThirdRange << std::endl;
//                    std::string inputVideo = "nature-gop_" + std::to_string(gopLength);
                    std::cout << "catalog-video " << customVideoName << std::endl;
                    std::cout << "base-video nature" << std::endl;

                    auto input = Scan(customVideoName);
                    input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
                    Coordinator().execute(input.Select(frameSelection).Sink());
                }
//            }

        }
    }
}

TEST_F(KeyframePlacementTestFixture, testBirdSelectionOnIdealGOPs) {
    std::string video("nature");
    FrameMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", "bird"));

    // Encode the video if it doesn't exist already.
    std::string savedName("nature-customGOP_noopt_bird");
    if (!Catalog::instance().exists(savedName)) {
        metadata::MetadataManager metadataManager(video, selection);
        auto framesForSelection = metadataManager.orderedFramesForMetadata();

        auto startEndRanges = convertFramesToRanges(framesForSelection);
        auto numFramesInNature = 3578;
        auto maxNumberOfKeyframes = 155;
        KeyframeFinder finder(numFramesInNature, maxNumberOfKeyframes, {startEndRanges});
        auto keyframes = finder.getKeyframes(numFramesInNature, maxNumberOfKeyframes);
        std::unordered_set<int> uniqueKeyframes(keyframes.begin(), keyframes.end());

        auto input = Scan(video);
        Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, uniqueKeyframes},
                                                           {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store(savedName));
    }

    // Then do selection.
    std::string object = "bird";
    for (int i = 0; i < 4; ++i) {
        std::cout << "\ngop-length " << "custom_bird" << std::endl;
        std::cout << "\nselection-object " << object << std::endl;
        std::cout << "catalog-video " << savedName << std::endl;
        std::cout << "base-video nature" << std::endl;

        auto input = Scan(savedName);
        input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
        Coordinator().execute(input.Select(selection).Sink());
    }
}
