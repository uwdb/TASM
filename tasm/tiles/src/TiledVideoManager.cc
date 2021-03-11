#include "TiledVideoManager.h"

#include "Files.h"
#include "Gpac.h"

namespace tasm {

void TiledVideoManager::loadAllTileConfigurations() {
    std::string video = entry_->name();
    totalWidth_ = layoutDB_->totalWidth(video);
    totalHeight_ = layoutDB_->totalHeight(video);
    largestWidth_ = layoutDB_->largestWidth(video);
    largestHeight_ = layoutDB_->largestHeight(video);
    maximumFrame_ = layoutDB_->maximumFrame(video);
}

std::vector<int> TiledVideoManager::tileLayoutIdsForFrame(unsigned int frameNumber) const {
    return layoutDB_->tileLayoutIdsForFrame(entry_->name(), frameNumber);
}

std::shared_ptr<TileLayout> TiledVideoManager::tileLayoutForId(int id) const {
    return layoutDB_->tileLayoutForId(entry_->name(), id);
}

std::experimental::filesystem::path TiledVideoManager::locationOfTileForId(unsigned int tileNumber, int id) const {
    return layoutDB_->locationOfTileForId(entry_, tileNumber, id);
}

} // namespace tasm