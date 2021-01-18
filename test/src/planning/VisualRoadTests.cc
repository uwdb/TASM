#include "HeuristicOptimizer.h"
#include "Metadata.h"
#include "extension.h"
#include "TileUtilities.h"

#include <cstdio>
#include <gtest/gtest.h>

#include "StatsCollector.h"
#include "WorkloadUtilities.h"
#include "VideoStats.h"

using namespace lightdb;
using namespace lightdb::logical;
using namespace lightdb::optimization;
using namespace lightdb::catalog;
using namespace lightdb::execution;

class VisualRoadTestFixture : public testing::Test {
public:
    VisualRoadTestFixture()
            : catalog("resources") {
        Catalog::instance(catalog);
        Optimizer::instance<HeuristicOptimizer>(LocalEnvironment());
    }

protected:
    Catalog catalog;
};

void DeleteDatabase(std::string id) {
    std::vector<std::string> extensions{ ".db", ".db-wal", ".db-shm" };
    for (auto &ext : extensions) {
        std::string dbPath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/" + id + ext;
        remove(dbPath.c_str());
    }
}

const unsigned int QUERY_DURATION = 60;
const unsigned int NUM_QUERIES = 100;
const unsigned int SEED = 5;
double REGRET_THRESHOLD = 1.0;
std::vector<std::vector<std::string>> QUERY_OBJECTS{
        {"car", "truck", "bus", "motorbike"},
        {"person"},
};
std::vector<std::string> ALL_OBJECTS{"car", "truck", "bus", "motorbike", "person"};
std::vector<std::string> VIDEOS{
    "traffic-2k-001",
    "traffic-4k-002-ds2k",
    "car-pov-2k-000-shortened",
    "car-pov-2k-001-shortened",
    "traffic-4k-000",
    "traffic-4k-002"
};
// TODO: Double check these.
std::unordered_map<std::string, unsigned int> VideoToMaxTile {
    {"traffic-2k-001", 900},
    {"traffic-4k-002-ds2k", 900},
    {"car-pov-2k-000-shortened", 510},
    {"car-pov-2k-001-shortened", 570},
    {"traffic-4k-000", 900},
    {"traffic-4k-002", 900},
};

std::string TiledName(const std::string &video) {
    return video + "-vr";
}

template <typename MetadataSpecificationType>
std::unique_ptr<utility::WorkloadGenerator<MetadataSpecificationType>> GetGenerator(const std::string &video, std::default_random_engine *generator) {
    return std::make_unique<utility::UniformRandomObjectWorkloadGenerator<MetadataSpecificationType>>(
            video,
            QUERY_OBJECTS,
            QUERY_DURATION,
            generator,
            std::make_unique<utility::UniformFrameGenerator>(video, QUERY_DURATION, VideoToNumFrames.at(video), VideoToFramerate.at(video)),
//            std::make_unique<utility::UniformFrameGenerator>(video, QUERY_DURATION, 300, VideoToFramerate.at(video)),
            VideoToFramerate.at(video));
}

TEST_F(VisualRoadTestFixture, testSetUpVideosForTest) {
    for (auto &video : VIDEOS) {
        Coordinator().execute(
            Scan(
                video
            ).PrepareForCracking(
                TiledName(video),
                VideoToFramerate.at(video),
                {{RetileOptions::SplitByGOP, true}}
            )
        );
    }
}

void AddQueryStats(std::shared_ptr<MetadataSpecification> selection, const std::string &object) {
    StatsCollector::instance().addStat("query-object", object);
    StatsCollector::instance().addStat("query-duration", QUERY_DURATION);
    StatsCollector::instance().addStat("first-frame", selection->firstFrame());
    StatsCollector::instance().addStat("last-frame", selection->lastFrame());
}

std::filesystem::path StatsFile(const std::filesystem::path &file) {
    std::filesystem::path outPath("/home/maureen/results/visualroad_experiments");
    return outPath / file;
//    auto csv = StatsCollector::instance().toCSV();
//    stats.write(csv.data(), csv.length());
}

std::string OutputVideoName(const std::string &label, unsigned int first, unsigned int last, bool stitched) {
    return "/home/maureen/masked_videos/" + label + "_" + (stitched ? "stitched" : "untiled") + "_" + std::to_string(first) + "_" + std::to_string(last) + ".mp4";
}

