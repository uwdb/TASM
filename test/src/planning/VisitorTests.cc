#include "AssertVideo.h"
#include "DropFrames.h"
#include "HeuristicOptimizer.h"
#include "Greyscale.h"
#include "Display.h"
#include "Metadata.h"
#include "SelectPixels.h"
#include "TestResources.h"
#include "extension.h"
#include <gtest/gtest.h>

#include "timer.h"
#include <iostream>
#include <random>

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

//TEST_F(VisitorTestFixture, testDropFrames) {
//    // "/home/maureen/dog_videos/dog.hevc"
//    // "/home/maureen/uadetrac_videos/MVI_20011/MVI_20011.hevc"
//    auto input = Load("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
//    auto shortened = input.Map(DropFrames).Save("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011_car_frames.hevc");
//    Coordinator().execute(shortened);
//}

//TEST_F(VisitorTestFixture, testSelectPixels) {
//    // "/home/maureen/dog_videos/dog_pixels_gpu.hevc"
//    auto input = Load("/home/maureen/dog_videos/runningDog.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
//    auto selected = input.Map(SelectPixels).Save("/home/maureen/dog_videos/runningDogPixels.hevc");
//    Coordinator().execute(selected);
//}

TEST_F(VisitorTestFixture, testMakeBoxes) {
    auto yolo = lightdb::extensibility::Load("yolo");
    auto input = Load("/home/maureen/dog_videos/dog_with_keyframes.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto query = input.Map(yolo).Save("/home/maureen/dog_videos/dog_with_keyframes.boxes");
    Coordinator().execute(query);
}

TEST_F(VisitorTestFixture, testDrawBoxes) {
    auto input = Load("/home/maureen/uadetrac_videos/MVI_20011/MVI_20011.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto boxes = Load("/home/maureen/uadetrac_videos/MVI_20011/labels/person_mvi_20011_boxes.boxes", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(boxes.Union(input).Save("/home/maureen/boxes.hevc"));

}

//TEST_F(VisitorTestFixture, testMapAndBoxThings) {
///*    auto left = Scan("red10");
//    auto right = Scan("red10");
//    auto unioned = left.Union(right);
//    auto encoded = unioned.Encode();
//    */
//    //auto foo = dlopen("/home/bhaynes/projects/yolo/cmake-build-debug/libyolo.so", RTLD_LAZY | RTLD_GLOBAL);
//    //printf( "Could not open file : %s\n", dlerror() );
//    auto yolo = lightdb::extensibility::Load("yolo"); //, "/home/bhaynes/projects/yolo/cmake-build-debug");
//
//    auto input = Load("/home/maureen/dog_videos/dog.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
//    Coordinator().execute(input.Map(DropFrames).Save("/home/maureen/dog_videos/dog_dog.hevc"));
////    auto boxes = Load("/home/maureen/dog_videos/short_dog_labels/dog_short_dog_labels.boxes", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
////    Coordinator().execute(boxes.Union(input).Save("/home/maureen/dog_videos/boxes_on_dogs.hevc"));
//
////    auto input = Load("/home/maureen/dog_videos/dogBoxes.boxes", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
////    Coordinator().execute(input);
//
////    auto boxesOnInput = input.Map(yolo).Union(input).Save("/home/maureen/boxes_on_dog.hevc");
////    auto boxes = input.Map(yolo);
////    Coordinator().execute(boxes.Save("/home/maureen/lightdb/dogBoxes.boxes"));
//
////    Coordinator().execute(boxes.Uniocdn(input).Save("/home/maureen/dog_videos/boxes_on_dogs.hevc"));
//}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames) {
//    auto input = Load("/home/maureen/dog_videos/dog_with_keyframes.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    auto input = Scan("dog_with_keyframes_real");
    MetadataSpecification metadataSelection("LABELS", "LABEL", "dog");
    Coordinator().execute(input.Select(metadataSelection).Store("dog_with_dog_selected"));
//    Coordinator().execute(input.Select(metadataSelection).Save("/home/maureen/dog_videos/dog_with_dog_selected.hevc"));
//    Coordinator().execute(input.Save("/home/maureen/test-add-pic-output-flag.hevc"));
}

TEST_F(VisitorTestFixture, testScanAndSink) {
    auto input = Scan("traffic-4k");
    Coordinator().execute(input.Sink());
}

TEST_F(VisitorTestFixture, testCrackIntoTiles) {
    auto input = ScanByGOP("traffic-4k-002");
    Coordinator().execute(input.StoreCracked("traffic-4k-002-alternating"));
}

TEST_F(VisitorTestFixture, testScanMultiTiled) {
    auto input = ScanMultiTiled("traffic-2k-cracked-2");
    Coordinator().execute(input);
}

TEST_F(VisitorTestFixture, testScanAndSave) {
    auto input = Load("/home/maureen/traffic-4k-002.mp4", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Store("traffic-4k-002"));
}

TEST_F(VisitorTestFixture, testCrackBasedOnMetadata) {
    auto input = Scan("traffic-4k-002");
    MetadataSpecification metadataSelection("labels", "label", "car");
    Coordinator().execute(input.StoreCracked("traffic-4k-002-cracked-grouping-extent-entire-video", "traffic-4k-002", &metadataSelection));
}

TEST_F(VisitorTestFixture, testCrackBasedOnMetadata2) {
    auto input = Scan("traffic-2k-001");
    MetadataSpecification metadataSelection("labels", "label", "car");
    Coordinator().execute(input.StoreCracked("traffic-2k-001-cracked-grouping-extent-entire-video", "traffic-2k-001", &metadataSelection));
}

TEST_F(VisitorTestFixture, testExecuteCracking) {
    PixelMetadataSpecification selection("labels", "label", "car", 8670, 8790);
//
//    auto input = Scan("traffic-2k");
//    Coordinator().execute(input.Select(selection).Sink());

    auto input = ScanMultiTiled("traffic-2k-single-tile");
    Coordinator().execute(input.Select(selection, true).Sink());
}

TEST_F(VisitorTestFixture, testRemoveTiles) {
    REMOVE_TILES()
}

TEST_F(VisitorTestFixture, testIdealCrackingOnAlternatingSelections) {
    std::unordered_map<unsigned int, unsigned int> timeRangeToNumIterations{
//            {1, 60},
            {2, 30},
            {3, 30}};

    for (auto it = timeRangeToNumIterations.begin(); it != timeRangeToNumIterations.end(); ++it) {
        REMOVE_TILES()

        auto timeRange = it->first;
        auto numberOfRounds = it->second;

        std::default_random_engine generator(1);

        auto numberOfFramesInTimeRange = timeRange * 60 * 30;
        auto totalNumberOfFrames = 27000;

        std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);

        for (auto i = 0u; i < numberOfRounds; ++i) {
            auto label = i % 2 ? "pedestrian" : "car";

            unsigned int start = distribution(generator) / 30 * 30;

            auto method = "crack-to-grouped-extent-aroundallobjects-duration30-tiled";
            {
                auto catalogEntry = "traffic-4k-002-single-tile";
//                auto catalogEntry = "traffic-2k-001";
                PixelMetadataSpecification selection("labels", "label", label, start,
                                                     start + numberOfFramesInTimeRange);


                std::cout << std::endl << "\n\nStep: Selecting pixels with method " << method << " for object " << label
                          << " from " << catalogEntry
                          << " from frames for " << timeRange
                          << " min, from " << start << " to " << start + numberOfFramesInTimeRange << std::endl;

                auto tiled = ScanMultiTiled(catalogEntry);
//                auto notTiled = Scan(catalogEntry);
//                Coordinator().execute(notTiled.Select(selection).Sink());
                Coordinator().execute(tiled.Select(selection, true, true).Sink());
            }

            sleep(3);
            GLOBAL_TIMER.reset();
        }
    }
}

