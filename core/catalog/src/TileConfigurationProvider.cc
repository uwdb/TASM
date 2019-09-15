#include "TileConfigurationProvider.h"

#include "Files.h"
#include "Gpac.h"

namespace lightdb::tiles {

void TileLayoutsManager::loadAllTileConfigurations() {
    // Get directory path from entry_.
    auto &catalogEntryPath = entry_.path();

    // Read all directory names.
    for (auto &dir : std::filesystem::directory_iterator(catalogEntryPath)) {
        if (!dir.is_directory())
            continue;

        auto tileDirectoryPath = dir.path();
        // Parse frame range from path.
        Interval firstAndLastFrame = catalog::TileFiles::firstAndLastFramesFromPath(tileDirectoryPath);

        // Find the tile-metadata file in this directory, and load the tile layout from it.
        TileLayout tileLayout = video::gpac::load_tile_configuration(catalog::TileFiles::tileMetadataFilename(tileDirectoryPath));

        intervalToAvailableTileLayouts_[firstAndLastFrame].push_back(tileLayout);
        numberOfTilesToFrameIntervals_[tileLayout.numberOfTiles()].addInterval(firstAndLastFrame);
        intervalToTileDirectory_[firstAndLastFrame] = tileDirectoryPath;
    }
}

std::list<TileLayout> TileLayoutsManager::tileLayoutsForFrame(unsigned int frameNumber) const {
    // Find the intervals that contain the frame number, and splice their lists together.
    std::list<TileLayout> layouts;
    for (auto it = intervalToAvailableTileLayouts_.begin(); it != intervalToAvailableTileLayouts_.end(); ++it) {
        if (it->first.contains(frameNumber))
            layouts.insert(layouts.end(), it->second.begin(), it->second.end());
    }

    assert(!layouts.empty());
    return layouts;
}

unsigned int TileLayoutsManager::maximumNumberOfTilesForFrames(const std::vector<int> &orderedFrames) const {
    assert(orderedFrames.size());

    for (auto it = numberOfTilesToFrameIntervals_.begin(); it != numberOfTilesToFrameIntervals_.end(); ++it) {
        // Only consider frames that fall within the min/max of the intervals.
        // Find iterator in orderedFrames to first frame that falls within range of the interval.
        // Find iterator in orderedFrames to last frame that falls within the range of the interval.
        // Only check if any interval contains those frames.
        auto framesExtent = it->second.extents();
        auto firstFrameToConsider = std::lower_bound(orderedFrames.begin(), orderedFrames.end(), framesExtent.start());
        auto lastFrameToConsider = std::upper_bound(orderedFrames.begin(), orderedFrames.end(), framesExtent.end());
        for (auto frameIt = firstFrameToConsider; frameIt != lastFrameToConsider; ++frameIt) {
            if (it->second.contains(*frameIt))
                return it->first;
        }
    }

    // Each frame should be covered by at least one tile layout.
    assert(false);
    return 0;

//    unsigned int currentMax = 0;
//    for (auto &frame : frames) {
//        auto layouts = tileLayoutsForFrame(frame);
//        auto maxNumberOfTiles = std::max_element(layouts.begin(), layouts.end(), [](TileLayout &first, TileLayout &second) {
//            return first.numberOfTiles() < second.numberOfTiles();
//        })->numberOfTiles();
//        if (maxNumberOfTiles > currentMax)
//            currentMax = maxNumberOfTiles;
//    }
//
//    return currentMax;
}

std::filesystem::path TileLayoutsManager::locationOfTileForFrameAndConfiguration(unsigned int tileNumber,
                                                                                 unsigned int frame,
                                                                                 const lightdb::tiles::TileLayout &tileLayout) const {
    // TODO: Figure out how to go from layout -> intervals -> directory.
    // For now, assume that each frame is covered by a single tile configuration.
    // TODO: Make finding correct interval faster.
    for (auto it = intervalToTileDirectory_.begin(); it != intervalToTileDirectory_.end(); ++it) {
        if (it->first.contains(frame)) {
            return catalog::TileFiles::tileFilename(it->second, tileNumber);
        }
    }
    // We should always find it.
    assert(false);
    return {};
}


} // namespace lightdb::tiles