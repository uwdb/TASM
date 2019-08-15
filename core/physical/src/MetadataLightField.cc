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
const std::unordered_set<int> &MetadataManager::framesForMetadata() const {
    if (didSetFramesForMetadata_)
        return framesForMetadata_;

    sqlite3 *db;
    ASSERT_SQLITE_OK(sqlite3_open_v2(lightdb::associations::VideoPathToLabelsPath.at(pathToVideo_).c_str(), &db, SQLITE_OPEN_READONLY, NULL));

    char *selectFramesStatement = nullptr;
    int size = asprintf(&selectFramesStatement, "SELECT DISTINCT FRAME FROM %s WHERE %s = '%s';",
                        metadataSpecification_.tableName().c_str(),
                        metadataSpecification_.columnName().c_str(),
                        metadataSpecification_.expectedValue().c_str());
    assert(size != -1);

    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db, selectFramesStatement, size, &select, nullptr));

    // Step and get results.
    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW) {
        framesForMetadata_.insert(sqlite3_column_int(select, 0));
    }
    assert(result == SQLITE_DONE);

    sqlite3_finalize(select);
    sqlite3_close(db);
    free(selectFramesStatement);

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

    auto allFrames = orderedFramesForMetadata();
    if (allFrames.empty())
        return {};

    // Find the starts of all sequences.
    auto lastFrame = allFrames.front();
    idealKeyframesForMetadata_.insert(lastFrame);
    for (auto it = allFrames.begin() + 1; it != allFrames.end(); it++) {
        if (*it != lastFrame + 1)
            idealKeyframesForMetadata_.insert(*it);

        lastFrame = *it;
    }

    return idealKeyframesForMetadata_;
}
} // namespace lightdb::metadata
