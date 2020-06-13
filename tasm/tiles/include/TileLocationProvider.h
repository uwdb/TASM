#ifndef TASM_TILELOCATIONPROVIDER_H
#define TASM_TILELOCATIONPROVIDER_H

#include "Files.h"
#include "TileConfigurationProvider.h"
#include "TiledVideoManager.h"

namespace tasm {

class TileLocationProvider : public TileLayoutProvider {
public:
    virtual std::experimental::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const = 0;

    unsigned int frameOffsetInTileFile(const std::experimental::filesystem::path &tilePath) const {
        return TileFiles::firstAndLastFramesFromPath(tilePath.parent_path()).first;
    }

    virtual unsigned int lastFrameWithLayout() const = 0;

    virtual ~TileLocationProvider() {}
};

class SingleTileLocationProvider : public TileLocationProvider {
public:
    SingleTileLocationProvider(std::shared_ptr<const TiledVideoManager> tileLayoutsManager)
            : tileLayoutsManager_(tileLayoutsManager)
    { }

    std::experimental::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const override {
        return tileLayoutsManager_->locationOfTileForId(tileNumber, layoutIdForFrame(frame));
    }

    std::shared_ptr<const TileLayout> tileLayoutForFrame(unsigned int frame) override {
        std::scoped_lock lock(mutex_);
        return tileLayoutsManager_->tileLayoutForId(layoutIdForFrame(frame));
    }

    unsigned int lastFrameWithLayout() const override {
        return tileLayoutsManager_->maximumFrame();
    }

private:
    int layoutIdForFrame(unsigned int frame) const {
        std::scoped_lock lock(mutex_);

        if (frameToLayoutId_.count(frame))
            return frameToLayoutId_.at(frame);

        auto layoutIds = tileLayoutsManager_->tileLayoutIdsForFrame(frame);
        // Pick the one with the largest value because it's the most recent layout.
        auto layoutId = *std::max_element(layoutIds.begin(), layoutIds.end());

        frameToLayoutId_[frame] = layoutId;
        return layoutId;
    }

    std::shared_ptr<const TiledVideoManager> tileLayoutsManager_;
    mutable std::unordered_map<unsigned int, int> frameToLayoutId_;
    mutable std::recursive_mutex mutex_;
};

} // namespace tasm

#endif //TASM_TILELOCATIONPROVIDER_H
