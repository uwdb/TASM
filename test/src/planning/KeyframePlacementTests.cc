#include "KeyframeFinder.h"
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
#include <unordered_set>

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

static std::vector<std::pair<int, int>> convertFramesToRanges(const std::vector<int> &orderedFrames) {
    std::vector<std::pair<int, int>> startEndRanges;
    auto it = orderedFrames.begin();
    auto firstValueInRange = *it++;
    auto currentValueInRange = firstValueInRange;
    while (it != orderedFrames.end()) {
        if (*it == currentValueInRange + 1) {
            ++currentValueInRange;
            ++it;
        } else {
            startEndRanges.emplace_back<std::pair<int, int>>(std::make_pair(firstValueInRange, currentValueInRange));
            firstValueInRange = *it++;
            currentValueInRange = firstValueInRange;
        }
    }
    // Get the last range.
    startEndRanges.emplace_back<std::pair<int, int>>(std::make_pair(firstValueInRange, currentValueInRange));
    return startEndRanges;
}

TEST_F(KeyframePlacementTestFixture, testEncodeVideosWithGOPs) {
    auto input = Scan("nature");
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store("nature2"));
}

TEST_F(KeyframePlacementTestFixture, testEncodeManyVideosWithGOPs) {
    std::vector<std::string> videos{
        "traffic-1k-002",
//        "traffic-2k-001",
//        "traffic-4k-002-ds2k",
//        "car-pov-2k-001-shortened",
//        "traffic-4k-000",
    };

    std::vector<unsigned int> gopLengths{30, 60, 120, 480, 1800, 5400, static_cast<unsigned int>(-1)};
    for (const auto &video : videos) {
        for (auto gopLength : gopLengths) {
            auto storedVideoName = video + "-gop_" + std::to_string(gopLength);
            std::cout << "\nEncoding " << storedVideoName << std::endl;
            auto input = Scan(video);
            Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, gopLength}}).Store(storedVideoName));
        }
    }
}

TEST_F(KeyframePlacementTestFixture, testEncodeNatureVideoWithGOPs) {
    std::string video = "nature";
    // 23 fps, 3578 frames
    std::vector<unsigned int> gopLengths{23, 46, 92, 368, 690, 1380, static_cast<unsigned int>(-1)};
    for (auto gopLength : gopLengths) {
        auto storedVideoName = video + "-gop_" + std::to_string(gopLength);
        std::cout << "\nEncoding " << storedVideoName << std::endl;
        auto input = Scan(video);
        Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, gopLength}}).Store(storedVideoName));
    }
}

TEST_F(KeyframePlacementTestFixture, testEncodeVideoWithSpecificKeyframes) {
    auto input = Scan("birdsincage");
    std::unordered_set<int> keyframes{0, 10};
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, keyframes},
                                                       {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store("birdsincage_keyframes"));
}

/* RUN QUERIES ON EVEN GOPs */
TEST_F(KeyframePlacementTestFixture, testNatureQueriesOverEvenGOPs) {
    std::string object = "bird";
    FrameMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", object));
    std::vector<unsigned int> gopLengths{23, 46, 92, 368, 690, 1380, static_cast<unsigned int>(-1)};
    for (auto gopLength : gopLengths) {
        for (int i = 0; i < 4; ++i) {
            std::cout << "\ngop-length " << gopLength << std::endl;
            std::cout << "\nselection-object " << object << std::endl;
            std::string inputVideo = "nature-gop_" + std::to_string(gopLength);
            std::cout << "\ncatalog-video " << inputVideo << std::endl;
            std::cout << "\ninput-video nature" << std::endl;

            auto input = Scan(inputVideo);
            input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
            Coordinator().execute(input.Select(selection).Sink());
        }
    }
}

