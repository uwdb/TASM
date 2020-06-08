#ifndef LIGHTDB_SEMANTICINDEX_H
#define LIGHTDB_SEMANTICINDEX_H

#include "sqlite3.h"
#include <filesystem>
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

    virtual ~SemanticIndex() {}
};

class SemanticIndexSQLite : public SemanticIndex {
public:
    SemanticIndexSQLite(std::filesystem::path dbPath = "labels.db") {
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

    ~SemanticIndexSQLite() {
        destroyStatements();
        closeDatabase();
    }

private:
    void openDatabase(const std::filesystem::path &dbPath);
    void createDatabase(const std::filesystem::path &dbPath);
    void closeDatabase();
    void initializeStatements();
    void destroyStatements();

    sqlite3 *db_;

    // Statements.
    sqlite3_stmt *addMetadataStmt_;
};

} // namespace tasm

#endif //LIGHTDB_SEMANTICINDEX_H
