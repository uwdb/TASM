#include "TiledVideoManager.h"

#include "Files.h"
#include "Gpac.h"

namespace tasm {

void TiledVideoManager::loadAllTileConfigurations() {
    std::scoped_lock lock(mutex_);

    // Get directory path from entry_.
    auto &catalogEntryPath = entry_->path();

    // Read all directory names.
    int globalDirId = 0;
    std::vector<IntervalEntry<unsigned int>> directoryIntervals;
    unsigned int lowerBound = INT32_MAX;
    unsigned int upperBound = 0;

    for (auto &dir : std::experimental::filesystem::directory_iterator(catalogEntryPath)) {
//        if (!dir.is_directory())
        if (!std::experimental::filesystem::is_directory(dir.status()))
            continue;

        auto dirId = globalDirId++;
        auto tileDirectoryPath = dir.path();
        // Parse frame range from path.
        auto firstAndLastFrame = TileFiles::firstAndLastFramesFromPath(tileDirectoryPath);
        auto tileVersion = TileFiles::tileVersionFromPath(tileDirectoryPath);
        dirId = tileVersion;

        directoryIntervals.emplace_back(firstAndLastFrame.first, firstAndLastFrame.second, dirId);
        if (firstAndLastFrame.second > maximumFrame_)
            maximumFrame_ = firstAndLastFrame.second;

        lowerBound = std::min(lowerBound, firstAndLastFrame.first);
        upperBound = std::max(upperBound, firstAndLastFrame.second);

        // Find the tile-metadata file in this directory, and load the tile layout from it.
        TileLayout tileLayout = gpac::load_tile_configuration(TileFiles::tileMetadataFilename(tileDirectoryPath));

        // All of the layouts should have the same total width and total height.
        if (!totalWidth_) {
            totalWidth_ = tileLayout.totalWidth();
            totalHeight_ = tileLayout.totalHeight();
        }

        largestWidth_ = std::max(largestWidth_, tileLayout.largestWidth());
        largestHeight_ = std::max(largestHeight_, tileLayout.largestHeight());

        if (!tileLayoutReferences_.count(tileLayout))
            tileLayoutReferences_[tileLayout] = std::make_shared<TileLayout>(tileLayout);

        directoryIdToTileLayout_[dirId] = tileLayoutReferences_.at(tileLayout);
        directoryIdToTileDirectory_[dirId] = tileDirectoryPath;
    }

    intervalTree_ = IntervalTree<unsigned int>(lowerBound, upperBound, directoryIntervals);
}

std::vector<int> TiledVideoManager::tileLayoutIdsForFrame(unsigned int frameNumber) const {
    std::scoped_lock lock(mutex_);

    // Find the intervals that contain the frame number, and splice their lists together.
    std::vector<IntervalEntry<unsigned int>> overlappingIntervals;
    intervalTree_.query(frameNumber, overlappingIntervals);

    std::vector<int> layoutIds(overlappingIntervals.size());
    std::transform(overlappingIntervals.begin(), overlappingIntervals.end(), layoutIds.begin(), [](const auto &intervalEntry) {
        return intervalEntry.id();
    });

    assert(!layoutIds.empty());
    return layoutIds;
}

std::experimental::filesystem::path TiledVideoManager::locationOfTileForId(unsigned int tileNumber, int id) const {
    std::scoped_lock lock(mutex_);
    return TileFiles::tileFilename(directoryIdToTileDirectory_.at(id), tileNumber);
}

} // namespace tasm