void SetUpOutFile(std::filesystem::path path) {
    if (std::filesystem::exists(path))
        std::filesystem::rename(path, path.string() + ".orig");
}

TEST_F(VisualRoadTestFixture, testRunWorkloadWithNoTiling) {
    auto outPath = StatsFile("UniformWorkloadWithNoTiling.csv");
    SetUpOutFile(outPath);
    std::ofstream out(outPath);
    for (const auto &video : VIDEOS) {
        auto yolo = lightdb::extensibility::Load("yologpu");
        auto videoId = TiledName(video);

        // Delete the past object detections.
//        DeleteDatabase(videoId);

        // Delete past tilings of this video.
        DeleteTilesPastNum(videoId, VideoToMaxTile.at(video));
        ResetTileNum(videoId, VideoToMaxTile.at(video));

        // Get workload generator.
        std::default_random_engine generator(SEED);
        auto queryGenerator = GetGenerator<PixelsInFrameMetadataSpecification>(video, &generator);

        for (auto i = 0u; i < NUM_QUERIES; ++i) {
            std::string object;
            auto selection = queryGenerator->getNextQuery(i, &object);

            std::cout << "Video " << video << std::endl;
            std::cout << "Query-object " << object << std::endl;
            std::cout << "Iteration " << i << std::endl;
            std::cout << "First-frame " << selection->firstFrame() << std::endl;
            std::cout << "Last-frame " << selection->lastFrame() << std::endl;

            StatsCollector::instance().setUpNewQuery(video, i, "untiled", "select");
            AddQueryStats(selection, object);
            Coordinator().execute(
                    ScanMultiTiled(
                            videoId
                    ).Select(
                            *selection, yolo, {{MetadataOptions::MetadataIdentifier, videoId}, {ScanOptions::ReadAllFrames, true}, {ScanOptions::IsReadingUntiledVideo, true}}
                    ).Save(
                            OutputVideoName(object, selection->firstFrame(), selection->lastFrame(), false)
                    )
            );
            auto csv = StatsCollector::instance().toCSV();
            out.write(csv.data(), csv.length());
            out.flush();
        }
    }
}

TEST_F(VisualRoadTestFixture, testRunWorkloadWithTiling) {
    auto outPath = StatsFile("UniformWorkloadWithTiling.csv");
    SetUpOutFile(outPath);
    std::ofstream out(outPath);
    for (const auto &video : VIDEOS) {
        auto yolo = lightdb::extensibility::Load("yologpu");
        auto videoId = TiledName(video);
        // Delete the past object detections.
        DeleteDatabase(videoId);

        // Delete past tilings of this video.
        DeleteTilesPastNum(videoId, VideoToMaxTile.at(video));
        ResetTileNum(videoId, VideoToMaxTile.at(video));

        // Get workload generator.
        std::default_random_engine generator(SEED);
        auto queryGenerator = GetGenerator<PixelsInFrameMetadataSpecification>(video, &generator);

        auto videoDimensions = VideoToDimensions.at(video);
        bool readEntireGOPs = true;
        auto regretAccumulator = std::make_shared<TASMRegretAccumulator>(
                videoId,
                videoDimensions.first, videoDimensions.second, VideoToFramerate.at(video),
                REGRET_THRESHOLD,
                readEntireGOPs);
        for (auto i = 0u; i < NUM_QUERIES; ++i) {
            std::string object;
            auto selection = queryGenerator->getNextQuery(i, &object);

            std::cout << "Video " << video << std::endl;
            std::cout << "Query-object " << object << std::endl;
            std::cout << "Iteration " << i << std::endl;
            std::cout << "First-frame " << selection->firstFrame() << std::endl;
            std::cout << "Last-frame " << selection->lastFrame() << std::endl;

            StatsCollector::instance().setUpNewQuery(video, i, "tiling", "select");
            AddQueryStats(selection, object);
            Coordinator().execute(
                ScanMultiTiled(
                    videoId
                ).Select(
                   *selection, yolo, {{MetadataOptions::MetadataIdentifier, videoId}}
                ).Save(
                    OutputVideoName(object, selection->firstFrame(), selection->lastFrame(), true)
                )
            );

            StatsCollector::instance().setQueryComponent("crack");
            Coordinator().execute(
                    ScanAndRetile(
                            videoId,
                            video,
                            *selection,
                            VideoToFramerate.at(video),
                            CrackingStrategy::SmallTiles,
                            RetileStrategy::RetileBasedOnRegret,
                            regretAccumulator,
                            {},
                            {{RetileOptions::SplitByGOP, true}}
                            )
                    );
            auto csv = StatsCollector::instance().toCSV();
            out.write(csv.data(), csv.length());
            out.flush();
        }
    }
}