/* RUN RANDOM QUERIES ON NATURE VIDEO */
TEST_F(KeyframePlacementTestFixture, testRandomQueriesOverNature) {
    std::default_random_engine generator(3);

    std::string video("nature");
    unsigned int durationOfVideoInSeconds = 155;
    unsigned int thirdOfDuration = durationOfVideoInSeconds / 3;
    unsigned int framerate = 23;
    std::vector<unsigned int> selectionDurationInSeconds{1, 5, 10, 20, 30};
    for (auto duration : selectionDurationInSeconds) {
        for (auto r = 0u; r < 3; ++r) {
            std::uniform_int_distribution<int> firstThirdDistribution(0, (thirdOfDuration - duration) * framerate);
            std::uniform_int_distribution<int> secondThirdDistribution(thirdOfDuration * framerate,
                                                                       (2 * thirdOfDuration - duration) * framerate);
            std::uniform_int_distribution<int> thirdThirdDistribution(2 * thirdOfDuration * framerate,
                                                                      (durationOfVideoInSeconds - duration) * framerate);

            auto startOfFirstRange = firstThirdDistribution(generator);
            auto startOfSecondRange = secondThirdDistribution(generator);
            auto startOfThirdRange = thirdThirdDistribution(generator);

            std::vector<int> frames;
            frames.reserve(duration * framerate * 3);
            std::vector<int> firstFrames{startOfFirstRange, startOfSecondRange, startOfThirdRange};
            for (auto firstFrame : firstFrames) {
                for (auto i = 0u; i < duration * framerate; ++i)
                    frames.push_back(firstFrame + i);
            }
            auto frameSelection = std::make_shared<FrameSpecification>(frames);

            std::string rangeStarts = std::to_string(startOfFirstRange) + "_" + std::to_string(startOfSecondRange) + "_" + std::to_string(startOfThirdRange);
            std::string customVideoName = video + "-customGOP_noopt_dur" + std::to_string(duration) + "_" + rangeStarts;
            if (!Catalog::instance().exists(customVideoName)) {
                auto startEndRanges = convertFramesToRanges(frames);
                auto numFramesInNature = 3578;
                auto maxNumberOfKeyframes = 155;
                KeyframeFinder finder(numFramesInNature, maxNumberOfKeyframes, {startEndRanges});
                auto keyframes = finder.getKeyframes(numFramesInNature, maxNumberOfKeyframes);
                std::unordered_set<int> uniqueKeyframes(keyframes.begin(), keyframes.end());

                auto input = Scan(video);
                Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, uniqueKeyframes},
                                                                   {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store(customVideoName));
            }

//            std::vector<unsigned int> gopLengths{23, 46, 92, 368, 690, 1380, static_cast<unsigned int>(-1)};
//            for (auto gopLength : gopLengths) {
                for (int i = 0; i < 3; ++i) {
                    std::cout << "\ngop-length " << rangeStarts << std::endl;
                    std::cout << "selection-duration " << duration << std::endl;
                    std::cout << "starting-frames " << startOfFirstRange << "_" << startOfSecondRange << "_" << startOfThirdRange << std::endl;
//                    std::string inputVideo = "nature-gop_" + std::to_string(gopLength);
                    std::cout << "catalog-video " << customVideoName << std::endl;
                    std::cout << "base-video nature" << std::endl;

                    auto input = Scan(customVideoName);
                    input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
                    Coordinator().execute(input.Select(frameSelection).Sink());
                }
//            }

        }
    }
}

TEST_F(KeyframePlacementTestFixture, testBirdSelectionOnIdealGOPs) {
    std::string video("nature");
    FrameMetadataSpecification selection("labels", std::make_shared<SingleMetadataElement>("label", "bird"));

    // Encode the video if it doesn't exist already.
    std::string savedName("nature-customGOP_noopt_bird");
    if (!Catalog::instance().exists(savedName)) {
        metadata::MetadataManager metadataManager(video, selection);
        auto framesForSelection = metadataManager.orderedFramesForMetadata();

        auto startEndRanges = convertFramesToRanges(framesForSelection);
        auto numFramesInNature = 3578;
        auto maxNumberOfKeyframes = 155;
        KeyframeFinder finder(numFramesInNature, maxNumberOfKeyframes, {startEndRanges});
        auto keyframes = finder.getKeyframes(numFramesInNature, maxNumberOfKeyframes);
        std::unordered_set<int> uniqueKeyframes(keyframes.begin(), keyframes.end());

        auto input = Scan(video);
        Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, uniqueKeyframes},
                                                           {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store(savedName));
    }

    // Then do selection.
    std::string object = "bird";
    for (int i = 0; i < 4; ++i) {
        std::cout << "\ngop-length " << "custom_bird" << std::endl;
        std::cout << "\nselection-object " << object << std::endl;
        std::cout << "catalog-video " << savedName << std::endl;
        std::cout << "base-video nature" << std::endl;

        auto input = Scan(savedName);
        input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
        Coordinator().execute(input.Select(selection).Sink());
    }
}

