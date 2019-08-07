//
// Created by Maureen Daum on 2019-06-21.
//

#include "MetadataLightField.h"

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))

namespace lightdb::associations {
    static std::unordered_map<std::string, std::string> VideoPathToLabelsPath(
            {
                    {"/home/maureen/dog_videos/dog_with_keyframes.hevc", "/home/maureen/dog_videos/dog_with_keyframes.boxes"},
                    {"/home/maureen/lightdb/cmake-build-debug-remote/test/resources/dog_with_keyframes/1-0-stream.mp4", "/home/maureen/dog_videos/dog_with_keyframes.boxes"}
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
std::unordered_set<int> MetadataManager::framesForMetadata(const MetadataSpecification &metadataSpecification) const {

    sqlite3 *db;
    ASSERT_SQLITE_OK(sqlite3_open_v2(lightdb::associations::VideoPathToLabelsPath.at(pathToMetadata_).c_str(), &db, SQLITE_OPEN_READONLY, NULL));

    char *selectFramesStatement = nullptr;
    int size = asprintf(&selectFramesStatement, "SELECT DISTINCT FRAME FROM %s WHERE %s = '%s';",
                        metadataSpecification.tableName().c_str(),
                        metadataSpecification.columnName().c_str(),
                        metadataSpecification.expectedValue().c_str());
    assert(size != -1);

    sqlite3_stmt *select;
    ASSERT_SQLITE_OK(sqlite3_prepare_v2(db, selectFramesStatement, size, &select, nullptr));

    // Step and get results.
    std::unordered_set<int> frames;
    int result;
    while ((result = sqlite3_step(select)) == SQLITE_ROW) {
        frames.insert(sqlite3_column_int(select, 0));
    }
    assert(result == SQLITE_DONE);

    sqlite3_finalize(select);
    sqlite3_close(db);
    free(selectFramesStatement);

    return frames;
}
} // namespace lightdb::logical