TEST_F(VisualRoadTestFixture, testRunWorkloadWithDetectionAndTilingBefore) {
    auto outPath = StatsFile("UniformWorkloadWithTilingBefore.csv");
    SetUpOutFile(outPath);
    std::ofstream out(outPath);
    for (const auto &video : VIDEOS) {
        auto yolo = lightdb::extensibility::Load("yologpu");
        auto videoId = TiledName(video);
        // Delete the past object detections.
        DeleteDatabase(videoId);

        // Delete past tilings of this video.
        DeleteTilesPastNum(videoId, VideoToMaxTile.at(video));
        ResetTileNum(videoId, VideoToMaxTile.at(video));

        {
            StatsCollector::instance().setUpNewQuery(video, 0, "pre-tiling", "detect");
            FrameMetadataSpecification selection(std::make_shared<EntireFrameMetadataElement>(0, VideoToNumFrames.at(video)));
            Coordinator().execute(ScanMultiTiled(videoId).Select(selection, {{MetadataOptions::MetadataIdentifier, videoId}}).Map(yolo));
            auto csv = StatsCollector::instance().toCSV();
            out.write(csv.data(), csv.length());
            out.flush();
        }

        {
            StatsCollector::instance().setQueryComponent("pre-tile");
            MetadataSpecification allElements(std::make_shared<AllMetadataElement>());
            Coordinator().execute(ScanAndRetile(
                    videoId,
                    video,
                    allElements,
                    VideoToFramerate.at(video),
                    CrackingStrategy::SmallTiles,
                    RetileStrategy::RetileIfDifferent,
                    {}, {},
                    {{RetileOptions::SplitByGOP, true}}));
            auto csv = StatsCollector::instance().toCSV();
            out.write(csv.data(), csv.length());
            out.flush();
        }



        // Get workload generator.
        std::default_random_engine generator(SEED);
        auto queryGenerator = GetGenerator<PixelsInFrameMetadataSpecification>(video, &generator);

        auto videoDimensions = VideoToDimensions.at(video);
        for (auto i = 0u; i < NUM_QUERIES; ++i) {
            std::string object;
            auto selection = queryGenerator->getNextQuery(i, &object);

            std::cout << "Video " << video << std::endl;
            std::cout << "Query-object " << object << std::endl;
            std::cout << "Iteration " << i << std::endl;
            std::cout << "First-frame " << selection->firstFrame() << std::endl;
            std::cout << "Last-frame " << selection->lastFrame() << std::endl;

            StatsCollector::instance().setUpNewQuery(video, i, "tiling", "select");
            AddQueryStats(selection, object);
            Coordinator().execute(
                    ScanMultiTiled(
                            videoId
                    ).Select(
                            *selection, yolo, {{MetadataOptions::MetadataIdentifier, videoId}}
                    ).Save(
                            OutputVideoName(object, selection->firstFrame(), selection->lastFrame(), true)
                    )
            );
            auto csv = StatsCollector::instance().toCSV();
            out.write(csv.data(), csv.length());
            out.flush();
        }
    }
}

