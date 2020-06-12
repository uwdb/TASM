#include "VideoManager.h"
#include <gtest/gtest.h>

#include "SemanticIndex.h"
#include "Video.h"
#include <cassert>

using namespace tasm;

class VideoManagerTestFixture : public testing::Test {
public:
    VideoManagerTestFixture() {}
};

TEST_F(VideoManagerTestFixture, testLoadVideoConfiguration) {
    Video vid("/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/red10/1-0-stream.mp4");

    assert(vid.configuration().displayWidth == 320);
    assert(vid.configuration().displayHeight == 240);
    assert(vid.configuration().codedWidth == 320);
    assert(vid.configuration().codedHeight == 256);
    assert(vid.configuration().frameRate == 25);
    assert(vid.configuration().codec == Codec::HEVC);
}

TEST_F(VideoManagerTestFixture, testScan) {
    VideoManager manager;
    manager.store("/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/red10/1-0-stream.mp4", "red10");
}

TEST_F(VideoManagerTestFixture, testSelect) {
    std::experimental::filesystem::path labels = "testLabels.db";

    // Delete DB at path.
    // Better would be to use an in-memory db for the tests.
    std::experimental::filesystem::remove(labels);
    auto semanticIndex = std::make_shared<SemanticIndexSQLite>(labels);

    std::string video("red10");
    std::string label("fish");
    for (int i = 0; i < 10; ++i)
        semanticIndex->addMetadata(video, label, i, 5, 5, 20, 100);

    auto metadataSelection = std::make_shared<SingleMetadataSelection>(label);
    std::shared_ptr<TemporalSelection> temporalSelection;

    VideoManager manager;
    manager.select(video, video, metadataSelection, temporalSelection, semanticIndex);

    std::experimental::filesystem::remove(labels);
}

