#ifndef TASM_SEMANTICINDEX_H
#define TASM_SEMANTICINDEX_H

#include "Rectangle.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "sqlite3.h"
#include <experimental/filesystem>
#include <string>

namespace tasm {

class SemanticIndex {
public:
    virtual void addMetadata(const std::string &video,
            const std::string &label,
            unsigned int frame,
            unsigned int x1,
            unsigned int y1,
            unsigned int x2,
            unsigned int y2) = 0;

    virtual std::unique_ptr<std::vector<int>> orderedFramesForSelection(
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            std::shared_ptr<TemporalSelection> temporalSelection) = 0;

    virtual std::unique_ptr<std::vector<Rectangle>> rectanglesForFrame(
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            int frame) = 0;

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

    std::unique_ptr<std::vector<int>> orderedFramesForSelection(
            const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            std::shared_ptr<TemporalSelection> temporalSelection) override;

    std::unique_ptr<std::vector<Rectangle>> rectanglesForFrame(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int frame) override;

    ~SemanticIndexSQLite() {
        destroyStatements();
        closeDatabase();
    }

private:
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
