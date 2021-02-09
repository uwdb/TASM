#ifndef TASM_SEMANTICINDEX_H
#define TASM_SEMANTICINDEX_H

#include "EnvironmentConfiguration.h"
#include "Rectangle.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "sqlite3.h"
#include <experimental/filesystem>
#include <string>
#include <iostream>

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

class SemanticIndexSQLiteBase : public SemanticIndex {
public:
    void addBulkMetadata(const std::vector<MetadataInfo>&) override;

protected:
    virtual std::unique_ptr<std::list<Rectangle>> rectanglesForQuery(sqlite3_stmt *stmt, unsigned int maxWidth = 0, unsigned int maxHeight = 0) = 0;
    virtual void openDatabase(const std::experimental::filesystem::path &dbPath) = 0;
    virtual void createTable() = 0;
    virtual void closeDatabase() = 0;
    virtual void initializeStatements() = 0;
    virtual void destroyStatements() = 0;

    sqlite3 *db_;

    // Statements.
    sqlite3_stmt *addMetadataStmt_;
};

class SemanticIndexSQLite : public SemanticIndexSQLiteBase {
public:
    SemanticIndexSQLite(const std::experimental::filesystem::path &dbPath = EnvironmentConfiguration::instance().defaultLabelsDatabasePath()) {
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

    std::unique_ptr<std::list<Rectangle>> rectanglesForFrame(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int frame, unsigned int maxWidth = 0, unsigned int maxHeight = 0) override;
    std::unique_ptr<std::list<Rectangle>> rectanglesForFrames(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int firstFrameInclusive, int lastFrameExclusive) override;

    ~SemanticIndexSQLite() {
        destroyStatements();
        closeDatabase();
    }

protected:
    // Finalizes the statement.
    std::unique_ptr<std::list<Rectangle>> rectanglesForQuery(sqlite3_stmt *stmt, unsigned int maxWidth = 0, unsigned int maxHeight = 0) override;

    void openDatabase(const std::experimental::filesystem::path &dbPath) override;
    void createTable() override;
    void closeDatabase() override;
    void initializeStatements() override;
    void destroyStatements() override;

    sqlite3 *db_;

    // Statements.
    sqlite3_stmt *addMetadataStmt_;
};

class SemanticIndexSQLiteInMemory : public SemanticIndexSQLite {
public:
    SemanticIndexSQLiteInMemory() {
        openDatabase("");
        initializeStatements();
    }

protected:
    void openDatabase(const std::experimental::filesystem::path &dbPath) override;
};

class SemanticIndexWH : public SemanticIndexSQLiteBase {
public:
    SemanticIndexWH(std::experimental::filesystem::path dbPath) {
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

    std::unique_ptr<std::list<Rectangle>> rectanglesForFrame(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int frame, unsigned int maxWidth = 0, unsigned int maxHeight = 0) override;
    std::unique_ptr<std::list<Rectangle>> rectanglesForFrames(const std::string &video, std::shared_ptr<MetadataSelection> metadataSelection, int firstFrameInclusive, int lastFrameExclusive) override;

    ~SemanticIndexWH() {
        destroyStatements();
        closeDatabase();
    }

protected:
    // Finalizes the statement.
    std::unique_ptr<std::list<Rectangle>> rectanglesForQuery(sqlite3_stmt *stmt, unsigned int maxWidth = 0, unsigned int maxHeight = 0) override;

    void openDatabase(const std::experimental::filesystem::path &dbPath) override;
    void createTable() override;
    void closeDatabase() override;
    void initializeStatements() override;
    void destroyStatements() override;
};

} // namespace tasm

#endif //TASM_SEMANTICINDEX_H
