//
// Created by Maureen Daum on 2019-06-21.
//

#include "MetadataLightField.h"

#define ASSERT_SQLITE_OK(i) (assert(i == SQLITE_OK))
#define ASSERT_SQLITE_DONE(i) (assert(i == SQLITE_DONE))

namespace lightdb::associations {
    static const std::string traffic_2k_tracking_db = "/home/maureen/visualroad/2k-short/traffic-001-with-tracking-info-corrected.db";
    static const std::string traffic_2k_tracking_db_no_id = "/home/maureen/visualroad/2k-short/traffic-001-with-tracking-info-corrected-no-id.db";
    static const std::string traffic_2k_db = "/home/maureen/visualroad/2k-short/traffic-001.db";

    static const std::unordered_map<std::string, std::string> VideoPathToLabelsPath(
            {
                    {"traffic-2k-001", traffic_2k_db}, // "/home/maureen/visualroad_tiling/labels_yolo/traffic-2k-001_yolo.db"}, //   traffic_2k_db

                    // 4k-002
                    {"traffic-4k-002", "/home/maureen/visualroad/4k-short/traffic-4k-002.db"}, //"/home/maureen/visualroad_tiling/labels_yolo/traffic-4k-002_yolo.db"}, //  "/home/maureen/visualroad/4k-short/traffic-4k-002.db"
                    {"traffic-4k-002-ds1k", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-1k.db"},
                    {"traffic-4k-002-ds2k", "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"}, //"/home/maureen/visualroad_tiling/labels_yolo/traffic-4k-002-ds2k_yolo.db"}, //  "/home/maureen/visualroad/4k-short/traffic-4k-002-downsampled-to-2k.db"

                    // 4k-000
                    {"traffic-4k-000", "/home/maureen/visualroad/4k-short/traffic-4k-000.db"}, //"/home/maureen/visualroad_tiling/labels_yolo/traffic-4k-000_yolo.db"}, //  "/home/maureen/visualroad/4k-short/traffic-4k-000.db"

                    // short-1k-2
                    {"traffic-1k-002", "/home/maureen/visualroad/1k-short-2/traffic-1k-002.db"},

                    // car-pov-2k-000
                    {"car-pov-2k-000", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"},

                    // car-pov-2k-000-shortened
                    {"car-pov-2k-000-shortened", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"}, //"/home/maureen/visualroad_tiling/labels_yolo/car-pov-2k-000-shortened_yolo.db"}, //  "/home/maureen/visualroad/car-pov-2k/car-pov-2k-000.db"

                    // car-pov-2k-001-shortened
                    {"car-pov-2k-001-shortened", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"}, //"/home/maureen/visualroad_tiling/labels_yolo/car-pov-2k-001-shortened_yolo.db"}, // "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"

                    // car-pov-2k-001
                    {"car-pov-2k-001", "/home/maureen/visualroad/car-pov-2k/car-pov-2k-001.db"},

                    // coral-reef
                    {"coral-reef", "/home/maureen/noscope_videos/coral-reef-short.db"},

                    // CDVL
                    // Aerial
                    {"aerial", "/home/maureen/cdvl/labels/aerial.db"},

                    // Busy
                    {"busy", "/home/maureen/cdvl/labels/busy.db"},

                    // Childs
                    {"childs", "/home/maureen/cdvl/labels/childs.db"},

                    // Nature
                    {"nature", "/home/maureen/cdvl/labels/nature.db"},

                    // NFLX_DATASET
                    // birdsincage
                    {"birdsincage", "/home/maureen/NFLX_dataset/detections/labels/birdsincage.db"},

                    // crowdrun
                    {"crowdrun", "/home/maureen/NFLX_dataset/detections/labels/crowdRun.db"},

                    // elfuente1
                    {"elfuente1", "/home/maureen/NFLX_dataset/detections/labels/elFuente1.db"},

                    // elfuente2
                    {"elfuente2", "/home/maureen/NFLX_dataset/detections/labels/elfuente2.db"},

                    // oldtown
                    {"oldtown", "/home/maureen/NFLX_dataset/detections/labels/oldTown.db"},

                    // seeking
                    {"seeking", "/home/maureen/NFLX_dataset/detections/labels/seeking.db"},

                    // tennis
                    {"tennis", "/home/maureen/NFLX_dataset/detections/labels/tennis.db"},

                    // XIPH DATASET
                    {"red_kayak", "/home/maureen/xiph/labels/red_kayak.db"},
                    {"touchdown_pass", "/home/maureen/xiph/labels/touchdown_pass.db"},
                    {"park_joy_2k", "/home/maureen/xiph/labels/park_joy_2k.db"},
                    {"park_joy_4k", "/home/maureen/xiph/labels/park_joy_4k.db"},
                    {"Netflix_ToddlerFountain", "/home/maureen/xiph/labels/Netflix_ToddlerFountain.db"},
                    {"Netflix_Narrator", "/home/maureen/xiph/labels/Netflix_Narrator.db"},
                    {"Netflix_FoodMarket", "/home/maureen/xiph/labels/Netflix_FoodMarket.db"},
                    {"Netflix_FoodMarket2", "/home/maureen/xiph/labels/Netflix_FoodMarket2.db"},
                    {"Netflix_DrivingPOV", "/home/maureen/xiph/labels/Netflix_DrivingPOV.db"},
                    {"Netflix_BoxingPractice", "/home/maureen/xiph/labels/Netflix_BoxingPractice.db"},

                    // MOT16 dataset
                    {"MOT16-01", "/home/maureen/mot16/labels/MOT16-01.db"},
                    {"MOT16-02", "/home/maureen/mot16/labels/MOT16-02.db"},
                    {"MOT16-03", "/home/maureen/mot16/labels/MOT16-03.db"},
                    {"MOT16-04", "/home/maureen/mot16/labels/MOT16-04.db"},
                    {"MOT16-07", "/home/maureen/mot16/labels/MOT16-07.db"},
                    {"MOT16-08", "/home/maureen/mot16/labels/MOT16-08.db"},
                    {"MOT16-09", "/home/maureen/mot16/labels/MOT16-09.db"},
                    {"MOT16-10", "/home/maureen/mot16/labels/MOT16-10.db"},
                    {"MOT16-11", "/home/maureen/mot16/labels/MOT16-11.db"},
                    {"MOT16-12", "/home/maureen/mot16/labels/MOT16-12.db"},
                    {"MOT16-13", "/home/maureen/mot16/labels/MOT16-13.db"},
                    {"MOT16-14", "/home/maureen/mot16/labels/MOT16-14.db"},

                    // NETFLIX dataset
                    {"meridian", "/home/maureen/Netflix/labels/meridian_60fps.db"},
                    {"elfuente_full", "/home/maureen/cdvl/labels/elfuente_full_60fps.db"},
                    {"cosmos", "/home/maureen/Netflix/labels/cosmos.db"},

                    // El Fuente full scenes
                    {"market_all", "/home/maureen/cdvl/elfuente_scenes/labels/market_all.db"},
                    {"narrator", "/home/maureen/cdvl/elfuente_scenes/labels/narrator.db"},
                    {"river_boat", "/home/maureen/cdvl/elfuente_scenes/labels/river_boat.db"},
                    {"square_with_fountain", "/home/maureen/cdvl/elfuente_scenes/labels/square_with_fountain.db"},
                    {"street_night_view", "/home/maureen/cdvl/elfuente_scenes/labels/street_night_view.db"},

                    // BlazeIt videos.
                    {"venice-grand-canal-2018-01-17", "/home/maureen/blazeit_stuff/data/background_subtraction/venice-grand-canal-2018-01-17.db"},
                    {"venice-grand-canal-2018-01-19", "/home/maureen/blazeit_stuff/data/background_subtraction/venice-grand-canal-2018-01-19.db"},
                    {"venice-grand-canal-2018-01-20", "/home/maureen/blazeit_stuff/data/background_subtraction/venice-grand-canal-2018-01-20.db"},
                    {"jackson-town-square-2017-12-14", "/home/maureen/blazeit_stuff/data/background_subtraction/jackson-town-square-2017-12-14.db"},
                    {"jackson-town-square-2017-12-16", "/home/maureen/blazeit_stuff/data/background_subtraction/jackson-town-square-2017-12-16.db"},
                    {"jackson-town-square-2017-12-17", "/home/maureen/blazeit_stuff/data/background_subtraction/jackson-town-square-2017-12-17.db"},
                    {"long-venice-grand-canal-2018-01-17", "/home/maureen/blazeit_stuff/data/background_subtraction/long-venice-grand-canal-2018-01-17.db"},
                    {"long-venice-grand-canal-2018-01-19", "/home/maureen/blazeit_stuff/data/background_subtraction/long-venice-grand-canal-2018-01-19.db"},
                    {"long-venice-grand-canal-2018-01-20", "/home/maureen/blazeit_stuff/data/background_subtraction/long-venice-grand-canal-2018-01-20.db"},
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
    void MetadataManager::createDatabase() {
        sqlite3 *db;
        int result = sqlite3_open_v2(dbPath_.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        assert(result == SQLITE_OK);

        // Create table called "labels".
        const char *createTable = "CREATE TABLE LABELS(" \
                        "LABEL  TEXT    NOT NULL," \
                        "FRAME INT NOT NULL," \
                        "X INT NULL," \
                        "Y INT NULL," \
                        "WIDTH INT NOT NULL," \
                        "HEIGHT INT NOT NULL," \
                        "PRIMARY KEY (LABEL, FRAME, X, Y, WIDTH, HEIGHT));";


        char *error = nullptr;
        result = sqlite3_exec(db, createTable, NULL, NULL, &error);
        if (result != SQLITE_OK) {
            std::cout << "Error\n";
            sqlite3_free(error);
        }

        // Create table to keep track of which frames have been detected on.
        const char *createDetectedTable = "CREATE TABLE DETECTED(" \
                "FRAME INT NOT NULL PRIMARY KEY, "
                "DETECTED INT NOT NULL);";
        result = sqlite3_exec(db, createDetectedTable, NULL, NULL, &error);
        if (result != SQLITE_OK) {
            std::cout << "Error creating detected table\n";
            sqlite3_free(error);
        }


        ASSERT_SQLITE_OK(sqlite3_close(db));
    }

    int MetadataManager::hasDetectedTableCallback(int argc, char **argv, char **colName) {
        hasDetectedTable_ = true;
        return 0;
    }

    static int c_hasDetectedTableCallback(void *param, int argc, char **argv, char **colName) {
        MetadataManager *manager = reinterpret_cast<MetadataManager*>(param);
        return manager->hasDetectedTableCallback(argc, argv, colName);
    }

    void MetadataManager::openDatabase() {
        dbPath_ = lightdb::associations::VideoPathToLabelsPath.count(videoIdentifier_)
                ? lightdb::associations::VideoPathToLabelsPath.at(videoIdentifier_)
                : "/home/maureen/lightdb-wip/cmake-build-debug-remote/test/resources/" + videoIdentifier_ + ".db";

        if (!std::filesystem::exists(dbPath_))
            createDatabase();

        std::scoped_lock lock(mutex_);
        int openResult = sqlite3_open_v2(dbPath_.c_str(), &db_, SQLITE_OPEN_READWRITE, NULL);
        assert(openResult == SQLITE_OK);
        sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", 0, 0, 0);

        const char *createIndex = "CREATE INDEX IF NOT EXISTS video_index ON labels (frame, label)";
        auto result = sqlite3_exec(db_, createIndex, NULL, NULL, NULL);
        assert(result == SQLITE_OK);

        // Prepare insert statement.
        std::string query = "INSERT INTO labels (label, frame, x, y, width, height) VALUES (?, ?, ?, ?, ?, ?)";
        result = sqlite3_prepare_v2(db_, query.c_str(), query.length(), &insertStmt_, nullptr);
        ASSERT_SQLITE_OK(result);

        // Check whether the detected table exists. If it doesn't, for backwards compatibility we'll assume detection ran on all frames.
        query = "SELECT name FROM sqlite_master WHERE type='table' AND name='DETECTED'";
        result = sqlite3_exec(db_, query.c_str(), &c_hasDetectedTableCallback, this, nullptr);
        ASSERT_SQLITE_OK(result);

        // Prepare checking for detected statement.
        if (hasDetectedTable_) {
            query = "SELECT COUNT(*) FROM detected WHERE frame>=? AND frame<?;";
            result = sqlite3_prepare_v2(db_, query.c_str(), query.length(), &detectedStmt_, nullptr);
            ASSERT_SQLITE_OK(result);

            query = "SELECT * FROM detected WHERE frame=? AND detected=1";
            ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &detectedOnFrameStmt_, nullptr));

            query = "INSERT INTO detected (frame, detected) VALUES (?, 1);";
            ASSERT_SQLITE_OK(sqlite3_prepare_v2(db_, query.c_str(), query.length(), &markDetectedStmt_, nullptr));
        }
    }

    void MetadataManager::closeDatabase() {
        std::scoped_lock lock(mutex_);

        ASSERT_SQLITE_OK(sqlite3_finalize(insertStmt_));
        ASSERT_SQLITE_OK(sqlite3_finalize(detectedStmt_));
        ASSERT_SQLITE_OK(sqlite3_finalize(detectedOnFrameStmt_));
        ASSERT_SQLITE_OK(sqlite3_finalize(markDetectedStmt_));

        int closeResult = sqlite3_close(db_);
        assert(closeResult == SQLITE_OK);
    }

    void MetadataManager::selectFromMetadataAndApplyFunction(const char* query, std::function<void(sqlite3_stmt*)> resultFn, std::function<void(sqlite3*)> afterOpeningFn) const {
        char *selectFramesStatement = nullptr;
        int size = asprintf(&selectFramesStatement, query, metadataSpecification_.tableName().c_str());
//        if (metadataSpecification_.expectedValue().length()) {
//            size = asprintf(&selectFramesStatement, query,
//                            metadataSpecification_.tableName().c_str(),
//                            metadataSpecification_.columnName().c_str(),
//                            metadataSpecification_.expectedValue().c_str());
//        } else {
//            size = asprintf(&selectFramesStatement, query,
//                            metadataSpecification_.tableName().c_str(),
//                            metadataSpecification_.columnName().c_str(),
//                            std::to_string(metadataSpecification_.expectedIntValue()).c_str());
//        }
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
    int size = asprintf(&selectFramesStatement, query, metadataSpecification_.tableName().c_str());
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

const std::unordered_set<int> &MetadataManager::framesForMetadata() {
        std::scoped_lock lock(mutex_);
    if (didSetFramesForMetadata_)
        return framesForMetadata_;

    std::string query = "SELECT DISTINCT frame FROM %s WHERE " + metadataSpecification_.whereClauseConstraints(true);

    selectFromMetadataAndApplyFunctionWithFrameLimits(query.c_str(), [this](sqlite3_stmt *stmt) {
        framesForMetadata_.insert(sqlite3_column_int(stmt, 0));
    });

    didSetFramesForMetadata_ = true;
    return framesForMetadata_;
}

const std::vector<int> &MetadataManager::framesForMetadataOrderedByNumObjects() const {
    std::scoped_lock lock(mutex_);
    if (didSetFramesForMetadataOrderedByNumObjects_)
        return framesForMetadataOrderedByNumObjects_;

    std::string query = "SELECT frame from %s WHERE " + metadataSpecification_.whereClauseConstraints(true) + " GROUP BY frame ORDER BY count(*) desc";
    selectFromMetadataAndApplyFunctionWithFrameLimits(query.c_str(), [this](sqlite3_stmt *stmt) {
        framesForMetadataOrderedByNumObjects_.push_back(sqlite3_column_int(stmt, 0));
    });

    didSetFramesForMetadataOrderedByNumObjects_ = true;
    return framesForMetadataOrderedByNumObjects_;
}

void MetadataManager::addMetadata(const std::string &label, int frame, int x1, int y1, int width, int height) {
    std::scoped_lock lock(mutex_);

    ASSERT_SQLITE_OK(sqlite3_bind_text(insertStmt_, 1, label.c_str(), -1, SQLITE_STATIC ));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertStmt_, 2, frame));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertStmt_, 3, x1));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertStmt_, 4, y1));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertStmt_, 5, width));
    ASSERT_SQLITE_OK(sqlite3_bind_int(insertStmt_, 6, height));

    auto result = sqlite3_step(insertStmt_);
    assert(result == SQLITE_DONE);
    sqlite3_reset(insertStmt_);
}

