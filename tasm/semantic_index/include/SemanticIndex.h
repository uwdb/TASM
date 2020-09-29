#ifndef TASM_SEMANTICINDEX_H
#define TASM_SEMANTICINDEX_H

#include "Rectangle.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "sqlite3.h"
#include <experimental/filesystem>
#include <string>

namespace tasm {

struct MetadataInfo {
    MetadataInfo(const std::string &video, const std::string &label, unsigned int frame, unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
            : video(video), label(label), frame(frame), x1(x1), y1(y1), x2(x2), y2(y2)
    {}

    std::string video;
    std::string label;
    unsigned int frame;
    unsigned int x1;
    unsigned int y1;
    unsigned int x2;
    unsigned int y2;
};

class SemanticIndex {
public:
    virtual void addMetadata(const std::string &video,
            const std::string &label,
            unsigned int frame,
            unsigned int x1,
            unsigned int y1,
            unsigned int x2,
            unsigned int y2) = 0;

    virtual void addBulkMetadata(const std::vector<MetadataInfo>&) = 0;

    virtual std::unique_ptr<std::vector<int>> orderedFramesForSelection(
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            std::shared_ptr<TemporalSelection> temporalSelection) = 0;

    virtual std::unique_ptr<std::list<Rectangle>> rectanglesForFrame(
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            int frame,
            unsigned int maxWidth = 0,
            unsigned int maxHeight = 0) = 0;

    virtual std::unique_ptr<std::list<Rectangle>> rectanglesForFrames(
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            int firstFrameInclusive,
            int lastFrameExclusive) = 0;

    virtual ~SemanticIndex() {}
};

class SemanticIndexSQLite : public SemanticIndex {
public:
    SemanticIndexSQLite(std::experimental::filesystem::path dbPath = "labels.db") {
        openDatabase(dbPath);
        initializeStatements();
    }

    void addMetadata(const std::string &video,
                     const std::string &label,
                     unsigned int frame,
                     unsigned int x1,
                     unsigned int y1,
                     unsigned int x2,
                     unsigned int y2) override;

    void addBulkMetadata(const std::vector<MetadataInfo>&) override;

    std::unique_ptr<std::vector<int>> orderedFramesForSelection(
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            std::shared_ptr<TemporalSelection> temporalSelection) override;

    std::unique_ptr<std::list<Rectangle>> rectanglesForFrame(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int frame, unsigned int maxWidth = 0, unsigned int maxHeight = 0) override;
    std::unique_ptr<std::list<Rectangle>> rectanglesForFrames(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int firstFrameInclusive, int lastFrameExclusive) override;

    ~SemanticIndexSQLite() {
        destroyStatements();
        closeDatabase();
    }

private:
    // Finalizes the statement.
    std::unique_ptr<std::list<Rectangle>> rectanglesForQuery(sqlite3_stmt *stmt, unsigned int maxWidth = 0, unsigned int maxHeight = 0);

    void openDatabase(const std::experimental::filesystem::path &dbPath);
    void createDatabase(const std::experimental::filesystem::path &dbPath);
    void closeDatabase();
    void initializeStatements();
    void destroyStatements();

    sqlite3 *db_;

    // Statements.
    sqlite3_stmt *addMetadataStmt_;
};

} // namespace tasm

#endif //TASM_SEMANTICINDEX_H
