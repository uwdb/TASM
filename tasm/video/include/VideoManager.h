#ifndef TASM_VIDEOMANAGER_H
#define TASM_VIDEOMANAGER_H

#include "GPUContext.h"
#include "ImageUtilities.h"
#include "VideoLock.h"
#include <experimental/filesystem>

namespace tasm {
class SemanticIndex;
class MetadataSelection;
class TemporalSelection;

class VideoManager {
public:
    VideoManager()
        : gpuContext_(new GPUContext(0)),
        lock_(new VideoLock(gpuContext_)) {
        createCatalogIfNecessary();
    }

    void store(const std::experimental::filesystem::path &path, const std::string &name);

    std::unique_ptr<ImageIterator> select(const std::string &video,
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
