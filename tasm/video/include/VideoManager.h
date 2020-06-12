#ifndef TASM_VIDEOMANAGER_H
#define TASM_VIDEOMANAGER_H

#include "GPUContext.h"
#include "ImageUtilities.h"
#include "VideoLock.h"
#include <experimental/filesystem>
#include <TileConfigurationProvider.h>

namespace tasm {
class SemanticIndex;
class MetadataSelection;
class TemporalSelection;
class Video;

class VideoManager {
public:
    VideoManager()
        : gpuContext_(new GPUContext(0)),
        lock_(new VideoLock(gpuContext_)) {
        createCatalogIfNecessary();
    }

    void store(const std::experimental::filesystem::path &path, const std::string &name);
    void storeWithUniformLayout(const std::experimental::filesystem::path &path, const std::string &name, unsigned int numRows, unsigned int numColumns);

    std::unique_ptr<ImageIterator> select(const std::string &video,
                                          const std::string &metadataIdentifier,
                                          std::shared_ptr<MetadataSelection> metadataSelection,
                                          std::shared_ptr<TemporalSelection> temporalSelection,
                                          std::shared_ptr<SemanticIndex> semanticIndex);

private:
    void createCatalogIfNecessary();
    void storeTiledVideo(std::shared_ptr<Video>, std::shared_ptr<TileLayoutProvider>, const std::string &savedName);

    std::shared_ptr<GPUContext> gpuContext_;
    std::shared_ptr<VideoLock> lock_;
};

} // namespace tasm

#endif //TASM_VIDEOMANAGER_H
