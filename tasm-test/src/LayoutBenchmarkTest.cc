#include "TiledVideoManager.h"
#include "SemanticIndex.h"
#include "VideoManager.h"
#include "Tasm.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <functional>

using namespace tasm;

const int NUM_VIDEOS = 2;
const std::string VIDEO_PATHS[NUM_VIDEOS] {"/home/maureen/red_videos/red_6sec_2k.mp4", "/home/maureen/red_videos/red_900sec_2k.mp4"};
const std::string LABEL_PATHS[NUM_VIDEOS] {"/home/maureen/NFLX_dataset/detections/labels/birdsincage.db", "/home/maureen/visualroad_tiling/labels_yolo/traffic-2k-001_yolo.db"};
const std::string VIDEO_LABELS[NUM_VIDEOS] {"bird", "car"};
const std::string VIDEO_NAMES[NUM_VIDEOS] {"birdsincage", "traffic-2k-001_yolo"};
const int NUM_FRAMES[NUM_VIDEOS] {180, 27000};
const int IDS[NUM_VIDEOS] {197, 8476}; // from tileLayoutIdsForFrame

const int NUM_ITERATIONS = 3;
const int FUNCTION_REPETITIONS = 50; // for functions that are too fast to measure without repetition
const std::string SEP = ",";
const std::string IMPLEMENTATION = "layout database";
const std::string OUTFILE_NAME = "layout_database_benchmark.csv";

void printStat(std::ofstream &file, std::string experiment, std::string step, std::string video, int frame,
               int iteration, long time, int repetitions) {
    file << IMPLEMENTATION << SEP << experiment << SEP << step << SEP << video << SEP << frame << SEP << iteration <<
        SEP << time << SEP << repetitions << "\n";
    file.flush();
}

void runBenchmark(std::string videoName, std::shared_ptr<TiledEntry> entry, int iteration, std::ofstream &outputFile,
                  int frameNumber, std::string experiment, std::function<void(TiledVideoManager &)> testFunction) {
    std::chrono::system_clock::time_point totalStart = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point constructorStart = std::chrono::system_clock::now();

    TiledVideoManager tiledVideoManager(entry);

    std::chrono::system_clock::time_point constructorEnd = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point functionStart = std::chrono::system_clock::now();

    for (int i = 0; i < FUNCTION_REPETITIONS; i++) {
        testFunction(tiledVideoManager);
    }

    std::chrono::system_clock::time_point functionEnd = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point totalEnd = std::chrono::system_clock::now();

    auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(totalEnd - totalStart).count();
    auto constructorDuration = std::chrono::duration_cast<std::chrono::microseconds>(constructorEnd - constructorStart).count();
    auto functionDuration = std::chrono::duration_cast<std::chrono::microseconds>(functionEnd - functionStart).count();

    // output data to csv file
    printStat(outputFile, experiment, "total", videoName, frameNumber, iteration, totalDuration, FUNCTION_REPETITIONS);
    printStat(outputFile, experiment, "constructor", videoName, frameNumber, iteration, constructorDuration, FUNCTION_REPETITIONS);
    printStat(outputFile, experiment, "processing", videoName, frameNumber, iteration, functionDuration, FUNCTION_REPETITIONS);
}

void storeVideos(std::string videoName, std::string videoPath, std::string label, int iteration, std::ofstream &outputFile) {
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

    // store red video around objects
    TASM tasm(SemanticIndex::IndexType::LegacyWH);
    tasm.storeWithNonUniformLayout(videoPath, videoName, videoName, label);

    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // output data to csv file
    printStat(outputFile, "storeVideos", "processing", videoName, -1, iteration, duration, 1);
}

void testConstructor(std::string videoName, std::shared_ptr<TiledEntry> entry, int iteration, std::ofstream &outputFile) {
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

    TiledVideoManager tiledVideoManager(entry);

    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // output data to csv file
    printStat(outputFile, "testConstructor", "constructor", videoName, -1, iteration, duration, 1);
}

void testTileLayoutIdsForFrame (std::string videoName, std::shared_ptr<TiledEntry> entry, int frames, int iteration, std::ofstream &outputFile) {
    for (int frameNumber = 0; frameNumber < frames; frameNumber += frames / 4) {
        runBenchmark(videoName, entry, iteration, outputFile, frameNumber, "tileLayoutIdsForFrame", [frameNumber](TiledVideoManager &manager) {
            manager.tileLayoutIdsForFrame(frameNumber);
        });
    }
}

void testTileLayoutForId (std::string videoName, std::shared_ptr<TiledEntry> entry, int id, int iteration, std::ofstream &outputFile) {
    runBenchmark(videoName, entry, iteration, outputFile, -1, "tileLayoutForId", [id] (TiledVideoManager &manager){
        manager.tileLayoutForId(id);
    });
}

