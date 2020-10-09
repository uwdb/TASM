#include <gtest/gtest.h>
#include "ContainsCar.h"

class PredicateTestFixture : public testing::Test {
public:
    PredicateTestFixture() {}
};

TEST_F(PredicateTestFixture, testLoadModel) {
    auto containsCar = ContainsCarPredicate();
    containsCar.loadModel();
}

TEST_F(PredicateTestFixture, testDetect) {
    auto containsCar = ContainsCarPredicate();
    containsCar.loadModel();

    containsCar.detectImage("/home/maureen/PPdemo/data/detrac/MVI_39931-img00002.jpg");
}