TEST_F(VisitorTestFixture, testLayoutImpactOnSelection) {
    std::unordered_map<unsigned int, unsigned int> timeRangeToNumIterations{
//            {1, 60},
            {2, 20},
            {3, 20}};

    for (auto it = timeRangeToNumIterations.begin(); it != timeRangeToNumIterations.end(); ++it) {
        REMOVE_TILES()

        auto timeRange = it->first;
        auto numberOfRounds = it->second;

        std::default_random_engine generator(1);

        auto numberOfFramesInTimeRange = timeRange * 60 * 30;
        auto totalNumberOfFrames = 27000;

        std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);

        for (auto i = 0u; i < numberOfRounds; ++i) {
            unsigned int start = distribution(generator) / 30 * 30;
            //            {
//                auto label = "pedestrian";
//                auto catalogEntry = "traffic-4k-002-cracked-groupingextent-layoutduration60-pedestrian";
////                auto catalogEntry = "traffic-2k-001-single-tile-pedestrian";
//                PixelMetadataSpecification selection("labels", "label", label, start, start+numberOfFramesInTimeRange);
//
//
//                std::cout << std::endl << "\n\nStep: Selecting pixels with method " << method << " for object " << label
//                                                << " from " << catalogEntry
//                                                << " from frames for " << timeRange
//                                                << " min, from " << start << " to " << start + numberOfFramesInTimeRange << std::endl;
//
////                auto notTiled = Scan(catalogEntry);
//                auto idealTiled = ScanMultiTiled(catalogEntry);
//                Coordinator().execute(idealTiled.Select(selection).Sink());
//            }
//
//            sleep(3);
//            GLOBAL_TIMER.reset();

            {
                auto method = "group-extent-tiled-layoutduration30";

                auto label = "car";
                auto catalogEntry = "traffic-4k-002-downsampled2k-cracked-groupingextent-layoutduration30-car";
                PixelMetadataSpecification selection("labels", "label", label, start, start+numberOfFramesInTimeRange);


                std::cout << std::endl << "\n\nStep: Selecting pixels with method " << method << " for object " << label
                          << " from " << catalogEntry
                          << " from frames for " << timeRange
                          << " min, from " << start << " to " << start + numberOfFramesInTimeRange << std::endl;

//                auto notTiled = Scan(catalogEntry);
                auto idealTiled = ScanMultiTiled(catalogEntry);
                Coordinator().execute(idealTiled.Select(selection).Sink());
            }

            sleep(3);
            GLOBAL_TIMER.reset();

            {
                auto method = "group-extent-tiled-layoutduration60";

                auto label = "car";
                auto catalogEntry = "traffic-4k-002-downsampled2k-cracked-groupingextent-layoutduration60-car";
                PixelMetadataSpecification selection("labels", "label", label, start, start+numberOfFramesInTimeRange);


                std::cout << std::endl << "\n\nStep: Selecting pixels with method " << method << " for object " << label
                          << " from " << catalogEntry
                          << " from frames for " << timeRange
                          << " min, from " << start << " to " << start + numberOfFramesInTimeRange << std::endl;

//                auto notTiled = Scan(catalogEntry);
                auto idealTiled = ScanMultiTiled(catalogEntry);
                Coordinator().execute(idealTiled.Select(selection).Sink());
            }

            sleep(3);
            GLOBAL_TIMER.reset();
        }
    }
}

