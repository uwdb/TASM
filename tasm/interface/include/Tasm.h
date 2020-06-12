#ifndef TASM_TASM_H
#define TASM_TASM_H

#include "SemanticIndex.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "VideoManager.h"

#include <memory>
#include <string>

namespace tasm {

class TASM {
public:
    TASM()
        : semanticIndex_(new SemanticIndexSQLite())
    {}

    TASM(std::experimental::filesystem::path dbPath)
        : semanticIndex_(new SemanticIndexSQLite(dbPath))
    {}

    TASM(const TASM&) = delete;

    virtual void addMetadata(const std::string &video,
            const std::string &label,
            unsigned int frame,
            unsigned int x1,
            unsigned int y1,
            unsigned int x2,
            unsigned int y2);

    virtual void store(const std::string &videoPath, const std::string &savedName) {
        videoManager_.store(videoPath, savedName);
    }

    virtual std::unique_ptr<ImageIterator> select(const std::string &video,
                         const std::string &label,
                         unsigned int firstFrameInclusive,
                         unsigned int lastFrameExclusive) {
        return videoManager_.select(
                video,
                std::make_shared<SingleMetadataSelection>(label),
                std::make_shared<RangeTemporalSelection>(firstFrameInclusive, lastFrameExclusive),
                semanticIndex_);
    }

    virtual ~TASM() = default;

private:
    std::shared_ptr<SemanticIndex> semanticIndex_;
    VideoManager videoManager_;
};

} // namespace tasm

#endif //TASM_TASM_H
