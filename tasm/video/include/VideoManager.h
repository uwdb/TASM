#ifndef TASM_VIDEOMANAGER_H
#define TASM_VIDEOMANAGER_H

#include "GPUContext.h"
#include "MergeTiles.h"
#include "SemanticIndex.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "TransformToImage.h"
#include "VideoLock.h"
#include <experimental/filesystem>

namespace tasm {

class ImageIterator {
public:
    ImageIterator(std::shared_ptr<Operator<std::unique_ptr<std::vector<ImagePtr>>>> parent)
        : parent_(parent) {}

    ImagePtr next() {
        if (!currentImages_ || imageIterator_ == currentImages_->end())
            getNextSetOfImages();

        if (!currentImages_) {
            // Done.
            return ImagePtr();
        }

        return *imageIterator_++;
    }

private:
    void getNextSetOfImages() {
        currentImages_.reset();
        while (!currentImages_ && !parent_->isComplete()) {
            auto next = parent_->next();
            if (next.has_value() && (**next).size() > 0)
                currentImages_ = std::move(*next);
        }
        if (currentImages_)
            imageIterator_ = currentImages_->begin();
    }

    std::shared_ptr<Operator<std::unique_ptr<std::vector<ImagePtr>>>> parent_;
    std::unique_ptr<std::vector<ImagePtr>> currentImages_;
    std::vector<ImagePtr>::const_iterator imageIterator_;
};

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