TEST_F(VisualRoadTestFixture, testRunWorkloadWithTilingAfter) {
    auto outPath = StatsFile("UniformWorkloadWithTilingAfter.csv");
    SetUpOutFile(outPath);
    std::ofstream out(outPath);
    for (const auto &video : VIDEOS) {
        auto yolo = lightdb::extensibility::Load("yologpu");
        auto videoId = TiledName(video);
        // Delete the past object detections.
        DeleteDatabase(videoId);

        // Delete past tilings of this video.
        DeleteTilesPastNum(videoId, VideoToMaxTile.at(video));
        ResetTileNum(videoId, VideoToMaxTile.at(video));

        // Get workload generator.
        std::default_random_engine generator(SEED);
        auto queryGenerator = GetGenerator<PixelsInFrameMetadataSpecification>(video, &generator);

        auto videoDimensions = VideoToDimensions.at(video);
        bool readEntireGOPs = true;
//        auto regretAccumulator = std::make_shared<TASMRegretAccumulator>(
//                videoId,
//                videoDimensions.first, videoDimensions.second, VideoToFramerate.at(video),
//                REGRET_THRESHOLD,
//                readEntireGOPs);

        for (auto i = 0u; i < NUM_QUERIES; ++i) {
            std::string object;
            auto selection = queryGenerator->getNextQuery(i, &object);

            std::cout << "Video " << video << std::endl;
            std::cout << "Query-object " << object << std::endl;
            std::cout << "Iteration " << i << std::endl;
            std::cout << "First-frame " << selection->firstFrame() << std::endl;
            std::cout << "Last-frame " << selection->lastFrame() << std::endl;

            StatsCollector::instance().setUpNewQuery(video, i, "tiling", "select");
            AddQueryStats(selection, object);
            Coordinator().execute(
                    ScanMultiTiled(
                            videoId
                    ).Select(
                            *selection, yolo, {{MetadataOptions::MetadataIdentifier, videoId}}
                    ).Save(
                            OutputVideoName(object, selection->firstFrame(), selection->lastFrame(), true)
                    )
            );

            StatsCollector::instance().setQueryComponent("crack");
            PixelMetadataSpecification allObjectsTiling(lightdb::logical::TASMRegretAccumulator::MetadataElementForObjects(ALL_OBJECTS, selection->firstFrame()), selection->lastFrame());
            Coordinator().execute(
                    ScanAndRetile(
                            videoId,
                            video,
                            allObjectsTiling,
                            VideoToFramerate.at(video),
                            CrackingStrategy::SmallTiles,
                            RetileStrategy::RetileIfDifferent,
                            {},
                            {},
                            {{RetileOptions::SplitByGOP, true}}
                    )
            );
            auto csv = StatsCollector::instance().toCSV();
            out.write(csv.data(), csv.length());
            out.flush();
        }
    }
}


TEST_F(VisualRoadTestFixture, testDebug) {
    std::string video("traffic-2k-001");
    auto videoId = TiledName(video);
    auto first = 17640;
    auto last = 17640+30;
    auto framerate = 30u;
    auto yolo = lightdb::extensibility::Load("yologpu");

    PixelsInFrameMetadataSpecification personSelection(
            std::make_shared<SingleMetadataElement>("label", "person", first, last));
    {
        DeleteTilesPastNum(videoId, VideoToMaxTile.at(video));
        ResetTileNum(videoId, VideoToMaxTile.at(video));

        auto retileOp = ScanAndRetile(
                videoId,
                video,
                personSelection,
                framerate,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileIfDifferent,
                {}, {},
                {{RetileOptions::SplitByGOP, true}});
        Coordinator().execute(retileOp);

        StatsCollector::instance().setUpNewQuery("people-t-people", 0, "people-t-people", "select");
        Coordinator().execute(ScanMultiTiled(videoId).Select(personSelection, yolo, {{MetadataOptions::MetadataIdentifier, videoId}}).Save("/home/maureen/masked_videos/tile_around_people.mp4"));
    }

    {
        PixelsInFrameMetadataSpecification selection(
                lightdb::logical::TASMRegretAccumulator::MetadataElementForObjects({"car", "truck", "bus", "motorbike"}, first, last));
        DeleteTilesPastNum(videoId, VideoToMaxTile.at(video));
        ResetTileNum(videoId, VideoToMaxTile.at(video));

        auto retileOp = ScanAndRetile(
                videoId,
                video,
                selection,
                framerate,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileIfDifferent,
                {}, {},
                {{RetileOptions::SplitByGOP, true}});
        Coordinator().execute(retileOp);

        {
            StatsCollector::instance().setUpNewQuery("car-t-car", 0, "car-t-car", "select");
            Coordinator().execute(ScanMultiTiled(videoId).Select(selection, yolo,
                                                                 {{MetadataOptions::MetadataIdentifier, videoId}}).Save(
                    "/home/maureen/masked_videos/tile_around_cars.mp4"));
        }
        {
            StatsCollector::instance().setUpNewQuery("people-t-car", 0, "people-t-car", "select");
            Coordinator().execute(ScanMultiTiled(videoId).Select(personSelection, yolo,
                                                                 {{MetadataOptions::MetadataIdentifier, videoId}}).Save(
                    "/home/maureen/masked_videos/tile_around_cars_select_people.mp4"));
        }
    }
//    WriteStatsToFile("debug.csv");
}








