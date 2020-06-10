#include "VideoManager.h"
#include <gtest/gtest.h>

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
//    Video vid("/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/red10/1-0-stream.mp4");

    VideoManager manager;
    manager.store("/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/red10/1-0-stream.mp4", "red10");
}

