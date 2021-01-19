#!/bin/bash

./lightdb_test --gtest_filter=VisualRoadTestFixture.testSetUpVideosForTest &> testSetUpVideosForTest.txt

./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithDetectionAndTilingBefore &> testRunWorkloadWithDetectionAndTilingBefore.txt

./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithNoTiling &> testRunWorkloadWithNoTiling.txt
