#include "AssertVideo.h"
#include "DropFrames.h"
#include "HeuristicOptimizer.h"
#include "Greyscale.h"
#include "Display.h"
#include "Metadata.h"
#include "SelectFramesWithObjectsInProximity.h"
#include "SelectPixels.h"
#include "TestResources.h"
#include "extension.h"
#include <gtest/gtest.h>

#include "timer.h"
#include <iostream>

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class VisitorTestFixture : public testing::Test {
public:
    VisitorTestFixture()
            : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

protected:
    Catalog catalog;
};

TEST_F(VisitorTestFixture, testBaz) {
    auto name = "red10";
    auto input = Scan(name);
    auto temporal = input.Select(SpatiotemporalDimension::Time, TemporalRange{2, 5});
    auto encoded = temporal.Encode();

    Coordinator().execute(encoded);
}

TEST_F(VisitorTestFixture, testFindDogAndPerson) {
    auto input = Load("/home/maureen/dog_videos/dog.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto dogAndPersonFrames = input.Map(SelectFramesWithObjectsInProximity).Save("/home/maureen/dog_videos/dog_and_person.hevc");
    Coordinator().execute(dogAndPersonFrames);
}

TEST_F(VisitorTestFixture, testSampleFrames) {
    auto yolo = lightdb::extensibility::Load("yolo");
    auto input = Load("/home/maureen/visualroad_videos/traffic-000.mp4", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto sampled = input.Discretize(Dimension::Time, 30).Map(yolo).Save("/home/maureen/visualroad_videos/traffic-000-sampled30-labels.boxes"); //.Store("traffic-000-sampled", Codec::hevc(), {GeometryReference::make<IntervalGeometry>(Dimension::Time, 30)});
    Coordinator().execute(sampled);
}

TEST_F(VisitorTestFixture, testReadSampledFrames) {
    auto input = Scan("traffic-000-sampled");
    Coordinator().execute(input);
}

TEST_F(VisitorTestFixture, testDropFrames) {
    // "/home/maureen/dog_videos/dog.hevc"
    // "/home/maureen/uadetrac_videos/MVI_20011/MVI_20011.hevc"
    auto input = Load("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto shortened = input.Map(DropFrames).Save("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011_car_frames_gpu.hevc");
    Coordinator().execute(shortened);
}

TEST_F(VisitorTestFixture, testSelectPixels) {
    // "/home/maureen/dog_videos/dog_pixels_gpu.hevc"
    auto input = Load("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto selected = input.Map(SelectPixels).Save("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011_car_pixels_gpu.hevc");
    Coordinator().execute(selected);
}

TEST_F(VisitorTestFixture, testMakeBoxes) {
    auto yolo = lightdb::extensibility::Load("yolo");
    auto input = Load("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto query = input.Map(yolo).Save("/home/maureen/uadetrac_videos/MVI_20011/labels/mvi_20011_boxes.boxes");
    Coordinator().execute(query);
}

TEST_F(VisitorTestFixture, testMapAndBoxThings) {
/*    auto left = Scan("red10");
    auto right = Scan("red10");
    auto unioned = left.Union(right);
    auto encoded = unioned.Encode();
    */
    //auto foo = dlopen("/home/bhaynes/projects/yolo/cmake-build-debug/libyolo.so", RTLD_LAZY | RTLD_GLOBAL);
    //printf( "Could not open file : %s\n", dlerror() );
    auto yolo = lightdb::extensibility::Load("yolo"); //, "/home/bhaynes/projects/yolo/cmake-build-debug");

    auto input = Load("/home/maureen/dog_videos/dog.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Map(DropFrames).Save("/home/maureen/dog_videos/dog_dog.hevc"));
//    auto boxes = Load("/home/maureen/dog_videos/short_dog_labels/dog_short_dog_labels.boxes", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
//    Coordinator().execute(boxes.Union(input).Save("/home/maureen/dog_videos/boxes_on_dogs.hevc"));

//    auto input = Load("/home/maureen/dog_videos/dogBoxes.boxes", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
//    Coordinator().execute(input);

//    auto boxesOnInput = input.Map(yolo).Union(input).Save("/home/maureen/boxes_on_dog.hevc");
//    auto boxes = input.Map(yolo);
//    Coordinator().execute(boxes.Save("/home/maureen/lightdb/dogBoxes.boxes"));

//    Coordinator().execute(boxes.Union(input).Save("/home/maureen/dog_videos/boxes_on_dogs.hevc"));
}

TEST_F(VisitorTestFixture, testSaveToCatalog) {
    auto input = Load("/home/maureen/MVI_20052/sampled_frames/sampled.mp4", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Store("uadetrac"));
}

TEST_F(VisitorTestFixture, testBar) {
    auto name = "red10";
    auto input = Scan(name);
    auto input2 = Scan(name);
    auto stored = input.Store("postred10_2");
    auto stored2 = input.Store("postred10_3");


    Coordinator().execute({stored, stored2});
}

TEST_F(VisitorTestFixture, testScanStore) {
    auto name = "red10";
    auto input = Scan(name);
    auto stored = input.Store("postred10");

    Coordinator().execute(stored);
}

TEST_F(VisitorTestFixture, testScanSave) {
    auto name = "red10";
    auto input = Scan(name);
    auto stored = input.Encode(Codec::hevc()).Save("dout.mp4");

    Coordinator().execute(stored);
}

TEST_F(VisitorTestFixture, testInterpolateDiscretizeMap) {
    auto yolo = lightdb::extensibility::Load("yolo");

    auto name = "red10";
    auto input = Scan(name);
    auto continuous_t = input.Interpolate(Dimension::Theta, interpolation::Linear());
    auto continuous = continuous_t.Interpolate(Dimension::Phi, interpolation::Linear());
    auto discrete_t = continuous.Discretize(Dimension::Theta, rational_times_real({2, 416}, PI));
    auto downsampled = discrete_t.Discretize(Dimension::Phi, rational_times_real({1, 416}, PI));
    auto boxes = downsampled.Map(yolo);
    auto encoded = boxes.Encode(Codec::boxes());

    //Coordinator().execute(encoded);
    GTEST_SKIP();
}

TEST_F(VisitorTestFixture, testPartitionEncode) {
    auto name = "red10";
    auto input = Scan(name);
    auto partitioned = input.Partition(Dimension::Theta, rational_times_real({2, 4}, PI));
    auto encoded = partitioned.Encode(Codec::hevc());

    Coordinator().execute(encoded);
}

TEST_F(VisitorTestFixture, testPartitionPartitionEncode) {
    auto name = "red10";
    auto input = Scan(name);
    auto partitioned1 = input.Partition(Dimension::Theta, rational_times_real({2, 4}, PI));
    auto partitioned2 = partitioned1.Partition(Dimension::Theta, rational_times_real({2, 4}, PI));
    auto encoded = partitioned2.Encode(Codec::hevc());

    Coordinator().execute(encoded);
}

TEST_F(VisitorTestFixture, testPartitionSubqueryUnion) {
    auto name = "red10";
    auto input = Scan(name);
    auto partitioned_t = input.Partition(Dimension::Theta, rational_times_real({2, 4}, PI));
    auto partitioned = partitioned_t.Partition(Dimension::Phi, rational_times_real({1, 4}, PI));
    //auto transcoded = partitioned_t.Subquery([](auto l) { return l.Encode(Codec::hevc()); });
    auto transcoded = partitioned.Subquery([](auto l) { return l.Encode(Codec::hevc()); });
    auto encoded = transcoded.Encode(Codec::hevc());

    Coordinator().execute(encoded);
}

TEST_F(VisitorTestFixture, testScanInterpolateDiscretize) {
    auto outputResolution = 416;

    auto input = Scan(Resources.red10.name);
    auto continuous = input.Interpolate(Dimension::Theta, interpolation::Linear());
    auto smallTheta = continuous.Discretize(Dimension::Theta, rational_times_real({2, outputResolution}, PI));
    auto small = smallTheta.Discretize(Dimension::Phi, rational_times_real({1, outputResolution}, PI));
    auto saved = small.Save(Resources.out.hevc);


    Coordinator().execute(saved);

    EXPECT_VIDEO_VALID(Resources.out.hevc);
    EXPECT_VIDEO_FRAMES(Resources.out.hevc, Resources.red10.frames);
    EXPECT_VIDEO_RESOLUTION(Resources.out.hevc, outputResolution, outputResolution);
    EXPECT_VIDEO_RED(Resources.out.hevc);
    EXPECT_EQ(remove(Resources.out.hevc), 0);
}
