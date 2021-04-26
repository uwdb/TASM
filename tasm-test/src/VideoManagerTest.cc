#include "VideoManager.h"
#include <gtest/gtest.h>

#include "SemanticIndex.h"
#include "LayoutDatabase.h"
#include "Video.h"
#include <cassert>

using namespace tasm;

class VideoManagerTestFixture : public testing::Test {
public:
    VideoManagerTestFixture() {}

protected:
    void SetUp() {
        // set test configuration
        std::unordered_map<std::string, std::string> options {
                {EnvironmentConfiguration::DefaultLayoutsDB, "test-layout.db"},
                {EnvironmentConfiguration::CatalogPath, "test-resources"},
                {EnvironmentConfiguration::DefaultLabelsDB, "test-labels.db"}
        };
        auto config = EnvironmentConfiguration::instance(EnvironmentConfiguration(options));
        std::experimental::filesystem::remove_all(config.defaultLayoutDatabasePath());
        std::experimental::filesystem::remove_all(config.catalogPath());
        std::experimental::filesystem::remove_all(config.defaultLabelsDatabasePath());

        // copy resources from video-manager-test-resources
        std::experimental::filesystem::copy("test-inputs/video-manager-test-resources/test-layout.db", config.defaultLayoutDatabasePath());
        std::experimental::filesystem::copy("test-inputs/video-manager-test-resources/test-resources", config.catalogPath(), std::experimental::filesystem::copy_options::recursive);

        LayoutDatabase::instance()->open();
    }
    void TearDown() {
        // remove test resources
        auto config = EnvironmentConfiguration::instance();
        std::experimental::filesystem::remove_all(config.defaultLayoutDatabasePath());
        std::experimental::filesystem::remove_all(config.catalogPath());
        std::experimental::filesystem::remove_all(config.defaultLabelsDatabasePath());
    }
};

TEST_F(VideoManagerTestFixture, DISABLED_createResources) {
    // For reference purposes, not use; SetUp and TearDown need to be commented out for resources to persist
    std::unordered_map<std::string, std::string> options {
            {EnvironmentConfiguration::DefaultLayoutsDB, "/home/kirsteng/tasm-dev/tasm-test/test-inputs/video-manager-test-resources/test-layout.db"},
            {EnvironmentConfiguration::CatalogPath, "/home/kirsteng/tasm-dev/tasm-test/test-inputs/video-manager-test-resources/test-resources"},
            {EnvironmentConfiguration::DefaultLabelsDB, "/home/kirsteng/tasm-dev/tasm-test/test-inputs/video-manager-test-resources/test-labels.db"}
    };
    auto config = EnvironmentConfiguration::instance(EnvironmentConfiguration(options));
    std::experimental::filesystem::remove(config.defaultLayoutDatabasePath());
    std::experimental::filesystem::remove_all(config.catalogPath());
    std::experimental::filesystem::remove(config.defaultLabelsDatabasePath());

    VideoManager manager;
    manager.store("/home/maureen/red_6sec_2k.mp4", "birdsincage-regret");
}

TEST_F(VideoManagerTestFixture, testLoadVideoConfiguration) {
    Video vid("test-inputs/video-manager-test-resources/1-0-stream.mp4");

    assert(vid.configuration().displayWidth == 320);
    assert(vid.configuration().displayHeight == 240);
    assert(vid.configuration().codedWidth == 320);
    assert(vid.configuration().codedHeight == 256);
    assert(vid.configuration().frameRate == 25);
    assert(vid.configuration().codec == Codec::HEVC);
}

TEST_F(VideoManagerTestFixture, testAccumulateRegret) {
    auto semanticIndex = SemanticIndexFactory::createInMemory();

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

TEST_F(VideoManagerTestFixture, testSelect) {
    auto semanticIndex = SemanticIndexFactory::createInMemory();

    std::string video("red10-2x2");
    std::string label("fish");
    for (int i = 0; i < 10; ++i)
        semanticIndex->addMetadata(video, label, i, 5, 5, 20, 100);

    auto metadataSelection = std::make_shared<SingleMetadataSelection>(label);
    std::shared_ptr<TemporalSelection> temporalSelection;

    VideoManager manager;
    manager.storeWithUniformLayout("test-inputs/video-manager-test-resources/red102k.mp4", "red10-2x2", 2, 2);
    manager.select(video, video, metadataSelection, temporalSelection, semanticIndex);
}



