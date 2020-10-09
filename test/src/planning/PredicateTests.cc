#include <gtest/gtest.h>
#include "ContainsCar.h"

class PredicateTestFixture : public testing::Test {
public:
    PredicateTestFixture() {}
};

TEST_F(PredicateTestFixture, testHW) {
    assert(true);
}
