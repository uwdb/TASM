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
    manager.store("/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/birdsincage/1-0-stream.mp4", "birdsincage-regret");
}

TEST_F(VideoManagerTestFixture, testSelect) {
    auto semanticIndex = std::make_shared<SemanticIndexSQLiteInMemory>();

    std::string video("red10-2x2");
    std::string label("fish");
    for (int i = 0; i < 10; ++i)
        semanticIndex->addMetadata(video, label, i, 5, 5, 20, 100);

    auto metadataSelection = std::make_shared<SingleMetadataSelection>(label);
    std::shared_ptr<TemporalSelection> temporalSelection;

    VideoManager manager;
    manager.storeWithUniformLayout("/home/maureen/red102k.mp4", "red10-2x2", 2, 2);
    manager.select(video, video, metadataSelection, temporalSelection, semanticIndex);
}

TEST_F(VideoManagerTestFixture, testAccumulateRegret) {
    auto semanticIndex = std::make_shared<SemanticIndexSQLiteInMemory>();

    std::string video("birdsincage-regret");
    std::string metadataIdentifier("birdsincage");
    std::string label("fish");
    std::string label2("horse");
    for (auto i = 0u; i < 10; ++i) {
        semanticIndex->addMetadata(metadataIdentifier, label, i, 5, 5, 260, 166);
        semanticIndex->addMetadata(metadataIdentifier, label2, i, 1800, 800, 1915, 1070);
    }

    auto metadataSelection = std::make_shared<SingleMetadataSelection>(label);
    auto secondMetadataSelection = std::make_shared<SingleMetadataSelection>(label2);
    std::shared_ptr<TemporalSelection> temporalSelection;

    VideoManager videoManager;
    videoManager.activateRegretBasedRetilingForVideo(video, metadataIdentifier, semanticIndex, 0.5);

    for (int i = 0; i < 5; ++i) {
        videoManager.select(video, metadataIdentifier, metadataSelection, temporalSelection, semanticIndex);
        if (!i)
            videoManager.select(video, metadataIdentifier, secondMetadataSelection, temporalSelection, semanticIndex);
    }
    videoManager.retileVideoBasedOnRegret(video);
}