TEST_F(VisualRoadTestFixture, testReencodeBlackTile) {
    std::string blackTilePath("/home/maureen/black_tiles/30_f/320_w/256_h/t_320_256.hevc");
    auto input = Load(blackTilePath, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(input.Encode(Codec::hevc(), {{EncodeOptions::GOPSize, 30u}}).Save("/home/maureen/masked_videos/t_320_256_reencoded.hevc"));
}

TEST_F(VisualRoadTestFixture, testCreateBlackTiles) {
    unsigned int minWidth = 256;
    unsigned int maxWidth = 3840;
    unsigned int minHeight = 160;
    unsigned int maxHeight = 1984;
    unsigned int alignment = 32;
    unsigned int numFrames = 30;
    std::filesystem::path base("/home/maureen/lightdb_created_black_tiles");
    Codec codec = Codec::hevc();

    for (auto w = minWidth; w <= maxWidth; w += alignment) {
        for (auto h = minHeight; h <= maxHeight; h += alignment) {
            std::cout << "Creating " << w << " x " << h << " black tile" << std::endl;
            auto outputPath = catalog::BlackTileFiles::pathForTile(base, numFrames, w, h);
            std::filesystem::create_directories(outputPath.parent_path());
            Coordinator().execute(CreateBlackTile(codec, w, h, numFrames).Encode(codec, {{EncodeOptions::GOPSize, numFrames}}).Save(outputPath));
        }
    }

    // Also create tiles for un-tiled 2k and 4k videos.
    std::vector<std::pair<unsigned int, unsigned int>> dimensions{
            {1920, 1080},
            {3840, 1980},
    };
    for (auto widthAndHeight : dimensions) {
        auto w = widthAndHeight.first;
        auto h = widthAndHeight.second;
        std::cout << "Creating " << w << " x " << h << " black tile" << std::endl;
        auto outputPath = catalog::BlackTileFiles::pathForTile(base, numFrames, w, h);
        std::filesystem::create_directories(outputPath.parent_path());
        Coordinator().execute(CreateBlackTile(codec, w, h, numFrames).Encode(codec, {{EncodeOptions::GOPSize, numFrames}}).Save(outputPath));
    }
}

TEST_F(VisualRoadTestFixture, testSetUpVideoForTiling) {
//    std::string videoPath("/home/maureen/masked_videos/cropped_traffic-4k-002-ds2k.mp4");
    std::string storedName("traffic-4k-002-ds2k-not-tiled-cracked");
    unsigned int framerate = 30;
//    auto input = Load(videoPath, Volume::limits(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()));
    Coordinator().execute(Scan("traffic-4k-002-ds2k").PrepareForCracking(storedName, framerate, {{RetileOptions::SplitByGOP, true}}));
}

TEST_F(VisualRoadTestFixture, testDetectRunYolo) {
    std::string videoId("traffic-4k-002-ds2k-not-tiled-cracked");
    DeleteDatabase(videoId);

    auto input = ScanMultiTiled(videoId);

    PixelsInFrameMetadataSpecification selection("labels",
            std::make_shared<SingleMetadataElement>("label", "person", 30, 120));
    auto yolo = lightdb::extensibility::Load("yologpu");

    // The first time, detection should run on all frames.
    Coordinator().execute(input.Select(selection, yolo));

    // The second time, detection shouldn't run on any frames.
    Coordinator().execute(input.Select(selection, yolo));

    // With this partially overlapping selection, 90 frames should be decoded, and YOLO should only run on 60 of them.
    PixelsInFrameMetadataSpecification partiallyOverlappingSelection("labels",
                                                 std::make_shared<SingleMetadataElement>("label", "person", 90, 180));
    Coordinator().execute(input.Select(partiallyOverlappingSelection, yolo));
}

TEST_F(VisualRoadTestFixture, testDetectOnGPU) {
    std::string video("traffic-4k-002-ds2k");
    auto input = Scan(video);

    auto yolo = lightdb::extensibility::Load("yologpu");
    auto mapped = input.Map(yolo);
    Coordinator().execute(mapped);
}

TEST_F(VisualRoadTestFixture, testDetectAndMaskUntiled) {
    std::string video("traffic-4k-002-ds2k");
    auto input = Scan(video);
    input.downcast<ScannedLightField>().setWillReadEntireEntry(false);
    auto yolo = lightdb::extensibility::Load("yologpu");
    PixelsInFrameMetadataSpecification selection("labels",
                                                 std::make_shared<SingleMetadataElement>("label", "person", 30, 120));
    std::string metadataId("traffic-4k-002-ds2k-yolo");
    Coordinator().execute(input.Select(selection, yolo, {{MetadataOptions::MetadataIdentifier, metadataId}}).Save("/home/maureen/masked_videos/untiled.mp4"));
}

TEST_F(VisualRoadTestFixture, testDetectAndMask) {
    std::string videoId("traffic-4k-002-ds2k-not-tiled-cracked");
    DeleteDatabase(videoId);

    auto input = ScanMultiTiled(videoId);

    PixelsInFrameMetadataSpecification selection("labels",
                                                 std::make_shared<SingleMetadataElement>("label", "person", 30, 120));
    auto yolo = lightdb::extensibility::Load("yologpu");
    Coordinator().execute(input.Select(selection, yolo).Store("traffic-4k-002-ds2k-masked"));
}

TEST_F(VisualRoadTestFixture, testCreateTilesAroundPeople) {
    std::string videoId("traffic-4k-002-ds2k-not-tiled-cracked");
    auto yolo = lightdb::extensibility::Load("yologpu");

//    DeleteDatabase(videoId);
//    DeleteTiles(videoId);
//    ResetTileNum(videoId);
//
//    // First, detect objects.
//    auto input = ScanMultiTiled(videoId);
//    FrameMetadataSpecification selection(std::make_shared<EntireFrameMetadataElement>(30, 120));
//    Coordinator().execute(input.Select(selection).Map(yolo));
//
    // Then, re-tile around people.
    PixelsInFrameMetadataSpecification personSelection(std::make_shared<SingleMetadataElement>("label", "person", 30, 120));
//    auto framerate = 30u;
//    auto retileOp = ScanAndRetile(
//            videoId,
//            "traffic-4k-002-ds2k",
//            personSelection,
//            framerate,
//            CrackingStrategy::GroupingExtent);
//    Coordinator().execute(retileOp);

    // Then, do selection on people tiles.
    // Re-do scan so that new tiles are discovered.
    Coordinator().execute(ScanMultiTiled(videoId).Select(personSelection, yolo).Store("traffic-4k-002-ds2k-tiled-masked"));
}

TEST_F(VisualRoadTestFixture, testDetectAndMaskAroundPeople) {
    const int MAX_TILES = 900;
    std::string videoId("traffic-4k-002-ds2k-vr");
    std::string metadataId("traffic-4k-002-ds2k-not-tiled-new");
//    std::string videoId("traffic-4k-002-ds2k-0_1800-3x3");
    auto yolo = lightdb::extensibility::Load("yologpu");

//    DeleteDatabase(metadataId);
//    DeleteTiles(videoId, MAX_TILES);
//    ResetTileNum(videoId, );

    // First, detect objects.
//    auto input = ScanMultiTiled(videoId);
//    FrameMetadataSpecification selection(std::make_shared<EntireFrameMetadataElement>(0, 120));
//    Coordinator().execute(input.Select(selection).Map(yolo));

//    PixelsInFrameMetadataSpecification selection(std::make_shared<SingleMetadataElement>("label", "person", 0, 120));
//    Coordinator().execute(ScanMultiTiled(videoId).Select(selection, yolo, {{MetadataOptions::MetadataIdentifier, metadataId}, {ScanOptions::ReadAllFrames, true}, {ScanOptions::IsReadingUntiledVideo, true}}).Save("/home/maureen/masked_videos/test_256.mp4"));

    std::vector<std::pair<int, int>> frames {
//            {0, 30},
//            {0, 60},
//            {0, 60}
            {1800*0, 1800*1},
//            {1800*1, 1800*2},
//            {1800*2, 1800*3},
//            {1800*3, 1800*4},
//            {1800*4, 1800*5},
//            {1800*5, 1800*6},
//            {1800*6, 1800*7},
//            {1800*7, 1800*8},
//            {1800*8, 1800*9},
//            {1800*9, 1800*10},
//            {1800*10, 1800*11},
//            {1800*11, 1800*12},
//            {1800*12, 1800*13},
//            {1800*13, 1800*14},
//            {1800*13, 1800*15},
    };
//    DeleteDatabase(metadataId);
    for (auto firstLast : frames) {
        DeleteTilesPastNum(videoId, MAX_TILES);
        ResetTileNum(videoId, MAX_TILES);
//
        auto first = firstLast.first;
        auto last = firstLast.second;
        PixelsInFrameMetadataSpecification selection(
                std::make_shared<SingleMetadataElement>("label", "car", first, last));
        std::filesystem::path outputPath;
        unsigned int qid = 0;

//        StatsCollector::instance().setUpNewQuery(qid, "untiled", "select");
//        outputPath = "/home/maureen/masked_videos/traffic_car_boxed_untiled_" + std::to_string(first) + "_" + std::to_string(last) + ".mp4";
//        std::cout << "Saving to " << outputPath << std::endl;
//        Coordinator().execute(ScanMultiTiled(videoId).Select(selection, yolo).Save(outputPath));
//
//        StatsCollector::instance().setUpNewQuery(videoId, 0, "untiled_optimized", "select");
//        outputPath = "/home/maureen/masked_videos/traffic_car_boxed_untiled_optimized_" + std::to_string(first) + "_" + std::to_string(last) + ".mp4";
//        std::cout << "Saving to " << outputPath << std::endl;
//        Coordinator().execute(ScanMultiTiled(videoId).Select(
//                selection, yolo, {{ScanOptions::ReadAllFrames, true}, {ScanOptions::IsReadingUntiledVideo, true}}
//                ).Save(outputPath));

//        Coordinator().execute(ScanMultiTiled(videoId).Select(selection, yolo, {{MetadataOptions::MetadataIdentifier, metadataId}}));

        // Then, re-tile around cars.
//        StatsCollector::instance().setUpNewQuery(videoId, qid, "tiled", "crack");
//        auto framerate = 30u;
//        auto retileOp = ScanAndRetile(
//                videoId,
//                "traffic-4k-002-ds2k",
//                selection,
//                framerate,
//                CrackingStrategy::SmallTiles,
//                RetileStrategy::RetileIfDifferent,
//                {}, {},
//                {{RetileOptions::SplitByGOP, true}});
//        Coordinator().execute(retileOp);

        // Then, do selection on people tiles.
        // Re-do scan so that new tiles are discovered.
        StatsCollector::instance().setQueryComponent("select");
        outputPath = "/home/maureen/masked_videos/traffic_car_boxed_stitched_" + std::to_string(first) + "_" + std::to_string(last) + ".mp4";
        std::cout << "Saving to " << outputPath << std::endl;
        Coordinator().execute(ScanMultiTiled(videoId).Select(selection, yolo, {{MetadataOptions::MetadataIdentifier, metadataId}}).Save(outputPath));

//        std::ofstream stats("/home/maureen/results/visualroad_experiments/test.csv");
//        auto csv = StatsCollector::instance().toCSV();
//        stats.write(csv.data(), csv.length());
    }
}

TEST_F(VisualRoadTestFixture, testLoadPrefix) {
    Coordinator().execute(Load("/home/maureen/masked_videos/traffic-4k-002-ds2k-prefix.mp4", Volume::zero(), GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples())).PrepareForCracking("prefix-retiled", 30));
}

TEST_F(VisualRoadTestFixture, testTilePrefix) {
    Coordinator().execute(
        Load(
            "/home/maureen/masked_videos/traffic-4k-002-ds2k-0_1800.mp4",
            Volume::zero(),
            GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples())
        ).StoreCrackedUniform(
            "traffic-4k-002-ds2k-0_1800-3x3",
            3, 3,
            {{RetileOptions::SplitByGOP, true}}
        )
    );
}
