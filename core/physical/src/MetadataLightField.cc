//
// Created by Maureen Daum on 2019-06-21.
//

#include "MetadataLightField.h"

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))

namespace lightdb::associations {
    static const std::unordered_map<std::string, std::string> VideoPathToLabelsPath(
            {
                    {"/home/maureen/dog_videos/dog_with_keyframes.hevc", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_keyframes/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_gop_60/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_gop_30/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_gop_15/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_gop_10/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_gop_5/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/debugbuild/test/resources/dog_with_gop_60/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/debugbuild/test/resources/dog_with_gop_30/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/debugbuild/test/resources/dog_with_gop_15/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/debugbuild/test/resources/dog_with_gop_10/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/debugbuild/test/resources/dog_with_gop_5/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_keyframes_real/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_gop250/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_gop60/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_gop30/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_gop15/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_gop10/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_gop5/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_gop1/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_40191_van/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_40191/MVI_40191.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_gop250/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_gop60/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_gop30/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_gop15/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_gop10/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_gop5/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_gop1/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_van/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_bus/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_others/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_car/1-0-stream.mp4", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/jackson_town_square_gop250/1-0-stream.mp4", "/home/maureen/noscope_videos/jackson_town_square_1hr.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/jackson_town_square_gop60/1-0-stream.mp4", "/home/maureen/noscope_videos/jackson_town_square_1hr.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/jackson_town_square_gop30/1-0-stream.mp4", "/home/maureen/noscope_videos/jackson_town_square_1hr.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/jackson_town_square_gop10/1-0-stream.mp4", "/home/maureen/noscope_videos/jackson_town_square_1hr.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/jackson_town_square_gop5/1-0-stream.mp4", "/home/maureen/noscope_videos/jackson_town_square_1hr.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/jackson_town_square_car/1-0-stream.mp4", "/home/maureen/noscope_videos/jackson_town_square_1hr.db"},
                    {"MVI_63563_tiled", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_tiled/orig-tile-0.hevc", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_tiled/orig-tile-1.hevc", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_tiled/black-tile-0.hevc", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/MVI_63563_tiled/black-tile-1.hevc", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"MVI_63563_tiled_custom_gops", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"MVI_63563_gops_for_tiles", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"jackson_square_150frame_gops_for_tiles", "/home/maureen/noscope_videos/jackson_town_square_resized_150frames.db"},
                    {"MVI_63563_gop30", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"jackson_square_gops_for_tiles", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/noscope_videos/tiles/jackson_left.hevc", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/noscope_videos/tiles/jackson_right.hevc", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/noscope_videos/tiles/black-tile-0.hevc", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/jackson_town_square_gop30/1-0-stream.mp4", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/noscope_videos/tiles/shortened/2x2tiles", "/home/maureen/noscope_videos/tiles/shortened/2x2tiles/jackson-town-square-640x512-150frames.db"},
                    {"jackson_square_150frame_680x512_gops_for_tiles", "/home/maureen/noscope_videos/tiles/shortened/2x2tiles/jackson-town-square-640x512-150frames.db"},
                    {"/home/maureen/noscope_videos/tiles/2x2tiles", "/home/maureen/noscope_videos/tiles/2x2tiles/jackson-town-square-640x512-1hr.db"},
                    {"jackson_square_1hr_680x512_gops_for_tiles", "/home/maureen/noscope_videos/tiles/2x2tiles/jackson-town-square-640x512-1hr.db"},
                    {"/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/jackson_square_1hr_680x512/1-0-stream.mp4", "/home/maureen/noscope_videos/tiles/2x2tiles/jackson-town-square-640x512-1hr.db"},
                    {"/home/maureen/noscope_videos/tiles/shortened/orig-tile-0.hevc", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/noscope_videos/tiles/shortened/orig-tile-1.hevc", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/noscope_videos/tiles/shortened/black-tile-0.hevc", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"/home/maureen/noscope_videos/tiles/shortened/black-tile-1.hevc", "/home/maureen/noscope_videos/tiles/jackson_town_square_1hr_resized.db"},
                    {"MVI_63563_960x576_100frames_cracked", "/home/maureen/uadetrac_videos/MVI_63563/multi-tile/MVI_63563_100frames.db"},
                    {"MVI_63563_960x576_100frames", "/home/maureen/uadetrac_videos/MVI_63563/multi-tile/MVI_63563_100frames.db"},
                    {"MVI_63563_960x576_100frames_cracked_van", "/home/maureen/uadetrac_videos/MVI_63563/multi-tile/MVI_63563_100frames.db"},
                    {"MVI_63563_960x576_100frames_cracked_van4x4", "/home/maureen/uadetrac_videos/MVI_63563/multi-tile/MVI_63563_100frames.db"},
                    {"MVI_63563_960x576_100frames_crackedForVan", "/home/maureen/uadetrac_videos/MVI_63563/multi-tile/MVI_63563_100frames.db"},
                    {"MVI_63563_960x576", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"MVI_63563_960x576_crackedForVan", "/home/maureen/uadetrac_videos/MVI_63563/MVI_63563.db"},
                    {"traffic-2k", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked3x3", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked3x3-2", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop60", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop120", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop240", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-cracked-gop300", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-single-tile", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-001", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration240-car", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration120-car", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration60-car", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration30-car", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration240-pedestrian", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration120-pedestrian", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration60-pedestrian", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-layoutduration30-pedestrian", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-single-tile-pedestrian", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-single-tile-car", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-single-tile", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-groupingextent-layoutduration60-pedestrian", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-001-cracked-groupingextent-layoutduration60-car", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-2k-cracked-groupingextent-layoutduration60-car", "/home/maureen/visualroad/2k-short/traffic-003.db"},
                    {"traffic-2k-001-cracked-grouped_around_all_objects-layoutduration60", "/home/maureen/visualroad/2k-short/traffic-001.db"},
                    {"traffic-4k-002", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-groupingextent-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-layoutduration60-pedestrian", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-002-cracked-groupingextent-layoutduration60-pedestrian", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"},
                    {"traffic-4k-000", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-groupingextent-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
                    {"traffic-4k-000-cracked-layoutduration60-car", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"},
            } );
} // namespace lightdb::associations

namespace lightdb::physical {

std::vector<Rectangle> MetadataLightField::allRectangles() {
    std::vector<Rectangle> allRectangles;
    std::for_each(metadata_.begin(), metadata_.end(), [&](std::pair<std::string, std::vector<Rectangle>> labelAndRectangles) {
       allRectangles.insert(allRectangles.end(), labelAndRectangles.second.begin(), labelAndRectangles.second.end());
    });

    return allRectangles;
}
} // namespace lightdb::physical

namespace lightdb::metadata {

    void MetadataManager::openDatabase() {
        std::scoped_lock lock(mutex_);
        int openResult = sqlite3_open_v2(lightdb::associations::VideoPathToLabelsPath.at(videoIdentifier_).c_str(), &db_, SQLITE_OPEN_READONLY, NULL);
        assert(openResult == SQLITE_OK);
    }

    void MetadataManager::closeDatabase() {
        std::scoped_lock lock(mutex_);
        int closeResult = sqlite3_close(db_);
        assert(closeResult == SQLITE_OK);
    }

    void MetadataManager::selectFromMetadataAndApplyFunction(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
        char *selectFramesStatement = nullptr;
        int size = asprintf(&selectFramesStatement, query,
                            metadataSpecification_.tableName().c_str(),
                            metadataSpecification_.columnName().c_str(),
                            metadataSpecification_.expectedValue().c_str());
        assert(size != -1);
        selectFromMetadataWithoutQuerySubstitutionsAndApplyFunction(selectFramesStatement, size, resultFn, afterOpeningFn);
        free(selectFramesStatement);
    }

    void MetadataManager::selectFromMetadataWithoutQuerySubstitutionsAndApplyFunction(const char* query, unsigned int querySize, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
        // Assume that mutex is already held.
        if (afterOpeningFn)
            afterOpeningFn(db_);

        sqlite3_stmt *select;
        auto prepareResult = sqlite3_prepare_v2(db_, query, querySize, &select, nullptr);
        ASSERT_SQLITE_OK(prepareResult);

        // Step and get results.
        int result;
        while ((result = sqlite3_step(select)) == SQLITE_ROW)
            resultFn(select);

        assert(result == SQLITE_DONE);

        sqlite3_finalize(select);
    }

void MetadataManager::selectFromMetadataAndApplyFunctionWithFrameLimits(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
    // Assume that mutex is already held.
    if (afterOpeningFn)
        afterOpeningFn(db_);

    char *selectFramesStatement = nullptr;
    int size = asprintf(&selectFramesStatement, query,
                        metadataSpecification_.tableName().c_str(),
                        metadataSpecification_.columnName().c_str(),
                        metadataSpecification_.expectedValue().c_str(),
                        std::to_string(metadataSpecification_.firstFrame()).c_str(),
                        std::to_string(metadataSpecification_.lastFrame()).c_str());
    assert(size != -1);

    sqlite3_stmt *select;
    auto prepareResult = sqlite3_prepare_v2(db_, selectFramesStatement, size, &select, nullptr);
    ASSERT_SQLITE_OK(prepareResult);

    // Step and get results.
    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW)
        resultFn(select);

    assert(result == SQLITE_DONE);

    sqlite3_finalize(select);
    free(selectFramesStatement);
}

const std::unordered_set<int> &MetadataManager::framesForMetadata() const {
        std::scoped_lock lock(mutex_);
    if (didSetFramesForMetadata_)
        return framesForMetadata_;

    selectFromMetadataAndApplyFunctionWithFrameLimits("SELECT DISTINCT frame FROM %s WHERE %s = '%s' AND frame >= %s AND frame < %s;", [this](sqlite3_stmt *stmt) {
        framesForMetadata_.insert(sqlite3_column_int(stmt, 0));
    });

    didSetFramesForMetadata_ = true;
    return framesForMetadata_;
}

const std::vector<int> &MetadataManager::orderedFramesForMetadata() const {
    {
        std::scoped_lock lock(mutex_);
        if (didSetOrderedFramesForMetadata_)
            return orderedFramesForMetadata_;
    }

    std::unordered_set<int> frames = framesForMetadata();

    std::scoped_lock lock(mutex_);
    orderedFramesForMetadata_.resize(frames.size());
    auto currentIndex = 0;
    for (auto &frame : frames)
        orderedFramesForMetadata_[currentIndex++] = frame;

    std::sort(orderedFramesForMetadata_.begin(), orderedFramesForMetadata_.end());

    didSetOrderedFramesForMetadata_ = true;
    return orderedFramesForMetadata_;
}

const std::unordered_set<int> &MetadataManager::idealKeyframesForMetadata() const {
    {
        std::scoped_lock lock(mutex_);
        if (didSetIdealKeyframesForMetadata_)
            return idealKeyframesForMetadata_;
    }

    auto &orderedFrames = orderedFramesForMetadata();

    {
        std::scoped_lock lock(mutex_);
        didSetIdealKeyframesForMetadata_ = true;
        idealKeyframesForMetadata_ = MetadataManager::idealKeyframesForFrames(orderedFrames);
        return idealKeyframesForMetadata_;
    }
}

std::unordered_set<int> MetadataManager::idealKeyframesForFrames(const std::vector<int> &orderedFrames) {
    if (orderedFrames.empty())
        return {};

    // Find the starts of all sequences.
    auto lastFrame = orderedFrames.front();
    std::unordered_set<int> idealKeyframes;
    idealKeyframes.insert(lastFrame);
    for (auto it = orderedFrames.begin() + 1; it != orderedFrames.end(); it++) {
        if (*it != lastFrame + 1)
            idealKeyframes.insert(*it);

        lastFrame = *it;
    }

    return idealKeyframes;
}

const std::vector<Rectangle> &MetadataManager::rectanglesForFrame(int frame) const {
    static std::vector<Rectangle> emptyVector;
    if (frame < metadataSpecification_.firstFrame() || frame >= metadataSpecification_.lastFrame())
        return emptyVector;

    std::scoped_lock lock(mutex_);
    if (frameToRectangles_.count(frame))
        return frameToRectangles_[frame];

    std::string query = "SELECT frame, x, y, width, height FROM %s WHERE %s = '%s' and frame = " + std::to_string(frame) + ";";

    selectFromMetadataAndApplyFunction(query.c_str(), [this, frame](sqlite3_stmt *stmt) {
        unsigned int queryFrame = sqlite3_column_int(stmt, 0);
        assert(queryFrame == (unsigned int)frame);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        frameToRectangles_[frame].emplace_back(queryFrame, x, y, width, height);
    });
    return frameToRectangles_[frame];
}

std::unique_ptr<std::list<Rectangle>> MetadataManager::rectanglesForFrames(int firstFrameInclusive, int lastFrameExclusive) const {
    std::unique_ptr<std::list<Rectangle>> rectangles(new std::list<Rectangle>());
    std::string query = "SELECT frame, x, y, width, height FROM %s WHERE %s = '%s' and frame >= " + std::to_string(firstFrameInclusive) + " and frame < " + std::to_string(lastFrameExclusive);
    selectFromMetadataAndApplyFunction(query.c_str(), [&](sqlite3_stmt *stmt) {
        unsigned int frame = sqlite3_column_int(stmt, 0);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        rectangles->emplace_back(frame, x, y, width, height);
    });
    return rectangles;
}

std::unique_ptr<std::list<Rectangle>> MetadataManager::rectanglesForAllObjectsForFrames(int firstFrameInclusive, int lastFrameExclusive) const {
    std::unique_ptr<std::list<Rectangle>> rectangles(new std::list<Rectangle>());
    std::string query = "SELECT frame, x, y, width, height FROM " + metadataSpecification_.tableName() + " where frame >= " + std::to_string(firstFrameInclusive) + " and frame < " + std::to_string(lastFrameExclusive) + ";";
    selectFromMetadataWithoutQuerySubstitutionsAndApplyFunction(query.c_str(), query.length(), [&](sqlite3_stmt *stmt) {
        unsigned int frame = sqlite3_column_int(stmt, 0);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        rectangles->emplace_back(frame, x, y, width, height);
    });
    return rectangles;
}

static void doesRectangleIntersectTileRectangle(sqlite3_context *context, int argc, sqlite3_value **argv) {
    assert(argc == 4);

    Rectangle *tileRectangle = reinterpret_cast<Rectangle*>(sqlite3_user_data(context));

    unsigned int x = sqlite3_value_int(argv[0]);
    unsigned int y = sqlite3_value_int(argv[1]);
    unsigned int width = sqlite3_value_int(argv[2]);
    unsigned int height = sqlite3_value_int(argv[3]);

    if (tileRectangle->intersects(Rectangle(0, x, y, width, height)))
        sqlite3_result_int(context, 1);
    else
        sqlite3_result_int(context, 0);
}

std::unordered_set<int> MetadataManager::idealKeyframesForMetadataAndTiles(const tiles::TileLayout &tileLayout) const {
    auto numberOfTiles = tileLayout.numberOfTiles();
    std::vector<std::vector<int>> framesForEachTile(numberOfTiles);
    for (auto i = 0u; i < numberOfTiles; ++i)
        framesForEachTile[i] = framesForTileAndMetadata(i, tileLayout);

    std::vector<int> globalFrames = orderedFramesForMetadata();
    if (!globalFrames.size())
        return {};

    enum class Inclusion {
        InGlobalFrames,
        NotInGlobalFrames,
    };

    std::vector<std::vector<int>::const_iterator> tileFrameIterators(numberOfTiles);
    std::transform(framesForEachTile.begin(), framesForEachTile.end(), tileFrameIterators.begin(), [](const auto &frames) {
        return frames.begin();
    });
    std::vector<int>::const_iterator globalFramesIterator = globalFrames.begin();

    std::vector<Inclusion> tileInclusions(numberOfTiles, Inclusion::NotInGlobalFrames);
    auto updateTileInclusions = [&]() -> bool {
        bool anyChanged = false;
        for (auto tileIndex = 0u; tileIndex < numberOfTiles; ++tileIndex) {
            if (tileFrameIterators[tileIndex] != framesForEachTile[tileIndex].end()) {
                if (*tileFrameIterators[tileIndex] == *globalFramesIterator) {
                    if (tileInclusions[tileIndex] == Inclusion::NotInGlobalFrames)
                        anyChanged = true;
                    tileInclusions[tileIndex] = Inclusion::InGlobalFrames;
                    ++tileFrameIterators[tileIndex];
                } else {
                    if (tileInclusions[tileIndex] == Inclusion::InGlobalFrames)
                        anyChanged = true;
                    tileInclusions[tileIndex] = Inclusion::NotInGlobalFrames;
                }
            } else {
                if (tileInclusions[tileIndex] == Inclusion::InGlobalFrames)
                    anyChanged = true;
                tileInclusions[tileIndex] = Inclusion::NotInGlobalFrames;
            }
        }
        return anyChanged;
    };
    // Set initial state of tile inclusions. We don't care about whether any change because the first frame is always a keyframe.
    updateTileInclusions();

    // Add the first global frame to the set of keyframes.
    std::unordered_set<int> keyframes;
    keyframes.insert(*globalFramesIterator++);

    for(; globalFramesIterator != globalFrames.end(); globalFramesIterator++) {
        if (updateTileInclusions())
            keyframes.insert(*globalFramesIterator);
    }

    // Insert frames from normal sequence.
    auto &originalIdealKeyframes = idealKeyframesForMetadata();
    keyframes.insert(originalIdealKeyframes.begin(), originalIdealKeyframes.end());

    return keyframes;
}


std::vector<int> MetadataManager::framesForTileAndMetadata(unsigned int tile, const tiles::TileLayout &tileLayout) const {
    // Get rectangle for tile.
    auto tileRectangle = tileLayout.rectangleForTile(tile);

    // Select frames that have a rectangle that intersects the tile rectangle.
    auto registerIntersectionFunction = [&](sqlite3 *db) {
        ASSERT_SQLITE_OK(sqlite3_create_function(db, "RECTANGLEINTERSECT", 4, SQLITE_UTF8 | SQLITE_DETERMINISTIC, &tileRectangle, doesRectangleIntersectTileRectangle, NULL, NULL));
    };

    std::string query = "SELECT DISTINCT frame FROM %s WHERE %s = '%s' AND frame >= %s AND frame < %s AND RECTANGLEINTERSECT(x, y, width, height) == 1;";

    std::vector<int> frames;
    selectFromMetadataAndApplyFunctionWithFrameLimits(query.c_str(), [&](sqlite3_stmt *stmt) {
        int queryFrame = sqlite3_column_int(stmt, 0);
        frames.push_back(queryFrame);
    }, registerIntersectionFunction);

    return frames;
}





} // namespace lightdb::metadata
