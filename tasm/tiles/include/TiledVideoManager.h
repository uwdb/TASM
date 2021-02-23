#ifndef TASM_TILEDVIDEOMANAGER_H
#define TASM_TILEDVIDEOMANAGER_H

#include "IntervalTree.h"
#include "TileLayout.h"
#include "Video.h"
#include <mutex>
#include "LayoutDatabase.h"

namespace tasm {
class TiledVideoManager {
public:
    TiledVideoManager(std::shared_ptr<TiledEntry> entry)
            : entry_(entry),
              layouts_(LayoutDatabase::instance()),
              totalWidth_(0),
              totalHeight_(0),
              largestWidth_(0),
              largestHeight_(0),
              maximumFrame_(0) {
        loadAllTileConfigurations();
    }

    std::shared_ptr<TiledEntry> entry() const { return entry_; }
    std::vector<int> tileLayoutIdsForFrame(unsigned int frameNumber) const;
    std::shared_ptr<TileLayout> tileLayoutForId(int id) const;
    std::experimental::filesystem::path locationOfTileForId(unsigned int tileNumber, int id) const;

    unsigned int totalWidth() const { return totalWidth_; }
    unsigned int totalHeight() const { return totalHeight_; }
    unsigned int largestWidth() const { return largestWidth_; }
    unsigned int largestHeight() const { return largestHeight_; }
    unsigned int maximumFrame() const { return maximumFrame_; }

private:
    void loadAllTileConfigurations();
    std::shared_ptr<TiledEntry> entry_;
    std::shared_ptr<LayoutDatabase> layouts_;

    unsigned int totalWidth_;
    unsigned int totalHeight_;
    unsigned int largestWidth_;
    unsigned int largestHeight_;
    unsigned int maximumFrame_;
};

} // namespace tasm

#endif //TASM_TILEDVIDEOMANAGER_H
