#ifndef TASM_TASM_H
#define TASM_TASM_H

#include "SemanticIndex.h"
#include "VideoManager.h"

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

    void store(const std::string &videoPath, const std::string &savedName) {
        videoManager_.store(videoPath, savedName);
    }

private:
    std::unique_ptr<SemanticIndex> semanticIndex_;
    VideoManager videoManager_;
};

} // namespace tasm

#endif //TASM_TASM_H