TEST_F(VisitorTestFixture, debugTilingByCracking) {
    std::default_random_engine generator(1);
    unsigned int timeRange = 2;
    auto numberOfFramesInTimeRange = timeRange * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);
    auto numberOfRounds = 1u;
    auto method = "ideal-tiled-dimsAlignedTo32-sortByHeight";
    auto layoutDuration = 30;
    auto catalogEntry = "traffic-2k-001-single-tile";

    // Get to the questionable iteration.
    for (auto i = 0u; i < 2; ++i)
        distribution(generator);

    auto object = "car";
    unsigned int start = distribution(generator) / 30 * 30;
    PixelMetadataSpecification selection("labels", "label", object, start, start + numberOfFramesInTimeRange);

    auto input = ScanMultiTiled(catalogEntry);
    Coordinator().execute(input.Select(selection, true));
}

TEST_F(VisitorTestFixture, testBasicSelection) {
    auto catalogEntry = "traffic-2k-001-cracked-grouping-extent-entire-video";
    auto object = "car";

    PixelMetadataSpecification selection("labels", "label", object);
    auto input = ScanMultiTiled(catalogEntry);
//    input.downcast<ScannedLightField>().setWillReadEntireEntry(false); // To force scan by GOP.
    Coordinator().execute(input.Select(selection));
}

