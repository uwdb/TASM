//
// Created by Maureen Daum on 2019-10-07.
//

#include "MultiTileOperators.h"

namespace lightdb::physical {

void ScanMultiTileOperator::Runtime::preprocess() {
    // Get all tiles, frames that have to be read.
    // Keep track of dimensions for each tile.

    // Use the same logic as in setUpNextEncodedFrameReader, just don't create the actual reader.
    while (framesIterator_ != endOfFramesIterator_) {
        auto possibleFramesToRead = nextGroupOfFramesWithTheSameLayoutAndFromTheSameFile();
        removeFramesThatDoNotContainObject(possibleFramesToRead);
//        possibleFramesToRead.resize(std::distance(possibleFramesToRead.begin(), lastIntersectingFrameIt));
//
//        if (possibleFramesToRead.empty())
//            continue;

        for (auto tileNumberIt = tileNumberForCurrentLayoutToFrames_.begin(); tileNumberIt != tileNumberForCurrentLayoutToFrames_.end(); ++tileNumberIt) {
            if (tileNumberIt->second.empty())
                continue;
            auto rectangleForTile = currentTileLayout_->rectangleForTile(tileNumberIt->first);
            orderedTileInformation_.emplace_back<TileInformation>(
                    {catalog::TileFiles::tileFilename(currentTilePath_->parent_path(), tileNumberIt->first),
                    static_cast<int>(tileNumberIt->first),
                    rectangleForTile.width,
                    rectangleForTile.height,
                    tileNumberIt->second,
                    tileLocationProvider_->frameOffsetInTileFile(*currentTilePath_),
                    rectangleForTile});
        }
    }

    // Sort so that similar-sized tiles are nearby in processing order.
    std::sort(orderedTileInformation_.begin(), orderedTileInformation_.end());
//    std::reverse(orderedTileInformation_.begin(), orderedTileInformation_.end()); // Sort by descending dimensions.

    orderedTileInformationIt_ = orderedTileInformation_.begin();
}

} // namespace lightdb::physical
