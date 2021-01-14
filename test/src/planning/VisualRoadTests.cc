#include "HeuristicOptimizer.h"
#include "Metadata.h"
#include "extension.h"
#include "TileUtilities.h"

#include <cstdio>
#include <gtest/gtest.h>

#include "StatsCollector.h"

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
    // Transform metadataIdentifier.
    auto crackedPos = id.find("-cracked");
    if (crackedPos != std::string::npos) {
        id = id.substr(0, crackedPos);
    } else {
        crackedPos = id.find_last_of("-");
        id = id.substr(0, crackedPos);
    }

    std::string dbPath = "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/" + id + ".db";
    remove(dbPath.c_str());
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
    std::string videoId("traffic-4k-002-ds2k-not-tiled-cracked");
    std::string metadataId("traffic-4k-002-ds2k-not-tiled");
//    std::string videoId("traffic-4k-002-ds2k-0_1800-3x3");
    auto yolo = lightdb::extensibility::Load("yologpu");

//    DeleteDatabase(videoId);
//    DeleteTiles(videoId);
//    ResetTileNum(videoId);

    // First, detect objects.
//    auto input = ScanMultiTiled(videoId);
//    FrameMetadataSpecification selection(std::make_shared<EntireFrameMetadataElement>(30, 120));
//    Coordinator().execute(input.Select(selection).Map(yolo));

    std::vector<std::pair<int, int>> frames {
//            {0, 30},
//            {0, 60},
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
    for (auto firstLast : frames) {
        DeleteTilesPastNum(videoId, MAX_TILES);
        ResetTileNum(videoId, MAX_TILES);

        auto first = firstLast.first;
        auto last = firstLast.second;
        std::string qid(std::to_string(first) + "_" + std::to_string(last));
        PixelsInFrameMetadataSpecification selection(
                std::make_shared<SingleMetadataElement>("label", "car", first, last));
        std::filesystem::path outputPath;

        StatsCollector::instance().setUpNewQuery(qid, "untiled", "select");
        outputPath = "/home/maureen/masked_videos/traffic_car_boxed_untiled_" + std::to_string(first) + "_" + std::to_string(last) + ".mp4";
        std::cout << "Saving to " << outputPath << std::endl;
        Coordinator().execute(ScanMultiTiled(videoId).Select(selection, yolo).Save(outputPath));

        StatsCollector::instance().setUpNewQuery(qid, "untiled_optimized", "select");
        outputPath = "/home/maureen/masked_videos/traffic_car_boxed_untiled_optimized_" + std::to_string(first) + "_" + std::to_string(last) + ".mp4";
        std::cout << "Saving to " << outputPath << std::endl;
        Coordinator().execute(ScanMultiTiled(videoId).Select(
                selection, yolo, {{ScanOptions::ReadAllFrames, true}, {ScanOptions::IsReadingUntiledVideo, true}}
                ).Save(outputPath));

        // Then, re-tile around cars.
        StatsCollector::instance().setUpNewQuery(qid, "tiled", "crack");
        auto framerate = 30u;
        auto retileOp = ScanAndRetile(
                videoId,
                "traffic-4k-002-ds2k",
                selection,
                framerate,
                CrackingStrategy::SmallTiles,
                RetileStrategy::RetileIfDifferent,
                {}, {},
                {{RetileOptions::SplitByGOP, true}});
        Coordinator().execute(retileOp);

        // Then, do selection on people tiles.
        // Re-do scan so that new tiles are discovered.
        StatsCollector::instance().setQueryComponent("select");
        outputPath = "/home/maureen/masked_videos/traffic_car_boxed_stitched_" + std::to_string(first) + "_" + std::to_string(last) + ".mp4";
        std::cout << "Saving to " << outputPath << std::endl;
        Coordinator().execute(ScanMultiTiled(videoId).Select(selection, yolo, {{MetadataOptions::MetadataIdentifier, metadataId}}).Save(outputPath));

        std::ofstream stats("/home/maureen/results/visualroad_experiments/test.csv");
        auto csv = StatsCollector::instance().toCSV();
        stats.write(csv.data(), csv.length());
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
