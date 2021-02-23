#include "TiledVideoManager.h"

#include "Files.h"
#include "Gpac.h"

namespace tasm {

void TiledVideoManager::loadAllTileConfigurations() {
    std::string video = entry_->name();
    totalWidth_ = layouts_->totalWidth(video);
    totalHeight_ = layouts_->totalHeight(video);
    largestWidth_ = layouts_->largestWidth(video);
    largestHeight_ = layouts_->largestHeight(video);
    maximumFrame_ = layouts_->maximumFrame(video);
}

std::vector<int> TiledVideoManager::tileLayoutIdsForFrame(unsigned int frameNumber) const {
    return layouts_->tileLayoutIdsForFrame(entry_->name(), frameNumber);
}

std::shared_ptr<TileLayout> TiledVideoManager::tileLayoutForId(int id) const {
    return layouts_->tileLayoutForId(entry_->name(), id);
}

std::experimental::filesystem::path TiledVideoManager::locationOfTileForId(unsigned int tileNumber, int id) const {
    return layouts_->locationOfTileForId(entry_, tileNumber, id);
}

} // namespace tasm