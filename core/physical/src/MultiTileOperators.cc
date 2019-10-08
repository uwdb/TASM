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
        auto lastIntersectingFrameIt = removeFramesThatDoNotContainObject(possibleFramesToRead);
        possibleFramesToRead.resize(std::distance(possibleFramesToRead.begin(), lastIntersectingFrameIt));

        if (possibleFramesToRead.empty())
            continue;

        for (auto tileNumberIt = tileNumbersForCurrentLayout_.begin(); tileNumberIt != tileNumbersForCurrentLayout_.end(); ++tileNumberIt) {
            auto rectangleForTile = currentTileLayout_->rectangleForTile(*tileNumberIt);
            orderedTileInformation_.emplace_back<TileInformation>(
                    {catalog::TileFiles::tileFilename(currentTilePath_->parent_path(), *tileNumberIt),
                    *tileNumberIt,
                    rectangleForTile.width,
                    rectangleForTile.height,
                    possibleFramesToRead,
                    tileLocationProvider_->frameOffsetInTileFile(*currentTilePath_),
                    rectangleForTile});
        }
    }

    std::sort(orderedTileInformation_.begin(), orderedTileInformation_.end());
//    std::reverse(orderedTileInformation_.begin(), orderedTileInformation_.end()); // Sort by descending dimensions.

    orderedTileInformationIt_ = orderedTileInformation_.begin();
}

} // namespace lightdb::physical