TEST_F(VisitorTestFixture, testTilingOnDecode30) {
    std::default_random_engine generator(1);
    unsigned int timeRange = 2;
    auto numberOfFramesInTimeRange = timeRange * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);
    auto numberOfRounds = 1u;
    auto method = "ideal-dimsAlignedTo64-20atOnce-noWaitForFrame";
    auto layoutDuration = 30;
    auto catalogEntry = "traffic-2k-001-cracked-dimsAlignedTo64-layoutduration30-car";

    // Get to the questionable iteration for visual profiler.
    for (auto i = 0u; i < 3; ++i)
        distribution(generator);

    for (auto i = 0u; i < numberOfRounds; ++i) {
        unsigned int start = distribution(generator) / 30 * 30;

        auto object = "car";
        PixelMetadataSpecification selection("labels", "label", object, start, start + numberOfFramesInTimeRange);
        {
            std::cout << std::endl << "\n\nStep: entry: " << catalogEntry << ", object: " << object
                      << ", time-range: " << timeRange << ", strategy: " << method
                      << ", tile-layout-duration: " << layoutDuration
                      << ", first-frame: " << start
                      << ", last-frame: " << start + numberOfFramesInTimeRange << std::endl;

            auto input = ScanMultiTiled(catalogEntry);
            Coordinator().execute(input.Select(selection));
        }

        GLOBAL_TIMER.reset();
        RECONFIGURE_DECODER_TIMER.reset();
        READ_FROM_NEW_FILE_TIMER.reset();
        sleep(3);
    }
}


TEST_F(VisitorTestFixture, testTilingOnDecode60) {
    std::default_random_engine generator(1);
    unsigned int timeRange = 2;
    auto numberOfFramesInTimeRange = timeRange * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);
    auto numberOfRounds = 15u;
    auto method = "groupextent-alignedTo32";
    auto layoutDuration = 60;
    auto catalogEntry = "traffic-2k-001-cracked-groupingextent-alignedTo32-layoutduration60-car"; //"traffic-2k-001-cracked-layoutduration60-car";
    for (auto i = 0u; i < numberOfRounds; ++i) {
        unsigned int start = distribution(generator) / 30 * 30;

        auto object = "car";
        PixelMetadataSpecification selection("labels", "label", object, start, start + numberOfFramesInTimeRange);
        {
            std::cout << std::endl << "\n\nStep: entry: " << catalogEntry << ", object: " << object
                      << ", time-range: " << timeRange << ", strategy: " << method
                      << ", tile-layout-duration: " << layoutDuration
                      << ", first-frame: " << start
                      << ", last-frame: " << start + numberOfFramesInTimeRange << std::endl;

            auto input = ScanMultiTiled(catalogEntry);
            Coordinator().execute(input.Select(selection));
        }

        GLOBAL_TIMER.reset();
        RECONFIGURE_DECODER_TIMER.reset();
        READ_FROM_NEW_FILE_TIMER.reset();
        sleep(3);
    }
}

TEST_F(VisitorTestFixture, testTilingOnDecode120) {
    std::default_random_engine generator(1);
    unsigned int timeRange = 2;
    auto numberOfFramesInTimeRange = timeRange * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);
    auto numberOfRounds = 15u;
    auto method = "groupextent-alignedTo32";
    auto layoutDuration = 120;
    auto catalogEntry = "traffic-2k-001-cracked-groupingextent-alignedTo32-layoutduration120-car"; //"traffic-2k-001-cracked-layoutduration60-car";
    for (auto i = 0u; i < numberOfRounds; ++i) {
        unsigned int start = distribution(generator) / 30 * 30;

        auto object = "car";
        PixelMetadataSpecification selection("labels", "label", object, start, start + numberOfFramesInTimeRange);
        {
            std::cout << std::endl << "\n\nStep: entry: " << catalogEntry << ", object: " << object
                      << ", time-range: " << timeRange << ", strategy: " << method
                      << ", tile-layout-duration: " << layoutDuration
                      << ", first-frame: " << start
                      << ", last-frame: " << start + numberOfFramesInTimeRange << std::endl;

            auto input = ScanMultiTiled(catalogEntry);
            Coordinator().execute(input.Select(selection));
        }

        GLOBAL_TIMER.reset();
        RECONFIGURE_DECODER_TIMER.reset();
        READ_FROM_NEW_FILE_TIMER.reset();
        sleep(3);
    }
}