void MetadataManager::addMetadata(const std::list<MetadataInfo> &metadataInfo, std::pair<int, int> firstLastFrame) {
    sqlite3_exec(db_, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    for (int f = firstLastFrame.first; f < firstLastFrame.second; ++f) {
        for (const auto &m : metadataInfo)
            addMetadata(m.label, f, m.x1, m.y1, m.w, m.h);

        markDetectionHasBeenRunOnFrame(videoIdentifier_, f);
    }
    sqlite3_exec(db_, "END TRANSACTION;", NULL, NULL, NULL);
}

const std::vector<int> &MetadataManager::orderedFramesForMetadata() {
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

int MetadataManager::getFramesWithoutMetadataCallback(int numCols, char **values, char **columns) {
    assert(numCols == 1);
    int frame = std::atoi(values[0]);
    framesWithoutMetadata_.erase(frame);
    return 0;
}

static int c_getFramesWithoutMetadataCallback(void *param, int argc, char **argv, char **columns) {
    MetadataManager *manager = reinterpret_cast<MetadataManager*>(param);
    return manager->getFramesWithoutMetadataCallback(argc, argv, columns);
}

const std::unordered_set<int> &MetadataManager::framesWithoutMetadata() {
    std::scoped_lock lock(mutex_);
    if (didSetFramesWithoutMetadata_)
        return framesWithoutMetadata_;

    if (!hasDetectedTable_) {
        framesWithoutMetadata_.clear();
        didSetFramesWithoutMetadata_ = true;
        return framesWithoutMetadata_;
    }

    // Create a set with all possible frames.
    int firstFrame = metadataSpecification().firstFrame();
    int lastFrame = metadataSpecification().lastFrame();
    framesWithoutMetadata_.clear();
    for (auto i = firstFrame; i < lastFrame; ++i)
        framesWithoutMetadata_.insert(i);

    // Select all frames with detections from the db.
    std::string query = "SELECT frame FROM detected WHERE detected=1 AND frame>=" + std::to_string(firstFrame) + " AND frame<" + std::to_string(lastFrame);
    char *error = nullptr;
    auto result = sqlite3_exec(db_, query.c_str(), &c_getFramesWithoutMetadataCallback, this, &error);
    assert(result == SQLITE_OK);

    didSetFramesWithoutMetadata_ = true;
    return framesWithoutMetadata_;
}

const std::vector<int> &MetadataManager::orderedFramesForMetadataOrWithoutMetadata() {
    {
        std::scoped_lock lock(mutex_);
        if (didSetOrderedFramesForMetadataOrWithoutMetadata_)
            return orderedFramesForMetadataOrWithoutMetadata_;
    }

    std::unordered_set<int> frames = framesForMetadata();
    const std::unordered_set<int> &framesWithout = framesWithoutMetadata();
    frames.insert(framesWithout.begin(), framesWithout.end());

    std::scoped_lock lock(mutex_);
    orderedFramesForMetadataOrWithoutMetadata_.clear();
    orderedFramesForMetadataOrWithoutMetadata_.reserve(frames.size());
    orderedFramesForMetadataOrWithoutMetadata_.insert(orderedFramesForMetadataOrWithoutMetadata_.begin(), frames.begin(), frames.end());
    std::sort(orderedFramesForMetadataOrWithoutMetadata_.begin(), orderedFramesForMetadataOrWithoutMetadata_.end());

    didSetOrderedFramesForMetadataOrWithoutMetadata_ = true;
    return orderedFramesForMetadataOrWithoutMetadata_;
}

const std::unordered_set<int> &MetadataManager::idealKeyframesForMetadata() {
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
    return *rectanglesPtrForFrame(frame);
}

std::shared_ptr<std::vector<Rectangle>> MetadataManager::rectanglesPtrForFrame(int frame) const {
    if ((unsigned int)frame < metadataSpecification_.firstFrame() || (unsigned int)frame >= metadataSpecification_.lastFrame())
        return nullptr;

    std::scoped_lock lock(mutex_);
    if (frameToRectangles_.count(frame))
        return frameToRectangles_[frame];

    frameToRectangles_[frame] = std::shared_ptr<std::vector<Rectangle>>(new std::vector<Rectangle>());

    std::string query = "SELECT frame, x, y, width, height FROM %s WHERE (" + metadataSpecification_.whereClauseConstraints(false) + ") and frame = " + std::to_string(frame) + ";";

    selectFromMetadataAndApplyFunction(query.c_str(), [this, frame](sqlite3_stmt *stmt) {
        unsigned int queryFrame = sqlite3_column_int(stmt, 0);
        assert(queryFrame == (unsigned int)frame);
        unsigned int x = sqlite3_column_int(stmt, 1);
        unsigned int y = sqlite3_column_int(stmt, 2);
        unsigned int width = sqlite3_column_int(stmt, 3);
        unsigned int height = sqlite3_column_int(stmt, 4);

        frameToRectangles_[frame]->emplace_back(queryFrame, x, y, width, height);
    });
    return frameToRectangles_[frame];
}

std::unique_ptr<std::list<Rectangle>> MetadataManager::rectanglesForFrames(int firstFrameInclusive, int lastFrameExclusive) const {
    std::unique_ptr<std::list<Rectangle>> rectangles(new std::list<Rectangle>());
    std::string query = "SELECT frame, x, y, width, height FROM %s WHERE ("
            + metadataSpecification_.whereClauseConstraints(false)
            + ") AND frame >= " + std::to_string(firstFrameInclusive)
            + " AND frame < " + std::to_string(lastFrameExclusive);

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

std::unordered_set<int> MetadataManager::idealKeyframesForMetadataAndTiles(const tiles::TileLayout &tileLayout) {
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

    std::string query = "SELECT DISTINCT frame FROM %s WHERE " + metadataSpecification_.whereClauseConstraints(true) + " AND RECTANGLEINTERSECT(x, y, width, height) == 1;";
    std::vector<int> frames;
    selectFromMetadataAndApplyFunctionWithFrameLimits(query.c_str(), [&](sqlite3_stmt *stmt) {
        int queryFrame = sqlite3_column_int(stmt, 0);
        frames.push_back(queryFrame);
    }, registerIntersectionFunction);

    return frames;
}

bool MetadataManager::anyFramesLackDetections() const {
    int firstFrameInclusive = metadataSpecification().firstFrame();
    int lastFrameExclusive = metadataSpecification().lastFrame();
    ASSERT_SQLITE_OK(sqlite3_bind_int(detectedStmt_, 1, firstFrameInclusive));
    ASSERT_SQLITE_OK(sqlite3_bind_int(detectedStmt_, 2, lastFrameExclusive));

    assert(sqlite3_step(detectedStmt_) == SQLITE_ROW);
    int count = sqlite3_column_int(detectedStmt_, 0);
    ASSERT_SQLITE_DONE(sqlite3_step(detectedStmt_));
    sqlite3_reset(detectedStmt_);

    return count != lastFrameExclusive - firstFrameInclusive;
}

bool MetadataManager::detectionHasBeenRunOnFrame(const std::string &videoId, int frame) {
    assert(!videoId.length() || videoId == videoIdentifier_);
    ASSERT_SQLITE_OK(sqlite3_bind_int(detectedOnFrameStmt_, 1, frame));
    auto result = sqlite3_step(detectedOnFrameStmt_);
    sqlite3_reset(detectedOnFrameStmt_);

    return result != SQLITE_DONE;
}

void MetadataManager::markDetectionHasBeenRunOnFrame(const std::string &videoId, int frame) {
    assert(!videoId.length() || videoId == videoIdentifier_);
    ASSERT_SQLITE_OK(sqlite3_bind_int(markDetectedStmt_, 1, frame));
    ASSERT_SQLITE_DONE(sqlite3_step(markDetectedStmt_));
    sqlite3_reset(markDetectedStmt_);
}



} // namespace lightdb::metadata