TEST_F(KeyframePlacementTestFixture, testSelectOnVisualRoadVideos) {
    std::vector<std::string> videos{
            "traffic-1k-002",
            "traffic-2k-001",
//            "traffic-4k-002-ds2k",
//            "car-pov-2k-001-shortened",
            "traffic-4k-000",
    };

    std::vector<unsigned int> gopLengths{30, 60, 120, 480, 1800, 5400, static_cast<unsigned int>(-1)};

    // For each video/gop:
    // - Select cars, then select frames with pedestrians.
    FrameMetadataSpecification carSelection("labels", std::make_shared<SingleMetadataElement>("label", "car"));
    FrameMetadataSpecification pedestrianSelection("labels", std::make_shared<SingleMetadataElement>("label", "pedestrian"));
    for (const auto &video : videos) {
        for (auto gopLength : gopLengths) {
            for (int i = 0; i < 3; ++i) {
                std::string catalogVideo = video + "-gop_" + std::to_string(gopLength);
                std::cout << "\niteration " << i << std::endl;
                std::cout << "gop-length " << gopLength << std::endl;
                std::cout << "catalog-video " << catalogVideo << std::endl;
                std::cout << "input-video " << video << std::endl;

                {
                    std::cout << "selection-object car" << std::endl;
                    auto input = Scan(catalogVideo);
                    input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
                    Coordinator().execute(input.Select(carSelection).Sink());
                }
                {
                    std::cout << "selection-object pedestrian" << std::endl;
                    auto input = Scan(catalogVideo);
                    input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
                    Coordinator().execute(input.Select(pedestrianSelection).Sink());
                }

            }
        }
    }
}

static std::string CombineIntsToString(const std::vector<int> &ints) {
    std::string output = "";
    for (auto i = 0u; i < ints.size(); ++i) {
        output += std::to_string(ints[i]);
        if (i < ints.size() - 1)
            output += "_";
    }
    return output;
}

TEST_F(KeyframePlacementTestFixture, testSelectRandomRangesOnVisualRoadVideos) {
    std::default_random_engine generator(3);

    std::vector<std::string> videos{
            "traffic-1k-002",
            "traffic-2k-001",
//            "traffic-4k-002-ds2k",
//            "car-pov-2k-001-shortened",
            "traffic-4k-000",
    };

    std::vector<unsigned int> gopLengths{30, 480, 1800, 5400, static_cast<unsigned int>(-1)};
    std::vector<unsigned int> numSelections{1, 5, 8, 10};
    std::vector<unsigned int> selectionRange{30, 60, 120};
    unsigned int framerate = 30;

    for (const auto &video : videos) {
        unsigned int numFrames = video == "car-pov-2k-000-shortened" ? 15300 : 27000;
        for (auto range : selectionRange) {
            std::cout << "*** selection-range " << range << std::endl;
            auto numFramesInSelection = range * framerate;
            std::uniform_int_distribution<int> startingFrameDistribution(0, numFrames - numFramesInSelection);
            for (auto selections : numSelections) {
                std::cout << "*** number-of-selections " << selections << std::endl;
                for (auto n = 0u; n < 3; ++n) {
                    std::cout << "*** selection-round " << n << std::endl;
                    // Generate selection selections.
                    std::vector<int> firstFrames;
                    for (auto i = 0u; i < selections; ++i)
                        firstFrames.push_back(startingFrameDistribution(generator));

                    if (video == "traffic-1k-002")
                        continue;
                    else if (video == "traffic-2k-001") {
                        if (range == 30)
                            continue;
                        else if (range == 60 and selections < 8)
                            continue;
                    }
//                    for (auto gopLength : gopLengths) {

                    std::string catalogVideo = video + "-customGOP_opt12_dur" + std::to_string(range)
                                               + "_numSel" + std::to_string(selections) + "_round_" + std::to_string(n);
                    if (!Catalog::instance().exists(catalogVideo)) {
                        std::vector<std::vector<std::pair<int, int>>> framesForAllQueries;
                        for (auto firstFrame : firstFrames) {
                            std::vector<int> framesForQuery;
                            for (auto i = 0u; i < numFramesInSelection; ++i)
                                framesForQuery.push_back(firstFrame + i);
                            framesForAllQueries.push_back(convertFramesToRanges(framesForQuery));
                        }

                        auto numFramesInVideo = 27000;
                        auto maxNumberOfKeyframes = 900;
                        Timer timer;
                        timer.startSection("get-keyframes");
                        KeyframeFinderOptOneTwo finder(numFramesInVideo, maxNumberOfKeyframes, framesForAllQueries);
                        auto keyframes = finder.getKeyframes(numFramesInVideo, maxNumberOfKeyframes);
                        timer.endSection("get-keyframes");
                        std::unordered_set<int> uniqueKeyframes(keyframes.begin(), keyframes.end());

                        std::cout << "*** Storing new video" << std::endl;
                        auto input = Scan(video);
                        timer.startSection("encode-with-custom-keyframes");
                        Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, uniqueKeyframes},
                                                                           {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store(catalogVideo));
                        timer.endSection("encode-with-custom-keyframes");
                        timer.printAllTimes();
                    }

                        // Run query for each range.
                        // Run each query set 1 time per input video.
                        for (auto j = 0u; j < 1; ++j) {
                            for (auto ff = 0u; ff < firstFrames.size(); ++ff) {
                                auto firstFrame = firstFrames[ff];
                                std::vector<int> frames;
                                frames.reserve(numFramesInSelection);
                                for (auto i = 0u; i < numFramesInSelection; ++i)
                                    frames.push_back(firstFrame + i);

                                auto frameSelection = std::make_shared<FrameSpecification>(frames);
//                                std::string catalogVideo = video + "-gop_" + std::to_string(gopLength);
//                                std::cout << "\ngop-length " << gopLength << std::endl;
                                std::cout << "starting-frame " << firstFrame << std::endl;
                                std::cout << "selection-duration " << range << std::endl;
                                std::cout << "total-num-selections " << selections << std::endl;
                                std::cout << "num-selection " << ff << std::endl;
                                std::cout << "catalog-video " << catalogVideo << std::endl;
                                std::cout << "base-video " << video << std::endl;
                                std::cout << "random-selection-round " << n << std::endl;
                                std::cout << "query-round " << j << std::endl;

                                auto input = Scan(catalogVideo);
                                input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
                                Coordinator().execute(input.Select(frameSelection).Sink());
                            }
                        }
//                    }
                }
            }
        }
    }
}