TEST_F(VisitorTestFixture, testTileLayoutDurationOnSelectPixels) {
    std::vector<unsigned int> timeRanges{2, 5};
    for (auto timeRange : timeRanges) {
        std::vector<std::string> entries{"traffic-2k-001-cracked-layoutduration30-car",
                                         "traffic-2k-001-cracked-layoutduration60-car",
                                         "traffic-2k-001-cracked-layoutduration120-car",
                                         "traffic-2k-001-cracked-layoutduration240-car"};
        for (auto &entry : entries) {
            std::default_random_engine generator(1);

            auto numberOfFramesInTimeRange = timeRange * 60 * 30;
            auto totalNumberOfFrames = 27000;

            std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);

            auto numberOfRounds = 20u;

            for (auto i = 0u; i < numberOfRounds; ++i) {
                unsigned int start = distribution(generator) / 30 * 30;

                PixelMetadataSpecification selection("labels", "label", "car", start,
                                                     start + numberOfFramesInTimeRange);

                {
                    std::cout << std::endl << "\n\nStep: Selecting pixels from " << entry
                              << " from frames for "
                              << timeRange << " min, from " << start << " to " << start + numberOfFramesInTimeRange
                              << std::endl;
                    auto evenCracked = ScanMultiTiled(entry);
                    Coordinator().execute(evenCracked.Select(selection).Sink());
                }

                sleep(3);
                GLOBAL_TIMER.reset();
            }
        }
    }
}

TEST_F(VisitorTestFixture, testPerformanceOnRandomSelectPixels) {
    REMOVE_TILES()

    std::default_random_engine generator(1);

    const unsigned int fps = 30;
    const unsigned int minimumNumberOfSelectionSeconds = 30;
    const unsigned int maximumNumberOfSelectionSeconds = 180;
    std::uniform_int_distribution<int> timeRangeDistribution(minimumNumberOfSelectionSeconds, maximumNumberOfSelectionSeconds);

    auto totalNumberOfFrames = 27000;
    // Make sure that no matter what start point we pick, it is far enough from the end even for the maximum selection range.
    // *30 because 30 FPS
    std::uniform_int_distribution<int> startingPointDistribution(0, totalNumberOfFrames - maximumNumberOfSelectionSeconds * fps);

    auto method = "cracking";
    auto numberOfRounds = 40u;
    for (auto i = 0u; i < numberOfRounds; ++i) {
        auto start = startingPointDistribution(generator) / 30 * 30;
        auto duration = timeRangeDistribution(generator);
        auto numberOfFramesInTimeRange = duration * fps;

        {
            auto label = "car";
            PixelMetadataSpecification selection("labels", "label", label, start, start + numberOfFramesInTimeRange);

            auto catalogEntry = "traffic-2k-001-single-tile-car";
            std::cout << std::endl << "\n\nStep: Selecting pixels with method " << method << " for object " << label << " at " << catalogEntry << " from frames for " << duration << " sec, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
            auto input = ScanMultiTiled(catalogEntry);
            Coordinator().execute(input.Select(selection, true).Sink());
        }

        sleep(3);
        GLOBAL_TIMER.reset();

        {
            auto label = "pedestrian";
            PixelMetadataSpecification selection("labels", "label", label, start, start + numberOfFramesInTimeRange);

            auto catalogEntry = "traffic-2k-001-single-tile-pedestrian";
            std::cout << std::endl << "\n\nStep: Selecting pixels with method " << method << " for object " << label << " at " << catalogEntry << " from frames for " << duration << " sec, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
            auto input = ScanMultiTiled(catalogEntry);
            Coordinator().execute(input.Select(selection, true).Sink());
        }

        sleep(3);
        GLOBAL_TIMER.reset();

//        {
//            auto catalogEntry = "traffic-2k-cracked-gop60";
//            std::cout << std::endl << "\n\nStep: Selecting pixels with method ideal-tiled at " << catalogEntry << " from frames for " << duration << " sec, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
//            auto idealCracked = ScanMultiTiled(catalogEntry);
//            Coordinator().execute(idealCracked.Select(selection).Sink());
//        }
//
//        sleep(3);
//        GLOBAL_TIMER.reset();
//
//        {
//            auto catalogEntry = "traffic-2k-single-tile";
//            std::cout << std::endl << "\n\nStep: Selecting pixels with method cracking at " << catalogEntry << " from frames for " << duration << " sec, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
//            auto crackingInProgress = ScanMultiTiled("traffic-2k-single-tile");
//            Coordinator().execute(crackingInProgress.Select(selection, true).Sink());
//        }
//
//        sleep(3);
//        GLOBAL_TIMER.reset();

    }
}

