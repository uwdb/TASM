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

TEST_F(KeyframePlacementTestFixture, testEncodeVideosWithGOPs) {
    auto input = Scan("nature");
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store("nature2"));
}