TEST_F(KeyframePlacementTestFixture, testFailure) {
//    gop-length 5400
//    catalog-video traffic-4k-000-gop_5400
//    input-video traffic-4k-000
//    selection-object car
    FrameMetadataSpecification carSelection("labels", std::make_shared<SingleMetadataElement>("label", "car"));

    auto input = Scan("traffic-4k-000-gop_5400");
    input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
    Coordinator().execute(input.Select(carSelection).Sink());
}

TEST_F(KeyframePlacementTestFixture, testRunManyRandomNonOverlappingQueries) {
    unsigned int rangeDuration = 15;
    unsigned int framerate = 23;
    auto numFramesInSelection = rangeDuration * framerate;
//    std::vector<std::vector<int>> startFrames{{884, 1294, 1931}, {1220, 1821, 2587}, {1136, 1709, 2666}, {920, 1478, 1837}, {1142, 1788, 2626}, {1012, 1855, 2526}, {1217, 1821, 3067}, {1190, 1814, 2636}, {1115, 1930, 2598}, {1143, 1525, 1931}, {1144, 1819, 2608}, {1139, 1503, 1919}, {1583, 1974, 2591}, {831, 1182, 1937}, {1149, 1551, 1911}, {908, 1562, 1949}, {1062, 1822, 2644}, {1143, 1729, 2590}, {1119, 1775, 2626}, {935, 1734, 2587}, {1155, 1516, 1922}, {1169, 1684, 2524}, {1121, 1574, 1929}, {1235, 1875, 2551}, {1456, 1812, 2648}, {1129, 1780, 2648}, {1146, 1625, 2544}, {1425, 1805, 2593}, {1154, 1965, 2561}, {1024, 1520, 1921}, {1203, 1778, 2527}, {1141, 1507, 1880}, {1125, 1537, 1900}, {972, 1552, 1897}, {169, 1757, 2620}, {1378, 1772, 2646}, {1170, 1632, 1977}, {1070, 1781, 2653}, {830, 1492, 1856}, {1196, 1541, 1937}};
//    std::vector<std::vector<int>> startFrames{{167, 1256, 1929}, {1433, 1818, 2544}, {1187, 1588, 1975}, {1131, 1764, 2554}, {1256, 1750, 2647}, {1152, 1922, 2573}, {833, 1248, 1824}, {837, 1238, 1797}, {1180, 1961, 2550}, {1112, 1774, 2603}, {922, 1395, 1972}, {830, 1386, 1793}, {174, 1215, 1981}, {1196, 1948, 2642}, {160, 1123, 1795}, {1237, 1581, 1935}, {1207, 1602, 1959}, {959, 1324, 1801}, {161, 1156, 1810}, {1126, 1541, 1962}, {1214, 1896, 2639}, {909, 1798, 2561}, {1146, 1581, 1934}, {1151, 1875, 2585}, {1232, 1774, 2678}, {1188, 1845, 2668}, {174, 1173, 1831}, {1146, 1520, 1883}, {1036, 1617, 2608}, {1118, 1663, 2522}, {1161, 1525, 1918}, {1157, 1791, 2662}, {1472, 1954, 2547}, {1156, 1505, 1960}, {1148, 1756, 2562}, {161, 1055, 1757}, {1192, 1564, 1948}, {1333, 1819, 2577}, {1192, 1809, 2541}, {1192, 1554, 1939}};
//    std::vector<std::vector<int>> startFrames{{1172, 1775, 2527}, {972, 1350, 1960}, {1029, 1717, 2592}, {1067, 1601, 1975}, {161, 1407, 1969}, {1100, 1531, 1945}, {958, 1562, 1967}, {1245, 1804, 2627}, {838, 1206, 1876}, {1169, 1759, 2569}, {1153, 1517, 1881}, {1031, 1442, 1900}, {1160, 1550, 2570}, {1162, 1576, 1950}, {881, 1426, 1826}, {1197, 1752, 2649}, {1224, 1708, 2526}, {1516, 1898, 2554}, {858, 1237, 1948}, {927, 1339, 1854}, {1211, 1618, 2680}, {1344, 1918, 2653}, {1373, 1960, 2552}, {1127, 1785, 2660}, {1270, 1732, 2606}, {1136, 1589, 1965}, {1131, 1663, 2617}, {1157, 1554, 1967}, {1007, 1780, 2616}, {873, 1221, 1957}, {1155, 1502, 1850}, {164, 1403, 1967}, {1271, 1749, 2538}, {855, 1213, 1879}, {1415, 1831, 2668}, {1124, 1473, 1837}, {1140, 1815, 2615}, {1129, 1489, 1840}, {1160, 1812, 2524}, {1215, 1890, 2549}};
//    std::vector<std::vector<int>> startFrames{{870, 1312, 1887}, {1177, 1626, 1973}, {1238, 1945, 2551}, {869, 1234, 1738}, {848, 1200, 1886}, {1210, 1813, 2531}, {1113, 1526, 1888}, {1002, 1401, 1889}, {1147, 1856, 2595}, {1204, 1734, 2570}, {1427, 1800, 2612}, {1194, 1784, 2662}, {1160, 1827, 2670}, {1123, 1494, 1947}, {1411, 1904, 2584}, {161, 1221, 1761}, {900, 1708, 2560}, {1122, 1785, 2530}, {1149, 1733, 2593}, {1194, 1795, 2605}, {167, 1127, 1829}, {1150, 1584, 1967}, {847, 1284, 1822}, {1147, 1634, 2582}, {1121, 1666, 2680}, {1179, 1775, 2573}, {1178, 1753, 2584}, {172, 1492, 1882}, {1200, 1721, 2646}, {1313, 1898, 2526}, {1174, 1554, 1976}, {1013, 1815, 2604}, {1232, 1941, 2527}, {1173, 1837, 2556}, {1071, 1448, 1799}, {1121, 1759, 2558}, {1183, 1803, 2661}, {896, 1261, 1846}, {1225, 1740, 2605}, {1478, 1856, 2603}, {1140, 1869, 2629}, {1175, 1541, 1914}, {1126, 1768, 2674}, {876, 1349, 1964}, {927, 1274, 1753}, {1180, 1546, 1939}, {1222, 1900, 2639}, {973, 1483, 1896}, {1068, 1710, 2578}, {1211, 1564, 1939}, {843, 1332, 1818}, {977, 1778, 2599}, {1354, 1748, 2634}, {174, 1277, 1844}, {1140, 1735, 2558}, {1453, 1825, 2528}, {946, 1461, 1870}, {840, 1226, 1687}, {1133, 1755, 2613}, {1128, 1676, 2673}, {862, 1403, 1747}, {1228, 1577, 2601}, {1117, 1470, 1900}, {1510, 1942, 2530}, {1221, 1778, 2616}, {896, 1807, 2600}, {829, 1183, 1748}, {1006, 1521, 1895}, {171, 955, 1599}, {1192, 1792, 2585}, {1157, 1841, 2597}, {1123, 1758, 2533}, {948, 1561, 1945}, {1124, 1931, 2553}, {1173, 1750, 2600}, {175, 1248, 1780}, {1132, 1716, 3039}, {914, 1804, 2528}, {835, 1218, 1807}, {1157, 1506, 1927}};
//    std::vector<std::vector<int>> startFrames{{878, 1233, 1738}, {1191, 1904, 2667}, {1119, 1832, 2673}, {1324, 1724, 2678}, {1162, 1752, 2603}, {1205, 1842, 2532}, {1061, 1690, 2598}, {951, 1527, 1921}, {1159, 1529, 1893}, {1148, 1741, 2586}, {1103, 1552, 2559}, {1129, 1774, 2558}, {944, 1513, 1878}, {1145, 1767, 2545}, {1246, 1680, 2521}, {1202, 1616, 1961}, {1227, 1751, 2536}, {1198, 1710, 2654}, {168, 1183, 1804}, {1221, 1833, 2523}, {168, 1580, 1957}, {1146, 1742, 2659}, {1122, 1843, 2637}, {1549, 1915, 2639}, {1080, 1537, 1908}, {1208, 1567, 1918}, {1021, 1395, 1923}, {973, 1359, 1738}, {1117, 1792, 2559}, {171, 1142, 1824}, {164, 1243, 1877}, {1122, 1493, 1912}, {1121, 1550, 1961}, {1239, 1879, 2648}, {1533, 1912, 2628}, {1116, 1512, 1944}, {1154, 1616, 1961}, {1503, 1905, 2667}, {1120, 1746, 2526}, {1181, 1631, 2527}, {859, 1225, 1683}, {857, 1733, 2575}, {1170, 1745, 2635}, {1217, 1815, 2591}, {1190, 1560, 1953}, {159, 1165, 1740}, {1465, 1918, 2526}, {964, 1527, 1882}, {962, 1397, 1802}, {1172, 1622, 1972}, {1497, 1895, 2524}, {1075, 1552, 1904}, {1204, 1803, 2588}, {1128, 1759, 3068}, {1190, 1595, 1944}, {1112, 1867, 2569}, {1183, 1700, 2677}, {945, 1688, 3039}, {856, 1262, 1730}, {173, 1498, 1885}, {1117, 1545, 1944}, {1156, 1824, 2586}, {1025, 1383, 1831}, {1180, 1528, 1872}, {159, 1275, 1899}, {170, 1163, 1800}, {1217, 1819, 2583}, {1537, 1975, 2655}, {1132, 1725, 2593}, {1132, 1483, 1834}, {871, 1454, 1813}, {1143, 1501, 1852}, {1162, 1753, 2529}, {1211, 1658, 2616}, {1012, 1484, 1874}, {1134, 1518, 1969}, {1105, 1568, 1938}, {1191, 1553, 1912}, {1215, 1732, 2570}, {1131, 1786, 2551}};
    std::vector<std::vector<int>> startFrames{{944, 1355, 1720}, {943, 1494, 1862}, {1043, 1481, 1828}, {1312, 1928, 2531}, {1191, 1657, 2558}, {1238, 1790, 2613}, {1148, 1756, 2527}, {1138, 1503, 1906}, {853, 1199, 1742}, {933, 1604, 1966}, {1189, 1889, 2600}, {1202, 1591, 1959}, {1219, 1741, 2626}, {829, 1610, 1969}, {1199, 1971, 2566}, {896, 1505, 1922}, {173, 1148, 1773}, {1133, 1641, 2579}, {1153, 1873, 2652}, {1181, 1603, 1968}, {1119, 1872, 2574}, {1228, 1925, 2634}, {1137, 1522, 1889}, {1185, 1825, 2679}, {952, 1347, 1912}, {828, 1227, 1634}, {1411, 1869, 2533}, {1133, 1838, 2600}, {170, 1195, 1888}, {961, 1497, 1908}, {1263, 1855, 2617}, {1200, 1578, 1934}, {1163, 1535, 2640}, {1183, 1723, 2656}, {961, 1367, 1945}, {1138, 1729, 2639}, {1131, 1751, 3068}, {1205, 1732, 2652}, {1230, 1948, 2548}, {1326, 1912, 2541}, {1121, 1582, 1927}, {1328, 1789, 2525}, {173, 1208, 1736}, {1237, 1828, 2544}, {1137, 1834, 2642}, {1475, 1903, 2679}, {1379, 1750, 2536}, {1127, 1627, 2674}, {1164, 1620, 2601}, {1225, 1752, 2595}, {1235, 1866, 2625}, {950, 1583, 1929}, {932, 1495, 1875}, {835, 1211, 1740}, {1249, 1917, 2546}, {1202, 1579, 1926}, {1455, 1963, 2528}, {922, 1395, 1739}, {1117, 1911, 2679}, {1206, 1727, 2549}, {1229, 1903, 2591}, {1183, 1610, 2591}, {1212, 1736, 2612}, {1219, 1621, 1968}, {827, 1258, 1749}, {168, 1157, 1810}, {1182, 1703, 2659}, {1459, 1912, 2530}, {1174, 1624, 1972}, {1159, 1818, 2570}, {1206, 1774, 2659}, {1468, 1829, 2632}, {1257, 1762, 2535}, {1040, 1743, 2613}, {1198, 1887, 2621}, {1192, 1914, 2641}, {1183, 1534, 1926}, {1203, 1579, 1980}, {1609, 1974, 2527}, {164, 1172, 1750}};
    assert(startFrames.size() == 80);

    // Convert start frames to pairs of start/end frames.
    std::vector<std::vector<std::pair<int, int>>> startEndRanges;
    startEndRanges.reserve(startFrames.size());
    for (const auto &starts : startFrames) {
        std::vector<std::pair<int, int>> queryRanges;
        queryRanges.reserve(starts.size());
        for (auto start : starts)
            queryRanges.emplace_back(std::make_pair(start, start + numFramesInSelection - 1));
        startEndRanges.push_back(queryRanges);
    }

    std::string video = "nature";
    std::string catalogVideo = "nature-customGOP-80non_overlapping_15s_queries_s33";
    if (!Catalog::instance().exists(catalogVideo)) {
        auto numFramesInNature = 3578;
        auto maxNumberOfKeyframes = 78;
        Timer timer;
        timer.startSection("get-keyframes");
        KeyframeFinderOptOneTwo finder(numFramesInNature, maxNumberOfKeyframes, startEndRanges);
        auto keyframes = finder.getKeyframes(numFramesInNature, maxNumberOfKeyframes);
        timer.endSection("get-keyframes");
        std::unordered_set<int> uniqueKeyframes(keyframes.begin(), keyframes.end());

        timer.startSection("encode-custom-video");
        auto input = Scan(video);
        Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::Keyframes, uniqueKeyframes},
                                                           {EncodeOptions::GOPSize, static_cast<unsigned int>(-1)}}).Store(catalogVideo));
        timer.endSection("encode-custom-video");
        timer.printAllTimes();
    }

    // Now, run range queries over custom video.
