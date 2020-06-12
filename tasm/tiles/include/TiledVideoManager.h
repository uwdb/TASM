#ifndef TASM_TILEDVIDEOMANAGER_H
#define TASM_TILEDVIDEOMANAGER_H

#include "IntervalTree.h"
#include "TileLayout.h"
#include "Video.h"
#include <mutex>

namespace tasm {
class TiledVideoManager {
public:
    TiledVideoManager(std::shared_ptr<TiledEntry> entry)
            : entry_(entry),
              totalWidth_(0),
              totalHeight_(0),
              largestWidth_(0),
              largestHeight_(0),
              maximumFrame_(0) {
        loadAllTileConfigurations();
    }

    std::shared_ptr<TiledEntry> entry() const { return entry_; }
    std::vector<int> tileLayoutIdsForFrame(unsigned int frameNumber) const;
    const TileLayout &tileLayoutForId(int id) const { return *directoryIdToTileLayout_.at(id); }
    std::experimental::filesystem::path locationOfTileForId(unsigned int tileNumber, int id) const;

    unsigned int totalWidth() const { return totalWidth_; }
    unsigned int totalHeight() const { return totalHeight_; }
    unsigned int largestWidth() const { return largestWidth_; }
    unsigned int largestHeight() const { return largestHeight_; }
    unsigned int maximumFrame() const { return maximumFrame_; }

private:
    void loadAllTileConfigurations();
    std::shared_ptr<TiledEntry> entry_;
    IntervalTree<unsigned int> intervalTree_;

public: // For sake of measuring.
    std::unordered_map<int, std::experimental::filesystem::path> directoryIdToTileDirectory_;
    std::unordered_map<int, std::shared_ptr<const TileLayout>> directoryIdToTileLayout_;
    std::unordered_map <TileLayout, std::shared_ptr<const TileLayout>> tileLayoutReferences_;

private:
    mutable std::mutex mutex_;

    unsigned int totalWidth_;
    unsigned int totalHeight_;
    unsigned int largestWidth_;
    unsigned int largestHeight_;
    unsigned int maximumFrame_;
};

} // namespace tasm

#endif //TASM_TILEDVIDEOMANAGER_H
