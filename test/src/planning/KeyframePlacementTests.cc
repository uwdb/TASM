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

TEST_F(KeyframePlacementTestFixture, testGetKeyframes) {
    std::string video = "nature";
    FrameMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", "bird"));
    std::shared_ptr<metadata::MetadataManager> metadataManager = std::make_shared<metadata::MetadataManager>(video, selection);
    auto framesForSelection = metadataManager->orderedFramesForMetadata();

    std::cout << "\nFrames (" << framesForSelection.size() << "): " << std::endl;
    for (const auto &frame : framesForSelection)
        std::cout << frame << ", ";
    std::cout << std::endl;

    // Now want to transform frames for selection into start/end pairs.
    auto startEndRanges = convertFramesToRanges(framesForSelection);

    std::cout << "Ranges: " << std::endl;
    for (const auto &range : startEndRanges)
        std::cout << range.first << ", " << range.second << std::endl;
    std::cout << std::endl;
}