TEST_F(VisitorTestFixture, testCrackingPerformanceOnRandomSelectPixels) {
    std::default_random_engine generator(1);

    const unsigned int fps = 30;
    const unsigned int minimumNumberOfSelectionSeconds = 30;
    const unsigned int maximumNumberOfSelectionSeconds = 180;
    std::uniform_int_distribution<int> timeRangeDistribution(minimumNumberOfSelectionSeconds, maximumNumberOfSelectionSeconds);

    auto totalNumberOfFrames = 27000;
    // Make sure that no matter what start point we pick, it is far enough from the end even for the maximum selection range.
    // *30 because 30 FPS
    std::uniform_int_distribution<int> startingPointDistribution(0, totalNumberOfFrames - maximumNumberOfSelectionSeconds * fps);

    auto numberOfRounds = 40u;
    for (auto i = 0u; i < numberOfRounds; ++i) {
        auto start = startingPointDistribution(generator) / 30 * 30;
        auto duration = timeRangeDistribution(generator);
        auto numberOfFramesInTimeRange = duration * fps;

        PixelMetadataSpecification selection("labels", "label", "car", start, start + numberOfFramesInTimeRange);

        {
            std::cout << std::endl << "\n\nStep: Selecting pixels when cracking video from frames for " << duration << " sec, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
            auto crackingInProgress = ScanMultiTiled("traffic-2k-single-tile");
            Coordinator().execute(crackingInProgress.Select(selection, true).Sink());
        }

        sleep(3);
        GLOBAL_TIMER.reset();

    }
}


TEST_F(VisitorTestFixture, testCrackingImpactOnSelectPixels) {
    std::default_random_engine generator(1);

    auto timeRangeInMinutes = 1;
    // * 60 seconds / minute * 30 frames / second
    auto numberOfFramesInTimeRange = timeRangeInMinutes * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);

    auto numberOfRounds = 30u;

    for (auto i = 0u; i < numberOfRounds; ++i) {
        unsigned int start = distribution(generator) / 30 * 30;

        PixelMetadataSpecification selection("labels", "label", "car", start, start + numberOfFramesInTimeRange);

        {
            std::cout << std::endl << "\n\nStep: Selecting pixels in not cracked video for " << timeRangeInMinutes << " min, from frames " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
            auto notCracked = Scan("traffic-2k");
            Coordinator().execute(notCracked.Select(selection).Sink());
        }

//        sleep(3);

//        {
//            std::cout << std::endl << "\n\nStep: Selecting pixels in a custom-tiled video from frames for " << timeRangeInMinutes << " min, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
//            auto idealCracked = ScanMultiTiled("traffic-2k-cracked-gop60");
//            Coordinator().execute(idealCracked.Select(selection).Sink());
//        }
//
//        {
//            std::cout << std::endl << "\n\nStep: Selecting pixels in a 3x3 cracked video from frames for " << timeRangeInMinutes << " min, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
//            auto evenCracked = ScanMultiTiled("traffic-2k-cracked3x3");
//            Coordinator().execute(evenCracked.Select(selection).Sink());
//        }

        sleep(3);
        GLOBAL_TIMER.reset();

//        {
//            std::cout << std::endl << "\n\nStep: Selecting pixels while cracking from frames for " << timeRangeInMinutes << " min, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
//            auto crackingInProgress = ScanMultiTiled("traffic-2k-single-tile");
//            Coordinator().execute(crackingInProgress.Select(selection, true).Sink());
//        }
    }
}

TEST_F(VisitorTestFixture, testCrackingImpactOnSelectPixels3min) {
    std::default_random_engine generator(1);

    auto timeRangeInMinutes = 3;
    // * 60 seconds / minute * 30 frames / second
    auto numberOfFramesInTimeRange = timeRangeInMinutes * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);

    auto numberOfRounds = 30u;

    for (auto i = 0u; i < numberOfRounds; ++i) {
        unsigned int start = distribution(generator) / 30 * 30;

        PixelMetadataSpecification selection("labels", "label", "car", start, start + numberOfFramesInTimeRange);

        {
            std::cout << std::endl << "\n\nStep: Selecting pixels in a 3x3 cracked video from frames for " << timeRangeInMinutes << " min, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
            auto evenCracked = ScanMultiTiled("traffic-2k-cracked3x3");
            Coordinator().execute(evenCracked.Select(selection).Sink());
        }

        sleep(3);
        GLOBAL_TIMER.reset();
    }
}

