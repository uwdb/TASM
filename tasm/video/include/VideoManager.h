#ifndef TASM_VIDEOMANAGER_H
#define TASM_VIDEOMANAGER_H

#include "GPUContext.h"
#include "SemanticIndex.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "VideoLock.h"
#include <filesystem>

namespace tasm {

class VideoManager {
public:
    VideoManager()
        : gpuContext_(new GPUContext(0)),
        lock_(new VideoLock(gpuContext_)) {
        createCatalogIfNecessary();
    }

    void store(const std::filesystem::path &path, const std::string &name);
    void select(const std::string &video,
            std::shared_ptr<MetadataSelection> metadataSelection,
            std::shared_ptr<TemporalSelection> temporalSelection,
            std::shared_ptr<SemanticIndex> semanticIndex);

private:
    void createCatalogIfNecessary();

    std::shared_ptr<GPUContext> gpuContext_;
    std::shared_ptr<VideoLock> lock_;
};

} // namespace tasm

#endif //TASM_VIDEOMANAGER_H
