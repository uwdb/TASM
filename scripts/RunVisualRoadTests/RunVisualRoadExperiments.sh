#!/bin/bash

#./lightdb_test --gtest_filter=VisualRoadTestFixture.testSetUpVideosForTest &> testSetUpVideosForTest.txt
#
#./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithDetectionAndTilingBefore &> testRunWorkloadWithDetectionAndTilingBefore.txt
#
#./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithNoTiling &> testRunWorkloadWithNoTiling.txt

#./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithZipfDistributionAndRegretTiling &> testRunWorkloadWithZipfDistributionAndRegretTiling.txt

#./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithZipfDistributionNoTiling &> testRunWorkloadWithZipfDistributionNoTiling.txt

#./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithZipfDistributionNoTilingMatchingQueryPlan &> testRunWorkloadWithZipfDistributionNoTilingMatchingQueryPlan.txt

./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithZipfDistributionAndDetectionAndRegretTiling &> testRunWorkloadWithZipfDistributionAndDetectionAndRegretTiling.txt

./lightdb_test --gtest_filter=VisualRoadTestFixture.testRunWorkloadWithZipfDistributionWithDetectNoTiling &> testRunWorkloadWithZipfDistributionWithDetectNoTiling.txt