TEST_F(VisitorTestFixture, testCrackingImpactOnSelectPixels2min) {
    std::default_random_engine generator(1);

    auto timeRangeInMinutes = 2;
    // * 60 seconds / minute * 30 frames / second
    auto numberOfFramesInTimeRange = timeRangeInMinutes * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);

    auto numberOfRounds = 30u;

    for (auto i = 0u; i < numberOfRounds; ++i) {
        unsigned int start = distribution(generator) / 30 * 30;

        PixelMetadataSpecification selection("labels", "label", "car", start, start + numberOfFramesInTimeRange);

        {
            std::cout << std::endl << "\n\nStep: Selecting pixels in a 3x3 cracked video from frames for " << timeRangeInMinutes << " min, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
            auto evenCracked = ScanMultiTiled("traffic-2k-cracked3x3");
            Coordinator().execute(evenCracked.Select(selection).Sink());
        }

        sleep(3);
        GLOBAL_TIMER.reset();
    }
}

TEST_F(VisitorTestFixture, testCrackingImpactOnSelectPixels1min) {
    std::default_random_engine generator(1);

    auto timeRangeInMinutes = 1;
    // * 60 seconds / minute * 30 frames / second
    auto numberOfFramesInTimeRange = timeRangeInMinutes * 60 * 30;
    auto totalNumberOfFrames = 27000;

    std::uniform_int_distribution<int> distribution(0, totalNumberOfFrames - numberOfFramesInTimeRange);

    auto numberOfRounds = 60u;

    for (auto i = 0u; i < numberOfRounds; ++i) {
        unsigned int start = distribution(generator) / 30 * 30;

        PixelMetadataSpecification selection("labels", "label", "car", start, start + numberOfFramesInTimeRange);

        {
            std::cout << std::endl << "\n\nStep: Selecting pixels in a 3x3 cracked video from frames for " << timeRangeInMinutes << " min, from " << start << " to " << start+numberOfFramesInTimeRange << std::endl;
            auto evenCracked = ScanMultiTiled("traffic-2k-cracked3x3");
            Coordinator().execute(evenCracked.Select(selection).Sink());
        }

        sleep(3);
        GLOBAL_TIMER.reset();
    }
}

TEST_F(VisitorTestFixture, testReadCrackedTiles) {
    auto input = ScanMultiTiled("MVI_63563_960x576_100frames_cracked");
    PixelMetadataSpecification selection("labels", "label", "van");
    Coordinator().execute(input.Select(selection).Sink()); //.Store("from_cracked"));
}

TEST_F(VisitorTestFixture, testReadNonCracked) {
    auto input = Scan("MVI_63563_960x576_100frames");
    PixelMetadataSpecification selection("labels", "label", "van");
    Coordinator().execute(input.Select(selection).Sink()); //.Store("vans_from_cracked"));
}

TEST_F(VisitorTestFixture, testScanTiled1) {
    // Has 2x2 tiling.
    auto input = ScanTiled("jackson_square_1hr_680x512_gops_for_tiles");
    PixelsInFrameMetadataSpecification selection("LABELS", "LABEL", "car");
    Coordinator().execute(input.Select(selection).Save("/home/maureen/noscope_videos/jackson_car_pixels.hevc")); //.Save("/home/maureen/uadetrac_videos/MVI_63563/tiles/selected.hevc"));
}

TEST_F(VisitorTestFixture, testScanTiledToPixels) {
    auto input = ScanTiled("jackson_square_gops_for_tiles");
    PixelMetadataSpecification selection("LABELS", "LABEL", "car");
    Coordinator().execute(input.Select(selection).Store("jackson_square_car_pixels"));
}

TEST_F(VisitorTestFixture, testScanNotTiledAndSelectPixelsInFrame) {
    auto input = Scan("jackson_square_1hr_680x512");
    PixelsInFrameMetadataSpecification selection("LABELS", "LABEL", "car");
    Coordinator().execute(input.Select(selection).Store("jackson_town_square_car_pixels_in_frame"));
}

TEST_F(VisitorTestFixture, testScanTiled2) {
    auto input = ScanTiled("MVI_63563_gop30");
    PixelMetadataSpecification selection("LABELS", "LABEL", "others");
    Coordinator().execute(input.Select(selection).Sink()); //.Store("MVI_63563_pixels"));
}

TEST_F(VisitorTestFixture, testScanSink) {
    auto input = Scan("MVI_63563_gop30");
    Coordinator().execute(input.Sink());
}

TEST_F(VisitorTestFixture, testScanCrop) {
    auto input = Scan("MVI_63563_gop30");
    auto selection = input.Select(PhiRange{0, rational_times_real({1, 2}, PI)});
    Coordinator().execute(selection.Store("cropped_mvi"));
}