//    std::vector<unsigned int> gopLengths{23, 46, 368, 690, static_cast<unsigned int>(-1)};
//    for (auto gopLength : gopLengths) {
//        std::cout << "*** new-gop-length " << gopLength << std::endl;
//        std::string catalogVideo = video + "-gop_" + std::to_string(gopLength);
        for (auto r = 0u; r < startEndRanges.size(); ++r) {
            auto ranges = startEndRanges[r];
            std::vector<int> frames;
            frames.reserve(numFramesInSelection * ranges.size());
            for (const auto &range : ranges) {
                for (auto i = range.first; i <= range.second; ++i)
                    frames.push_back(i);
            }
            std::cout << "range-seed " << 33 << std::endl;
//            std::cout << "gop-length " << gopLength << std::endl;
            std::cout << "selection-duration " << rangeDuration << std::endl;
            std::cout << "total-num-selections " << startEndRanges.size() << std::endl;
            std::cout << "num-selection " << r << std::endl;
            std::cout << "catalog-video " << catalogVideo << std::endl;
            std::cout << "base-video " << video << std::endl;
//        std::cout << "query-round " << j << std::endl;

            auto frameSelection = std::make_shared<FrameSpecification>(frames);
            auto input = Scan(catalogVideo);
            input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
            Coordinator().execute(input.Select(frameSelection).Sink());
        }
//    }
}
































