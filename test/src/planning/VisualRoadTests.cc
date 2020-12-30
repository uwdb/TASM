#include "HeuristicOptimizer.h"
#include "Metadata.h"
#include "extension.h"

#include <cstdio>
#include <gtest/gtest.h>

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class VisualRoadTestFixture : public testing::Test {
public:
    VisualRoadTestFixture()
            : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

protected:
    Catalog catalog;
};

void DeleteDatabase(std::string id) {
    // Transform metadataIdentifier.
    auto crackedPos = id.find("-cracked");
    if (crackedPos != std::string::npos) {
        id = id.substr(0, crackedPos);
    } else {
        crackedPos = id.find_last_of("-");
        id = id.substr(0, crackedPos);
    }

    std::string dbPath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/" + id + ".db";
    remove(dbPath.c_str());
}

TEST_F(VisualRoadTestFixture, testDetectAndMask) {
    std::string videoId("traffic-4k-002-ds2k-not-tiled-cracked");
    DeleteDatabase(videoId);

    auto input = ScanMultiTiled(videoId);

    PixelsInFrameMetadataSpecification selection("labels",
            std::make_shared<SingleMetadataElement>("label", "person", 30, 120));
    auto yolo = lightdb::extensibility::Load("yologpu");

    // The first time, detection should run on all frames.
    Coordinator().execute(input.Select(selection, yolo));

    // The second time, detection shouldn't run on any frames.
    Coordinator().execute(input.Select(selection, yolo));

    // With this partially overlapping selection, 90 frames should be decoded, and YOLO should only run on 60 of them.
    PixelsInFrameMetadataSpecification partiallyOverlappingSelection("labels",
                                                 std::make_shared<SingleMetadataElement>("label", "person", 90, 180));
    Coordinator().execute(input.Select(partiallyOverlappingSelection, yolo));
}

TEST_F(VisualRoadTestFixture, testDetectOnGPU) {
    std::string video("traffic-4k-002-ds2k");
    auto input = Scan(video);

    auto yolo = lightdb::extensibility::Load("yologpu");
    auto mapped = input.Map(yolo);
    Coordinator().execute(mapped);
}
