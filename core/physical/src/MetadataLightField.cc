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

    void MetadataManager::selectFromMetadataAndApplyFunction(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
        sqlite3 *db;
        ASSERT_SQLITE_OK(sqlite3_open_v2(lightdb::associations::VideoPathToLabelsPath.at(videoIdentifier_).c_str(), &db, SQLITE_OPEN_READONLY, NULL));

        if (afterOpeningFn)
            afterOpeningFn(db);

        char *selectFramesStatement = nullptr;
        int size = asprintf(&selectFramesStatement, query,
                            metadataSpecification_.tableName().c_str(),
                            metadataSpecification_.columnName().c_str(),
                            metadataSpecification_.expectedValue().c_str());
        assert(size != -1);

        sqlite3_stmt *select;
        auto prepareResult = sqlite3_prepare_v2(db, selectFramesStatement, size, &select, nullptr);
        ASSERT_SQLITE_OK(prepareResult);

        // Step and get results.
        int result;
        while ((result = sqlite3_step(select)) == SQLITE_ROW)
            resultFn(select);

        assert(result == SQLITE_DONE);

        sqlite3_finalize(select);
        sqlite3_close(db);
        free(selectFramesStatement);
    }

const std::unordered_set<int> &MetadataManager::framesForMetadata() const {
    if (didSetFramesForMetadata_)
        return framesForMetadata_;

    selectFromMetadataAndApplyFunction("SELECT DISTINCT FRAME FROM %s WHERE %s = '%s';", [this](sqlite3_stmt *stmt) {
        framesForMetadata_.insert(sqlite3_column_int(stmt, 0));
    });

    didSetFramesForMetadata_ = true;
    return framesForMetadata_;
}

const std::vector<int> &MetadataManager::orderedFramesForMetadata() const {
    if (didSetOrderedFramesForMetadata_)
        return orderedFramesForMetadata_;

    std::unordered_set<int> frames = framesForMetadata();

    orderedFramesForMetadata_.resize(frames.size());
    auto currentIndex = 0;
    for (auto &frame : frames)
        orderedFramesForMetadata_[currentIndex++] = frame;

    std::sort(orderedFramesForMetadata_.begin(), orderedFramesForMetadata_.end());

    didSetOrderedFramesForMetadata_ = true;
    return orderedFramesForMetadata_;
}

const std::unordered_set<int> &MetadataManager::idealKeyframesForMetadata() const {
    if (didSetIdealKeyframesForMetadata_)
        return idealKeyframesForMetadata_;

    didSetIdealKeyframesForMetadata_ = true;
    idealKeyframesForMetadata_ = MetadataManager::idealKeyframesForFrames(orderedFramesForMetadata());
    return idealKeyframesForMetadata_;
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
    if (frameToRectangles_.count(frame))
        return frameToRectangles_[frame];

    std::string query = "SELECT frame, x, y, width, height FROM %s WHERE %s = '%s' and frame = " + std::to_string(frame) + ";";

    selectFromMetadataAndApplyFunction(query.c_str(), [this, frame](sqlite3_stmt *stmt) {
        unsigned int queryFrame = sqlite3_column_int(stmt, 0);
        assert(queryFrame == frame);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        frameToRectangles_[frame].emplace_back(queryFrame, x, y, width, height);
    });
    return frameToRectangles_[frame];
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

std::vector<int> MetadataManager::framesForTileAndMetadata(unsigned int tile, const tiles::TileLayout &tileLayout) const {
    // Get rectangle for tile.
    auto tileRectangle = tileLayout.rectangleForTile(tile);

    // Select frames that have a rectangle that intersects the tile rectangle.
    auto registerIntersectionFunction = [&](sqlite3 *db) {
        ASSERT_SQLITE_OK(sqlite3_create_function(db, "RECTANGLEINTERSECT", 4, SQLITE_UTF8 | SQLITE_DETERMINISTIC, &tileRectangle, doesRectangleIntersectTileRectangle, NULL, NULL));
    };

    std::string query = "SELECT DISTINCT frame FROM %s WHERE %s = '%s' AND RECTANGLEINTERSECT(x, y, width, height) == 1;";

    std::vector<int> frames;
    selectFromMetadataAndApplyFunction(query.c_str(), [&](sqlite3_stmt *stmt) {
        int queryFrame = sqlite3_column_int(stmt, 0);
        frames.push_back(queryFrame);
    }, registerIntersectionFunction);

    return frames;
}



} // namespace lightdb::metadata
