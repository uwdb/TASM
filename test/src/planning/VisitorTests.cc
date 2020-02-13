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

TEST_F(VisitorTestFixture, testIngestAndCrack) {
    /* Read raw video into LightDB. */
    std::string videoName = "elfuente2_2";
    auto input = Load(
            "/home/maureen/NFLX_dataset/ElFuente2_hevc.mp4",
            Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Store(videoName));

    /* Crack entire video into uniform tiles. */
    // This is ugly, but set "UNIFORM_TILING_DIMENSION" to the number of uniform tiles you want. It currently works for 2-5.
    UNIFORM_TILING_DIMENSION = 2;
    auto outputName = videoName + std::to_string(UNIFORM_TILING_DIMENSION) + "x" + std::to_string(UNIFORM_TILING_DIMENSION);
    Coordinator().execute(Scan(videoName).StoreCracked(outputName));

    /* Crack entire video into tiles based on metadata. */

    // Specify object to crack on by specifying table name, column, and value.
    auto object = "person";
    MetadataSpecification metadataSelection("labels", "label", object);

    // In StoreCracked, specify the key to "VideoPathToLabelsPath" (in MetadataLightField.cc) that points to the relevant database of objects.
    auto databasePathKey = "elfuente2";

    // Specify the number of frames the layout should persist for. Specifying a number greater than the number of frames
    // means that the same tile layout will be used for the entire video.
    auto layoutDuration = 20000000;

    // Specify the name that the cracked video will be saved under.
    auto savedName = videoName + "-cracked-" + object + "-entire-video";

    // Specify cracking strategy.
    // GroupingExtent - One large tile around all of the objects.
    // SmallTiles - Small tiles around objects that are far enough apart.
    auto crackingStrategy = CrackingStrategy::GroupingExtent;

    // I don't remember why StoreCracked takes a pointer to metadata specification, but it does.
    Coordinator().execute(Scan(videoName).StoreCracked(savedName, databasePathKey, &metadataSelection, layoutDuration, crackingStrategy));

    /* Select pixels containing a person. */
    // Currently this results in decode + sink.
    auto firstFrame = 0u;
    auto lastFrame = 180u; // Not inclusive.
    // PixelMetadataSpecification takes the DB table, column, and value to match on.
    PixelMetadataSpecification selection("labels", "label", object, firstFrame, lastFrame);

    // Argument passed to ScanMultiTiled has to be a key in "VideoPathToLabelsPath".
    Coordinator().execute(ScanMultiTiled(savedName).Select(selection));
}

TEST_F(VisitorTestFixture, testCrackIntoTiles) {
    std::vector<std::string> videos{
        "traffic-2k-001",
        "car-pov-2k-001-shortened",
        "car-pov-2k-000-shortened",
        "traffic-4k-002-ds2k",
        "traffic-4k-002",
        "traffic-4k-000",
    };

    for (const auto &video : videos) {
        UNIFORM_TILING_DIMENSION = 6;
        auto outputName = video + "-" + std::to_string(6) + "x" + std::to_string(6);
        std::cout << "Cracking " << video << " to " << outputName << std::endl;
        auto input = Scan(video);
        Coordinator().execute(input.StoreCracked(outputName));

        UNIFORM_TILING_DIMENSION = 7;
        for (int i = 7; i <= 10; ++i) {
            UNIFORM_TILING_DIMENSION_COLUMNS = i;
            auto outputName = video + "-" + std::to_string(UNIFORM_TILING_DIMENSION) + "x" + std::to_string(UNIFORM_TILING_DIMENSION_COLUMNS);
            std::cout << "Cracking " << video << " to " << outputName << std::endl;
            auto input = Scan(video);
            Coordinator().execute(input.StoreCracked(outputName));
        }
    }
}

TEST_F(VisitorTestFixture, testScanMultiTiled) {
    auto input = ScanMultiTiled("traffic-2k-cracked-2");
    Coordinator().execute(input);
}