TEST_F(VisitorTestFixture, testSaveToCatalog) {
    auto input = Load("/home/maureen/dog_videos/dog_with_keyframes.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 30u}}).Store("dog_with_gop_30"));
}

static const char *videoToScan = "/home/maureen/noscope_videos/jackson_town_square_1hr.hevc";
static const std::string videoCatalogName = "MVI_63563_gop30";
static const std::string labelCategory = "bus";

TEST_F(VisitorTestFixture, testLoadAndSelectFramesBasic) {
    auto input = Scan(videoCatalogName);
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectPixelsBasic) {
    auto input = Scan(videoCatalogName);
    PixelMetadataSpecification selection("labels", "label", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames250) {
    auto input = Scan(videoCatalogName + "_gop250");
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames60) {
    auto input = Scan(videoCatalogName + "_gop60");
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames30) {
    auto input = Scan(videoCatalogName + "_gop30");
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames15) {
    auto input = Scan(videoCatalogName + "_gop15");
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames10) {
    auto input = Scan(videoCatalogName + "_gop10");
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames5) {
    auto input = Scan(videoCatalogName + "_gop5");
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFrames1) {
    auto input = Scan(videoCatalogName + "_gop1");
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testLoadAndSelectFramesCustom) {
    auto input = Scan(videoCatalogName + "_" + labelCategory);
    MetadataSpecification selection("LABELS", "LABEL", labelCategory);
    Coordinator().execute(input.Select(selection).Store(videoCatalogName + "_selected"));
}

TEST_F(VisitorTestFixture, testSavingMetadata) {
//    auto input = Scan(videoCatalogName);
    auto filenames = { "upper_left.hevc", "lower_left.hevc", "upper_right.hevc", "lower_right.hevc", "black_tile.hevc" };
    for (auto &file : filenames) {
        auto input = Load(std::filesystem::path("/home/maureen/noscope_videos/tiles/2x2tiles") / file, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
        MetadataSpecification selection("LABELS", "LABEL", "car");
        Coordinator().execute(input.Encode(selection).Store("jackson_square_1hr_680x512_gops_for_tiles"));
    }
}

TEST_F(VisitorTestFixture, testLoadIntoCatalog) {
    auto input = Load("/home/maureen/noscope_videos/tiles/2x2tiles/jackson_town_square_1hr_640x512.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Store("jackson_square_1hr_680x512"));

//    auto input = Load("/home/maureen/noscope_videos/tiles/shortened/black-tile-0.hevc", Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
//    MetadataSpecification selection("LABELS", "LABEL", "car");
//    Coordinator().execute(input.Encode(selection).Store("jackson_square_gops_for_tiles"));
}

TEST_F(VisitorTestFixture, testGOPSaving250) {
    auto input = Load(videoToScan, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 250u}}).Store(videoCatalogName + "_gop250"));
}

TEST_F(VisitorTestFixture, testGOPSaving60) {
    auto input = Load(videoToScan, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 60u}}).Store(videoCatalogName + "_gop60"));
}

TEST_F(VisitorTestFixture, testGOPSaving30) {
    auto input = Load(videoToScan, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 30u}}).Store(videoCatalogName + "_gop30"));
}

TEST_F(VisitorTestFixture, testGOPSaving15) {
    auto input = Load(videoToScan, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 15u}}).Store(videoCatalogName + "_gop15"));
}

TEST_F(VisitorTestFixture, testGOPSaving10) {
    auto input = Load(videoToScan, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 10u}}).Store(videoCatalogName + "_gop10"));
}

TEST_F(VisitorTestFixture, testGOPSaving5) {
    auto input = Load(videoToScan, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 5u}}).Store(videoCatalogName + "_gop5"));
}

TEST_F(VisitorTestFixture, testGOPSaving1) {
    auto input = Load(videoToScan, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 1u}}).Store(videoCatalogName + "_gop1"));
}

TEST_F(VisitorTestFixture, testEncodeForMetadata) {
    // Want:
    // Scan
    // Transfer to GPU
    // GPU Encode To CPU with keyframe options
    // GPU to CPU
    // Store

    auto input = Scan("dog_with_keyframes");
    MetadataSpecification metadataSelection("LABELS", "LABEL", "dog");
    Coordinator().execute(input.Encode(metadataSelection).Store("dog_with_keyframes_real"));
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