void testLocationOfTileForId (std::string videoName, std::shared_ptr<TiledEntry> entry, int frames, int id, int iteration, std::ofstream &outputFile) {
    for (int frameNumber = 0; frameNumber < frames; frameNumber += frames / 4) {
        runBenchmark(videoName, entry, iteration, outputFile, frameNumber, "locationOfTileForId", [frameNumber, id] (TiledVideoManager &manager){
            manager.locationOfTileForId(frameNumber, id);
        });
    }
}

void testTotalWidth (std::string videoName, std::shared_ptr<TiledEntry> entry, int iteration, std::ofstream &outputFile) {
    runBenchmark(videoName, entry, iteration, outputFile, -1, "testTotalWidth", [] (TiledVideoManager &manager){
        manager.totalWidth();
    });
}

void testTotalHeight (std::string videoName, std::shared_ptr<TiledEntry> entry, int iteration, std::ofstream &outputFile) {
    runBenchmark(videoName, entry, iteration, outputFile, -1, "totalHeight", [] (TiledVideoManager &manager){
        manager.totalHeight();
    });
}

void testLargestWidth (std::string videoName, std::shared_ptr<TiledEntry> entry, int iteration, std::ofstream &outputFile) {
    runBenchmark(videoName, entry, iteration, outputFile, -1, "largestWidth", [] (TiledVideoManager &manager){
        manager.largestWidth();
    });
}

void testLargestHeight (std::string videoName, std::shared_ptr<TiledEntry> entry, int iteration, std::ofstream &outputFile) {
    runBenchmark(videoName, entry, iteration, outputFile, -1, "largestHeight", [] (TiledVideoManager &manager){
        manager.largestHeight();
    });
}

void testMaximumFrame (std::string videoName, std::shared_ptr<TiledEntry> entry, int iteration, std::ofstream &outputFile) {
    runBenchmark(videoName, entry, iteration, outputFile, -1, "maximumFrame", [] (TiledVideoManager &manager){
        manager.maximumFrame();
    });
}

class LayoutBenchmarkTestFixture : public ::testing::Test {
public:
    LayoutBenchmarkTestFixture() {
    }
};

TEST_F(LayoutBenchmarkTestFixture, runBenchmarks) {
    // set test configuration
    std::unordered_map<std::string, std::string> options {
            {EnvironmentConfiguration::DefaultLayoutsDB, "benchmark-layout.db"},
            {EnvironmentConfiguration::CatalogPath, "benchmark-resources"},
            {EnvironmentConfiguration::DefaultLabelsDB, "benchmark-labels.db"}
    };
    auto config = EnvironmentConfiguration::instance(EnvironmentConfiguration(options));

    // open output file and add header
    std::ofstream outputFile;
    outputFile.open(OUTFILE_NAME, std::ios::out);
    outputFile << "implementation" << SEP << "experiment" << SEP << "step" << SEP << "video" << SEP << "frame"\
        << SEP << "iteration" << SEP << "time" << SEP << "repetitions\n";

    // run experiments for each video
    for (int i = 0; i < NUM_VIDEOS; i++) {
        std::string labelPath = LABEL_PATHS[i];
        std::string videoPath = VIDEO_PATHS[i];
        std::string label = VIDEO_LABELS[i];
        std::string name = VIDEO_NAMES[i];
        int frames = NUM_FRAMES[i];
        int id = IDS[i];

        std::experimental::filesystem::remove(config.defaultLayoutDatabasePath());
        LayoutDatabase::instance()->open();

        // load object database from real video
        std::experimental::filesystem::remove(config.defaultLabelsDatabasePath());
        std::experimental::filesystem::copy(labelPath, config.defaultLabelsDatabasePath());

        // initialize tiledentry
        std::shared_ptr<TiledEntry> entry = std::make_shared<TiledEntry>(name);

        for (int j = 0; j < NUM_ITERATIONS; j++) {
            storeVideos(name, videoPath, label, j, outputFile);
            testConstructor(name, entry, j, outputFile);
            testTileLayoutIdsForFrame(name, entry, frames, j, outputFile);
            testTileLayoutForId(name, entry, id, j, outputFile);
            testLocationOfTileForId(name, entry, id, frames, j, outputFile);
            testTotalWidth(name, entry, j, outputFile);
            testTotalHeight(name, entry, j, outputFile);
            testLargestWidth(name, entry, j, outputFile);
            testLargestHeight(name, entry, j, outputFile);
            testMaximumFrame(name, entry, j, outputFile);
        }
    }
    outputFile.close();
}