TEST_F(VisitorTestFixture, testScanAndSave) {
    std::vector<std::string> videos{"car-pov-2k-000-shortened.mp4", "car-pov-2k-001-shortened.mp4"};
    std::vector<std::string> videoNames{"car-pov-2k-000-shortened", "car-pov-2k-001-shortened"};

    for (auto i = 0u; i < videos.size(); ++i)
    {
        std::string &video = videos[i];
        std::string &videoName = videoNames[i];
        auto videoPath = "/home/maureen/" + video;
        auto input = Load(
                videoPath,
                Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
        Coordinator().execute(input.Store(videoName));
    }
}

TEST_F(VisitorTestFixture, testCrackBasedOnMetadata) {
    auto name = "car-pov-2k-001-shortened";
    std::vector<int> durations{20000}; //, //30, 60};
//    auto duration = 30;
    for (const auto &duration : durations) {
        auto input = Scan(name);
        MetadataSpecification metadataSelection("labels", "label", "car");
        std::string savedName = "test-crack"; // + std::to_string(duration);
        Coordinator().execute(input.StoreCracked(savedName, name, &metadataSelection, duration));
    }
}

TEST_F(VisitorTestFixture, testCrackManyBasedOnMetadata) {
    std::vector<std::string> videos{ "car-pov-2k-000-shortened", "car-pov-2k-001-shortened" };
//        "traffic-2k-001" , "traffic-1k-002",  "traffic-4k-000", "traffic-4k-002",
//                                       "traffic-4k-002-ds1k", "traffic-4k-002-ds2k", "car-pov-2k-000-shortened", "car-pov-2k-001-shortened" };
//    std::vector<std::string> videos{"birdsincage", "crowdrun", "elfuente1", "elfuente2", "oldtown", "seeking", "tennis"};
    std::unordered_map<std::string, std::vector<std::string>> videoToObjectsToCrackOn({
        {"aerial", {"car"}},
        {"busy", {"person", "car", "bus"}},
        {"childs", {"person"}},
        {"nature", {"bird"}},
        {"birdsincage", {"bird"}},
        {"crowdrun", {"person"}},
        {"elfuente1", {"person"}},
        {"elfuente2", {"person"}},
        {"oldtown", {"car"}},
        {"seeking", {"person"}},
        {"tennis", {"person", "sports ball", "tennis racket"}},
        {"elfuente_full_correctfps", {"person"}},
    });
    std::unordered_map<std::string, unsigned int> videoToFramerate({
        {"aerial", 29},
        {"busy", 23},
        {"childs", 23},
        {"nature", 23},
        {"birdsincage", 30},
        {"crowdrun", 25},
        {"elfuente1", 30},
        {"elfuente2", 30},
        {"oldtown", 25},
        {"seeking", 25},
        {"tennis", 24},
        {"elfuente_full_correctfps", 59},
    });

    auto MAX_TILE_LAYOUTS = 64;
    unsigned int baseFramerate = 30; // videoToFramerate.at(video);

    for (const auto &video : videos) {
        unsigned int numFramesInVideo = 27001;
        if (video == "car-pov-2k-000-shortened")
            numFramesInVideo = 15302;
        else if (video == "car-pov-2k-001-shortened")
            numFramesInVideo = 17102;

        int minFramesPerLayout = ceil(numFramesInVideo / MAX_TILE_LAYOUTS);
        if (minFramesPerLayout % baseFramerate)
            minFramesPerLayout = baseFramerate * (minFramesPerLayout / baseFramerate + 1);

        std::string object = "car";
//        for (const auto &object : videoToObjectsToCrackOn.at(video)) {
            MetadataSpecification metadataSpecification("labels", "label", object);
            std::vector<int> smallTileDurations{1, 2, 4};
            for (const auto &durationMultiplier : smallTileDurations) {
                int duration = baseFramerate * durationMultiplier;
                auto input = Scan(video);
                std::string savedName = video + "-cracked-" + object + "-smalltiles-duration" + std::to_string(duration);
                std::cout << "*** Cracking " << video << ", " << duration << " to " << savedName << std::endl;
                Coordinator().execute(input.StoreCracked(savedName, video, &metadataSpecification, duration,
                                                         CrackingStrategy::SmallTiles));
            }

//            std::vector<int> largeTileDurations{1};
//            for (const auto &durationMultiplier : largeTileDurations) {
//                int duration = baseFramerate * durationMultiplier;
//                auto input = Scan(video);
//                std::string savedName = video + "-cracked-" + object + "-grouping-extent-duration" + std::to_string(duration);
//                std::cout << "*** Cracking " << video << ", " << duration << " to " << savedName << std::endl;
//                Coordinator().execute(input.StoreCracked(savedName, video, &metadataSpecification, duration,
//                                                         CrackingStrategy::GroupingExtent));
//            }
//            {
//                auto input = Scan(video);
//                auto duration = 2800000;
//                std::string savedName = video + "-cracked-2-" + object + "-grouping-extent-entire-video";
//                std::cout << "*** Cracking " << video << ", grouped entire video to " << savedName << std::endl;
//                Coordinator().execute(input.StoreCracked(savedName, video, &metadataSpecification, duration,
//                                                         CrackingStrategy::GroupingExtent));
//            }
//        }
    }
}

TEST_F(VisitorTestFixture, testDecodeSmallTilesFromTrafficVideos) {
    std::vector<std::string> videos{  "car-pov-2k-000-shortened", "car-pov-2k-001-shortened" };
    auto MAX_TILE_LAYOUTS = 64;
    unsigned int baseFramerate = 30;

    std::string object = "car";
    std::vector<int> tileDurations{1, 2, 4};

    for (const auto &video : videos) {
        unsigned int numFramesInVideo = 27001;
        if (video == "car-pov-2k-000-shortened")
            numFramesInVideo = 15302;
        else if (video == "car-pov-2k-001-shortened")
            numFramesInVideo = 17102;

        int minFramesPerLayout = ceil(numFramesInVideo / MAX_TILE_LAYOUTS);
        if (minFramesPerLayout % baseFramerate)
            minFramesPerLayout = baseFramerate * (minFramesPerLayout / baseFramerate + 1);

        std::cout << "\n***object," << object << std::endl;
        PixelMetadataSpecification selection("labels", "label", object);
        for (auto durationMultiplier : tileDurations) {
            auto duration = durationMultiplier * baseFramerate;
            bool suffixIsForEntireVideo = durationMultiplier < 0;
            auto layoutDurationString = suffixIsForEntireVideo ? "entire-video" : "duration" + std::to_string(duration);
            std::string suffix = "-cracked-" + object + "-smalltiles-" + layoutDurationString;
            for (int i = 0; i < 1; ++i) {
                std::cout << "\n***video," << video << "\n***tile strategy," << suffix << std::endl;

                bool usesOnlyOneTile = suffixIsForEntireVideo;
                std::cout << "\n***usesOnlyOneTile," << (usesOnlyOneTile ? 1 : 0) << std::endl;
                auto input = ScanMultiTiled(video + suffix, usesOnlyOneTile);
                Coordinator().execute(input.Select(selection));
            }
        }
    }
}

TEST_F(VisitorTestFixture, testCrackBasedOnMetadata2) {
    auto input = Scan("traffic-1k-002");
    MetadataSpecification metadataSelection("labels", "label", "car");
    Coordinator().execute(input.StoreCracked("traffic-1k-002-cracked-grouping-extent-duration60", "traffic-1k-002", &metadataSelection));
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
//    auto numberOfRounds = 1u;
//    auto method = "ideal-tiled-dimsAlignedTo32-sortByHeight";
//    auto layoutDuration = 30;
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
    auto catalogEntry = "car-pov-2k-001-shortened";
    auto object = "car";

    PixelMetadataSpecification selection("labels", "label", object);
//    bool usesOnlyOneTile = false;
    auto input = Scan(catalogEntry);
//    input.downcast<ScannedLightField>().setWillReadEntireEntry(false); // To force scan by GOP.
    Coordinator().execute(input.Select(selection));
}

TEST_F(VisitorTestFixture, testMeasureTiles) {
//    std::vector<std::string> videos{"birdsincage", "crowdrun", "elfuente1", "elfuente2", "oldtown", "seeking", "tennis"};
    std::vector<std::string> videos {
//        "traffic-1k-002",
        "traffic-2k-001",
        "car-pov-2k-001-shortened",
        "car-pov-2k-000-shortened",
//        "traffic-4k-002-ds1k",
        "traffic-4k-002-ds2k",
        "traffic-4k-002",
        "traffic-4k-000",
    };
    std::unordered_map<std::string, std::vector<std::string>> videoToObjectsToCrackOn({
        {"aerial", {"car"}},
        {"busy", {"person", "car", "bus"}},
        {"childs", {"person"}},
        {"nature", {"bird"}},
        {"birdsincage", {"bird"}},
        {"crowdrun", {"person"}},
        {"elfuente1", {"person"}},
        {"elfuente2", {"person"}},
        {"oldtown", {"car"}},
        {"seeking", {"person"}},
        {"tennis", {"person", "sports ball", "tennis racket"}},
        {"elfuente_full", {"person" , "car"}}
    });
    std::unordered_map<std::string, unsigned int> videoToFramerate({
        {"aerial", 29},
        {"busy", 23},
        {"childs", 23},
        {"nature", 23},
        {"birdsincage", 30},
        {"crowdrun", 25},
        {"elfuente1", 30},
        {"elfuente2", 30},
        {"oldtown", 25},
        {"seeking", 25},
        {"tennis", 24},
        {"elfuente_full", 59},
    });

    std::unordered_set<std::string> usesOnlyOneTileStrategies{ "-2x2", "-3x3", "-4x4", "-5x5"};
    std::vector<std::string> uniformTileSuffixes{"-6x6", "-7x7", "-7x8", "-7x9", "-7x10"};

//    bool shouldLookAtSameCrackingAndQueryObjects = false;
    for (const auto &video : videos) {
        auto baseFramerate = 30; // videoToFramerate.at(video);
//        const auto &objectsForVideo = videoToObjectsToCrackOn.at(video);
        std::vector<std::string> objectsForVideo{ "car" };
//        if (!shouldLookAtSameCrackingAndQueryObjects && objectsForVideo.size() == 1)
//            continue;
        for (const auto &queryObject : objectsForVideo) {
            std::cout << "\n***object," << queryObject << std::endl;
            PixelMetadataSpecification selection("labels", "label", queryObject);

            auto runQuery = [&](const std::string &video, const std::string &suffix, bool usesOnlyOneTile) {
                for (int i = 0; i < 3; ++i) {
                    std::cout << "\n***video," << video << "\n***tile strategy," << suffix << std::endl;

                    auto catalogEntry = video + suffix;
                    std::cout << "***usesOnlyOneTile," << usesOnlyOneTile << std::endl;
                    auto input = ScanMultiTiled(catalogEntry, usesOnlyOneTile);
                    Coordinator().execute(input.Select(selection));
                }
            };

            auto setUpQuery = [&](const std::string &video, const std::string &crackingObject, std::string suffixSpecifier, int baseFramerate, int layoutDurationMultiplier) {
                auto layoutDuration = baseFramerate * layoutDurationMultiplier;
                bool suffixIsForEntireVideo = layoutDurationMultiplier < 0;
                auto layoutDurationString = suffixIsForEntireVideo ? "entire-video" : "duration" + std::to_string(
                        layoutDuration);
                std::string suffix = "-cracked-" + crackingObject + suffixSpecifier + layoutDurationString;
                bool usesOnlyOneTile = usesOnlyOneTileStrategies.count(suffix) || suffixIsForEntireVideo; // || (shouldLookAtSameCrackingAndQueryObjects && suffixIsForEntireVideo);
                runQuery(video, suffix, usesOnlyOneTile);
            };

            for (const auto &suffix : uniformTileSuffixes) {
                bool usesOnlyOneTile = true;
                runQuery(video, suffix, usesOnlyOneTile);
            }

//            std::vector<std::string> crackingObjects{"person"};
//            for (const auto &crackingObject : crackingObjects) {
////                if (shouldLookAtSameCrackingAndQueryObjects && crackingObject != queryObject)
////                    continue;
////                else if (!shouldLookAtSameCrackingAndQueryObjects && crackingObject == queryObject)
////                    continue;
//
////                std::vector<int> smallLayoutDurationMultipliers{1, 2, 3};
//                std::vector<int> largeLayoutDurationMultipliers{1, 2, -1};
////                for (const auto &layoutDurationMultiplier : smallLayoutDurationMultipliers) {
////                    setUpQuery(video, crackingObject, "-smalltiles-", baseFramerate, layoutDurationMultiplier);
////                }
////                for (const auto &layoutDurationMultiplier : largeLayoutDurationMultipliers) {
////                    setUpQuery(video, crackingObject, "-grouping-extent-", baseFramerate, layoutDurationMultiplier);
////                }
//                for (const auto &suffix : uniformTileSuffixes) {
//                    bool usesOnlyOneTile = true;
//                    runQuery(video, suffix, usesOnlyOneTile);
//                }
//            }
//            for (int i = 0; i < 2; ++i) {
//                std::cout << "\n***video," << video << "\n***tile strategy,none" << std::endl;
//                auto input = Scan(video);
//                input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
//                Coordinator().execute(input.Select(selection));
//            }
        }
    }
}

TEST_F(VisitorTestFixture, testTrackVehicle) {
    auto video = "traffic-2k-001";
    auto object = "car";
    std::cout << "\n***object," << object << std::endl;
    std::vector<int> trackingIds{0, 2, 8, 10, 101, 111, 116, 108, 113, 1000, 1001, 1002, 1041};
    std::vector<std::string> tileSuffixes{"-3x3",
                                         "-cracked-grouping-extent-entire-video", "-cracked-grouping-extent-duration30", "-cracked-grouping-extent-duration60",
                                         "-cracked-smalltiles-duration30", "-cracked-smalltiles-duration60", "-cracked-smalltiles-duration120"};
    std::unordered_set<std::string> usesOnlyOneTileStrategies{"-3x3", "-cracked-grouping-extent-entire-video"};

    for (const auto &trackingId : trackingIds) {
        PixelMetadataSpecification selection("labels", "tracking_id", trackingId);
        std::cout << "\n***tracking_id," << trackingId << std::endl;

        for (int i = 0; i < 1; ++i) {
            std::cout << "\n***video,"<<video << "\n***tile strategy,none" << std::endl;
            auto input = Scan(video);
            input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
            Coordinator().execute(input.Select(selection));
        }

        for (const auto& suffix : tileSuffixes) {
            for (int i = 0; i < 1; ++i) {
                std::cout << "\n***video," << video << "\n***tile strategy," << suffix << std::endl;

                auto catalogEntry = video + suffix;
                bool usesOnlyOneTile = usesOnlyOneTileStrategies.count(suffix);
                std::cout << "***usesOnlyOneTile," << usesOnlyOneTile << std::endl;
                auto input = ScanMultiTiled(catalogEntry, usesOnlyOneTile);
                Coordinator().execute(input.Select(selection));
            }
        }
    }
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
    auto input = Scan("traffic-2k-001");
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
