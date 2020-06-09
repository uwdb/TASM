#ifndef LIGHTDB_TASM_H
#define LIGHTDB_TASM_H

#include "SemanticIndex.h"

#include <memory>
#include <string>

namespace tasm {

class TASM {
public:
    TASM()
        : semanticIndex_(new SemanticIndexSQLite())
    {}

    TASM(std::filesystem::path dbPath)
        : semanticIndex_(new SemanticIndexSQLite(dbPath))
    {}

    TASM(const TASM&) = delete;

    void addMetadata(const std::string &video,
            const std::string &label,
            unsigned int frame,
            unsigned int x1,
            unsigned int y1,
            unsigned int x2,
            unsigned int y2);

private:
    std::unique_ptr<SemanticIndex> semanticIndex_;
};

} // namespace tasm

#endif //LIGHTDB_TASM_H
