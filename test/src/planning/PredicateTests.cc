#include <gtest/gtest.h>
#include "ContainsCar.h"

#include "HeuristicOptimizer.h"
#include "Display.h"
#include "Metadata.h"

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class PredicateTestFixture : public testing::Test {
public:
    PredicateTestFixture()
        : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

protected:
    Catalog catalog;
};

TEST_F(PredicateTestFixture, testLoadModel) {
    auto containsCar = ContainsCarPredicate();
    containsCar.loadModel();
}

TEST_F(PredicateTestFixture, testDetect) {
    auto containsCar = ContainsCarPredicate();
    containsCar.loadModel();
    char base[] = "/home/maureen/PPdemo/data/detrac/MVI_39931-img00000.jpg";
    for (auto i = 1u; i < 2; ++i) {
        sprintf(base, "/home/maureen/PPdemo/data/detrac/MVI_39931-img%05d.jpg", i);
        auto boxes = containsCar.detectImage(base);
        std::cout << "Frame " << i << ", " << boxes.size() << "cars" << std::endl;
    }
}

TEST_F(PredicateTestFixture, testIngestMVIVideo) {
    auto input = Load("/home/maureen/PPdemo/data/detrac/MVI_39931.mp4",
                      Volume::limits(),
                      GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    std::string savePath = "MVI_39931";
    Coordinator().execute(input.Store(savePath));
}

TEST_F(PredicateTestFixture, testDetectOnFrames) {
    std::string video = "MVI_39931";
    auto input = Scan(video);
    Coordinator().execute(input.Predicate());
}

