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
    enum class IndexType {
        XY,
        LegacyWH,
        InMemory,
    };

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
    virtual void setup() {
        openDatabase(dbPath_);
        initializeStatements();
    }

protected:
    SemanticIndexSQLiteBase(const std::experimental::filesystem::path &dbPath)
            : dbPath_(dbPath)
    { }

    virtual std::unique_ptr<std::list<Rectangle>> rectanglesForQuery(sqlite3_stmt *stmt, unsigned int maxWidth = 0, unsigned int maxHeight = 0) = 0;
    virtual void openDatabase(const std::experimental::filesystem::path &dbPath) = 0;
    virtual void createTable() = 0;
    virtual void closeDatabase() = 0;
    virtual void initializeStatements() = 0;
    virtual void destroyStatements() = 0;

    sqlite3 *db_;

    // Statements.
    sqlite3_stmt *addMetadataStmt_;

    const std::experimental::filesystem::path dbPath_;
};

class SemanticIndexSQLite : public SemanticIndexSQLiteBase {
    friend class SemanticIndexFactory;
public:
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
    SemanticIndexSQLite(const std::experimental::filesystem::path &dbPath)
            : SemanticIndexSQLiteBase(dbPath)
    {}

    // Finalizes the statement.
    std::unique_ptr<std::list<Rectangle>> rectanglesForQuery(sqlite3_stmt *stmt, unsigned int maxWidth = 0, unsigned int maxHeight = 0) override;

    void openDatabase(const std::experimental::filesystem::path &dbPath) override;
    void createTable() override;
    void closeDatabase() override;
    void initializeStatements() override;
    void destroyStatements() override;
};

class SemanticIndexSQLiteInMemory : public SemanticIndexSQLite {
    friend class SemanticIndexFactory;
protected:
    SemanticIndexSQLiteInMemory()
            : SemanticIndexSQLite(":memory:")
    { }
};

class SemanticIndexWH : public SemanticIndexSQLiteBase {
    friend class SemanticIndexFactory;
public:
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
    SemanticIndexWH(const std::experimental::filesystem::path &dbPath)
            : SemanticIndexSQLiteBase(dbPath)
    { }

    // Finalizes the statement.
    std::unique_ptr<std::list<Rectangle>> rectanglesForQuery(sqlite3_stmt *stmt, unsigned int maxWidth = 0, unsigned int maxHeight = 0) override;

    void openDatabase(const std::experimental::filesystem::path &dbPath) override;
    void createTable() override;
    void closeDatabase() override;
    void initializeStatements() override;
    void destroyStatements() override;
};

class SemanticIndexFactory {
public:
    static std::shared_ptr<SemanticIndex> create(SemanticIndex::IndexType indexType, const std::experimental::filesystem::path &path) {
        std::shared_ptr<SemanticIndexSQLiteBase> index;
        switch (indexType) {
            case SemanticIndex::IndexType::XY:
                index = std::shared_ptr<SemanticIndexSQLite>(new SemanticIndexSQLite(path));
                break;
            case SemanticIndex::IndexType::LegacyWH:
                index = std::shared_ptr<SemanticIndexWH>(new SemanticIndexWH(path));
                break;
            case SemanticIndex::IndexType::InMemory:
                index = std::shared_ptr<SemanticIndexSQLiteInMemory>(new SemanticIndexSQLiteInMemory());
                break;
            default:
                std::cerr << "Unrecognized index type: " << static_cast<std::underlying_type<SemanticIndex::IndexType>::type>(indexType) << std::endl;
                assert(false);
        }
        index->setup();
        return index;
    }

    static std::shared_ptr<SemanticIndex> createInMemory() {
        return create(SemanticIndex::IndexType::InMemory, "");
    }
};

} // namespace tasm

#endif //TASM_SEMANTICINDEX_H
