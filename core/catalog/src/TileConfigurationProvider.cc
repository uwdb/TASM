#include "TileConfigurationProvider.h"

#include "Files.h"
#include "Gpac.h"

namespace lightdb::tiles {

enum class Direction {
    BACKWARDS,
    FORWARDS
};

unsigned int align(unsigned int number, Direction direction=Direction::FORWARDS, unsigned int alignment=32) {
    if (!(number % alignment))
        return number;

    auto multiples = int(number / alignment);
    if (direction == Direction::BACKWARDS)
        return multiples * alignment;
    else
        return (multiples + 1) * alignment;
}

void ROITileConfigurationProvider::makeLayout() {
    const unsigned int minWidth = 256;
    const unsigned int minHeight = 136;

    std::vector<unsigned int> widths;
    auto left = align(roi_.x1, Direction::BACKWARDS);
    if (left >= minWidth)
        widths.push_back(left);
    auto xComp = widths.empty() ? 0 : widths[0];
    auto right = std::min(totalWidth_, align(roi_.x2, Direction::FORWARDS));
    if ((right - xComp) >= minWidth && (totalWidth_ - right) >= minWidth)
        widths.push_back(right - xComp);
    auto accountedWidth = std::accumulate(widths.begin(), widths.end(), 0);
    if (accountedWidth < totalWidth_)
        widths.push_back(totalWidth_ - accountedWidth);

    std::vector<unsigned int> heights;
    auto top = align(roi_.y1, Direction::BACKWARDS);
    if (top >= minHeight)
        heights.push_back(top);
    auto yComp = heights.empty() ? 0 : heights[0];
    auto bottom = std::min(totalHeight_, align(roi_.y2, Direction::FORWARDS));
    if ((bottom - yComp) >= minHeight && (totalHeight_ - bottom) >= minHeight)
        heights.push_back(bottom - yComp);
    auto accountedHeight = std::accumulate(heights.begin(), heights.end(), 0);
    if (accountedHeight < totalHeight_)
        heights.push_back(totalHeight_ - accountedHeight);

    layout_ = std::make_unique<TileLayout>(widths.size(), heights.size(), widths, heights);
}

void TileLayoutsManager::loadAllTileConfigurations() {
    std::scoped_lock lock(mutex_);

    // Get directory path from entry_.
    auto &catalogEntryPath = entry_.path();

    // Read all directory names.
    int globalDirId = 0;
    std::vector<IntervalEntry<unsigned int>> directoryIntervals;
    unsigned int lowerBound = INT32_MAX;
    unsigned int upperBound = 0;

    hasASingleTile_ = true;
    for (auto &dir : std::filesystem::directory_iterator(catalogEntryPath)) {
        if (!dir.is_directory())
            continue;

        auto dirId = globalDirId++;
        auto tileDirectoryPath = dir.path();
        // Parse frame range from path.
        auto firstAndLastFrame = catalog::TileFiles::firstAndLastFramesFromPath(tileDirectoryPath);
        auto tileVersion = catalog::TileFiles::tileVersionFromPath(tileDirectoryPath);
        dirId = tileVersion;
        if (hasASingleTile_ && dirId)
            hasASingleTile_ = false;

        directoryIntervals.emplace_back(firstAndLastFrame.first, firstAndLastFrame.second, dirId);
        if (firstAndLastFrame.second > maximumFrame_)
            maximumFrame_ = firstAndLastFrame.second;

        lowerBound = std::min(lowerBound, firstAndLastFrame.first);
        upperBound = std::max(upperBound, firstAndLastFrame.second);

        // Find the tile-metadata file in this directory, and load the tile layout from it.
        TileLayout tileLayout = video::gpac::load_tile_configuration(catalog::TileFiles::tileMetadataFilename(tileDirectoryPath));

        if (hasASingleTile_ && tileLayout.numberOfTiles() > 1)
            hasASingleTile_ = false;

        // All of the layouts should have the same total width and total height.
        if (!totalWidth_) {
            totalWidth_ = tileLayout.totalWidth();
            totalHeight_ = tileLayout.totalHeight();
        }

        largestWidth_ = std::max(largestWidth_, tileLayout.largestWidth());
        largestHeight_ = std::max(largestHeight_, tileLayout.largestHeight());

        if (!tileLayoutReferences_.count(tileLayout))
            tileLayoutReferences_[tileLayout] = std::make_shared<const TileLayout>(tileLayout);

        directoryIdToTileLayout_[dirId] = tileLayoutReferences_.at(tileLayout);
        directoryIdToTileDirectory_[dirId] = tileDirectoryPath;
    }

    intervalTree_ = IntervalTree<unsigned int>(lowerBound, upperBound, directoryIntervals);
}

std::vector<int> TileLayoutsManager::tileLayoutIdsForFrame(unsigned int frameNumber) const {
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

std::filesystem::path TileLayoutsManager::locationOfTileForId(unsigned int tileNumber, int id) const {
    std::scoped_lock lock(mutex_);
    return catalog::TileFiles::tileFilename(directoryIdToTileDirectory_.at(id), tileNumber);
}

static std::vector<unsigned int> tileDimensionsForIntervals(const CoalescedIntervals<unsigned int> &intervals, unsigned int minimumDistance, unsigned int totalDimension) {
    std::vector<unsigned int> dimensions;
    auto startPosition = 0u;

    auto addNewDelimiter = [&](unsigned int newOffset) {
        dimensions.push_back(newOffset - startPosition);
        startPosition = newOffset;
    };

    // TODO: Dimensions have to be an even number.
    for (const auto &interval : intervals.intervals()) {
        // If iterator.start - start_position >= minimum distance, we can make that range a tile.
        if (interval.start() > startPosition && interval.start() - startPosition >= minimumDistance) {
            if (totalDimension - interval.start() < minimumDistance) {
                // There isn't room for another tile starting at interval.start().
                // See if we can go back further and still be ok given the current starting position.
                if ((totalDimension - minimumDistance) - startPosition >= minimumDistance)
                    addNewDelimiter(totalDimension - minimumDistance);
            } else if (interval.start() > startPosition)
                addNewDelimiter(interval.start());
        }

        // We don't want to use something less than the end of the interval else the object would be cut into two tiles.
        // Also don't go past the end of the frame. If the end is close enough to the edge of the frame, just extend the tile
        // to the edge.
        if (interval.end() > startPosition && interval.end() - startPosition >= minimumDistance && totalDimension - interval.end() >= minimumDistance) {
            addNewDelimiter(interval.end());
        } else if (totalDimension - (startPosition + minimumDistance) >= minimumDistance) {
            // The end of the interval isn't far enough away, so go past it such that we satisfy the minimum distance.
            addNewDelimiter(startPosition + minimumDistance);
        }
    }

    // Add width/height from total dimension to last offset.
    dimensions.push_back(totalDimension - startPosition);

    unsigned int total = std::accumulate(dimensions.begin(), dimensions.end(), 0);
    if (total > totalDimension)
        assert(false);

    return dimensions;
}

const TileLayout &GroupingExtentsTileConfigurationProvider::tileLayoutForFrame(unsigned int frame) {
    unsigned int tileLayoutIntervalForFrame = frame / tileLayoutDuration_;
    if (layoutIntervalToTileLayout_.count(tileLayoutIntervalForFrame))
        return layoutIntervalToTileLayout_.at(tileLayoutIntervalForFrame);

    // Get rectangles that are in this duration.
    auto startFrame = tileLayoutIntervalForFrame * tileLayoutDuration_;
    auto endFrame = (tileLayoutIntervalForFrame + 1) * tileLayoutDuration_;
    std::unique_ptr<std::list<Rectangle>> rectanglesForLayoutIntervalPtr = shouldConsiderAllObjectsInFrames_
            ? metadataManager_->rectanglesForAllObjectsForFrames(startFrame, endFrame)
            : metadataManager_->rectanglesForFrames(startFrame, endFrame);
    auto &rectanglesForLayoutInterval = *rectanglesForLayoutIntervalPtr;

    // Combine all of the rectangles into one giant one that contains all of them.
    if (rectanglesForLayoutInterval.size()) {
        auto superRectangle = rectanglesForLayoutInterval.front();
        for (auto it = std::next(rectanglesForLayoutInterval.begin(), 1); it != rectanglesForLayoutInterval.end(); ++it) {
            superRectangle.expand(*it);
        }


        auto tileHeights = tileDimensions(Interval<int>(superRectangle.y, superRectangle.y + superRectangle.height), 136, frameHeight_);
        auto tileWidths = tileDimensions(Interval<int>(superRectangle.x, superRectangle.x + superRectangle.width), 256, frameWidth_);

        layoutIntervalToTileLayout_.emplace(std::pair<unsigned int, TileLayout>(
                std::piecewise_construct,
                std::forward_as_tuple(tileLayoutIntervalForFrame),
                std::forward_as_tuple(tileWidths.size(), tileHeights.size(), tileWidths, tileHeights)));
    } else {
        layoutIntervalToTileLayout_.emplace(std::pair<unsigned int, TileLayout>(
                std::piecewise_construct,
                std::forward_as_tuple(tileLayoutIntervalForFrame),
                std::forward_as_tuple(1, 1, std::vector<unsigned int>({ frameWidth_ }), std::vector<unsigned int>({ frameHeight_ }))));
    }

    return layoutIntervalToTileLayout_.at(tileLayoutIntervalForFrame);

}

const TileLayout &GroupingTileConfigurationProvider::tileLayoutForFrame(unsigned int frame) {
    unsigned int tileGroupForFrame = frame / tileGroupDuration_;
    if (tileGroupToTileLayout_.count(tileGroupForFrame))
        return tileGroupToTileLayout_.at(tileGroupForFrame);

    // Get rectangles that are in the gop.
    std::unique_ptr<std::list<Rectangle>> rectanglesForGroupPtr = metadataManager_->rectanglesForFrames(tileGroupForFrame * tileGroupDuration_, (tileGroupForFrame + 1) * tileGroupDuration_);
    auto &rectanglesForGroup = *rectanglesForGroupPtr;

    // Compute horizontal and vertical intervals for all of the rectangles.
    std::vector<Interval<int>> horizontalIntervals(rectanglesForGroup.size());
    std::transform(rectanglesForGroup.begin(), rectanglesForGroup.end(), horizontalIntervals.begin(), [](Rectangle &rect) {
        return Interval<int>(rect.x, rect.x + rect.width);
    });
    std::sort(horizontalIntervals.begin(), horizontalIntervals.end());
    auto tileWidths = horizontalIntervals.size() ? tileDimensions(horizontalIntervals, 256, frameWidth_) : std::vector<unsigned int>({ frameWidth_ });

    std::vector<Interval<int>> verticalIntervals(rectanglesForGroup.size());
    std::transform(rectanglesForGroup.begin(), rectanglesForGroup.end(), verticalIntervals.begin(), [](Rectangle &rect) {
        return Interval<int>(rect.y, rect.y + rect.height);
    });
    std::sort(verticalIntervals.begin(), verticalIntervals.end());
    auto tileHeights = verticalIntervals.size() ? tileDimensions(verticalIntervals, 136, frameHeight_) : std::vector<unsigned int>({ frameHeight_ });

    tileGroupToTileLayout_.emplace(std::pair<unsigned int, TileLayout>(
            std::piecewise_construct,
            std::forward_as_tuple(tileGroupForFrame),
            std::forward_as_tuple(tileWidths.size(), tileHeights.size(), tileWidths, tileHeights)));

    return tileGroupToTileLayout_.at(tileGroupForFrame);
}

std::vector<unsigned int> GroupingExtentsTileConfigurationProvider::tileDimensions(Interval<int> interval, int minDistance, int totalDimension) {
    std::vector<int> offsets;
    int lastOffset = 0;

    auto getAlignedOffset = [](int offset, int lastOffset, bool shouldMoveBackwards) {
        unsigned int alignment = 32;
        if (!((offset - lastOffset) % alignment))
            return offset;

        int base = (offset - lastOffset) / alignment * alignment;
        if (!shouldMoveBackwards)
            base += alignment;

        return lastOffset + base;
    };

    auto start = getAlignedOffset(interval.start(), lastOffset, true);
    if (start >= minDistance) {
        if (totalDimension - start >= minDistance) {
            offsets.push_back(start);
            lastOffset = start;
        } else if (totalDimension - minDistance >= minDistance) {
            offsets.push_back(totalDimension - minDistance);
            lastOffset = totalDimension - minDistance;
        }
    }

    auto end = getAlignedOffset(interval.end(), lastOffset, false);
    if (end - lastOffset >= minDistance) {
        if (totalDimension - end >= minDistance)
            offsets.push_back(end);
    } else if (lastOffset + minDistance >= end) {
        if (totalDimension - (lastOffset + minDistance) >= minDistance)
            offsets.push_back(lastOffset + minDistance);
    }

    if (offsets.empty())
        return { static_cast<unsigned int>(totalDimension) };

    std::vector<unsigned int> dimensions(offsets.size() + 1);
    dimensions[0] = offsets[0];
    for (auto i = 1u; i < offsets.size(); ++i) {
        assert(offsets[i] > offsets[i-1]);
        dimensions[i] = offsets[i] - offsets[i - 1];
    }
    assert(totalDimension > offsets.back());
    dimensions.back() = totalDimension - offsets.back();

    return dimensions;
}

std::vector<unsigned int> GroupingTileConfigurationProvider::tileDimensions(const std::vector<lightdb::Interval<int>> &sortedIntervals, int minDistance, int totalDimension) {
    std::vector<int> offsets;
    std::vector<int> intervalEnds;
    int lastOffset = 0;

    auto doesOffsetStrictlyIntersect = [&](int proposedOffset, unsigned int intervalIndex) {
        for (auto i = 0u; i < sortedIntervals.size(); ++i) {
            if (i == intervalIndex)
                continue;

            // Don't use the normal definition of "contains" because we don't want to consider the case where proposed == start as containment.
            auto &interval = sortedIntervals[i];
            if (proposedOffset > interval.start() && proposedOffset < interval.end()) {
                return true;
            }
        }

        return false;
    };

    auto tryAppend = [&](int offset, bool tryToMoveBack = false, unsigned int intervalIndex = 0) {
        if (!intervalEnds.size() || offset >= intervalEnds.back()) {
            if (totalDimension - offset >= minDistance) {
                offsets.push_back(offset);
                lastOffset = offset;
            } else if (tryToMoveBack && totalDimension - minDistance - lastOffset >= minDistance && !doesOffsetStrictlyIntersect(totalDimension - minDistance, intervalIndex)) {
                auto newOffset = totalDimension - minDistance;
                offsets.push_back(newOffset);
                lastOffset = newOffset;
            }
        }
    };

    auto getAlignedOffset = [](int offset, int lastOffset, bool shouldMoveBackwards) {
        unsigned int alignment = 32;
        if (!((offset - lastOffset) % alignment))
            return offset;

        int base = (offset - lastOffset) / alignment * alignment;
        if (!shouldMoveBackwards)
            base += alignment;

        return lastOffset + base;
    };

    for (auto index = 0u; index < sortedIntervals.size(); ++index) {
        auto &interval = sortedIntervals[index];
        bool isLastInterval = index == sortedIntervals.size() - 1;
        auto offset = getAlignedOffset(interval.start(), lastOffset, true);
        if (offset - lastOffset >= minDistance && !doesOffsetStrictlyIntersect(offset, index))
            tryAppend(offset, true, index);

        offset = getAlignedOffset(interval.end(), lastOffset, false);
        if (offset != lastOffset) {
            if (offset - lastOffset >= minDistance and totalDimension - offset >= minDistance) {
                if (!doesOffsetStrictlyIntersect(offset, index)) {
                    tryAppend(offset);
                }
            } else if (!isLastInterval
                       //                    && lastOffset != offset
                       && lastOffset + minDistance >= interval.end()
                       && sortedIntervals[index + 1].start() - (lastOffset + minDistance) >= minDistance) {
                tryAppend(lastOffset + minDistance); // Issue when offset == lastOffset, it will come in here.
            } else if (isLastInterval
                       && lastOffset < interval.end()
                       && totalDimension - (lastOffset + minDistance) >= minDistance
                       && lastOffset + minDistance >= interval.end()) {
                tryAppend(lastOffset + minDistance);
            }
        }
        intervalEnds.push_back(offset);
    }

    // Now transform offsets into widths/heights.
//    assert(offsets.size()); // Offsets may be empty if no tiles should be added in a given dimension.
    if (offsets.empty())
        return { static_cast<unsigned int>(totalDimension) };

    std::vector<unsigned int> dimensions(offsets.size() + 1);
    dimensions[0] = offsets[0];
    for (auto i = 1u; i < offsets.size(); ++i) {
        assert(offsets[i] > offsets[i-1]);
        dimensions[i] = offsets[i] - offsets[i - 1];
    }
    assert(totalDimension > offsets.back());
    dimensions.back() = totalDimension - offsets.back();

//    for (auto &dimension : dimensions) {
//        assert(!(dimension % 2));
//    }

    return dimensions;
}

//void MultiTileLocationProvider::insertTileLayoutForTileGroup(TileLayout &layout, unsigned int frame) const {
//    std::scoped_lock lock(mutex_);
//
//    if (!tileLayoutReferences_.count(layout))
//        tileLayoutReferences_[layout] = std::make_shared<const TileLayout>(layout);
//
//    tileGroupToTileLayout_[frame] = tileLayoutReferences_.at(layout);
//}

//const TileLayout &MultiTileLocationProvider::tileLayoutForFrame(unsigned int frame) const {
//    std::scoped_lock lock(mutex_);
//
//    auto got = tileGroupForFrame(frame);
//    if (tileGroupToTileLayout_.count(got))
//        return *tileGroupToTileLayout_.at(got);
//
//    auto layouts = tileLayoutsManager_->tileLayoutsForFrame(frame);
//
//    if (layouts.size() == 1) {
//        insertTileLayoutForTileGroup(layouts.front(), got);
//        return *tileGroupToTileLayout_.at(got);
//    }
//
//    // Else we have to pick between the layouts.
//    // Get the boxes that are in this GOT from the metadata manager.
//    // Find the overlapping rectangles so we can compare those vs. the tile layouts.
//    // TODO: This should be considering the rectangles within each layout interval, which may not be the same.
//    RectangleMerger rectangleMerger(metadataManager_->rectanglesForFrames(got * tileGroupSize_, (got + 1) * tileGroupSize_));
//
//    // For each layout:
//        // For each merged rectangle, compute which tiles intersect it.
//        // Compute the difference in area between the intersecting tiles and the rectangle.
//        // The cost for the layout is the sum of these costs over all rectangles.
//    unsigned int minCost = UINT32_MAX;
//    auto bestLayoutIt = layouts.end();
//    for (auto layoutIt = layouts.begin(); layoutIt != layouts.end(); ++layoutIt) {
//        unsigned int layoutCost = 0;
//        for (auto &rectangle : rectangleMerger.rectangles()) {
//            auto intersectingTiles = layoutIt->tilesForRectangle(rectangle);
//            auto areaOfIntersectingTiles = 0u;
//            for (auto &intersectingTile : intersectingTiles)
//                areaOfIntersectingTiles += layoutIt->rectangleForTile(intersectingTile).area();
//
//            layoutCost += areaOfIntersectingTiles;
//            layoutCost -= rectangle.area();
//
//            // Cost will only go up.
//            if (layoutCost > minCost)
//                break;
//        }
//
//        if (layoutCost < minCost) {
//            minCost = layoutCost;
//            bestLayoutIt = layoutIt;
//        }
//    }
//
//    // Pick the layout with the lowest cost.
//    insertTileLayoutForTileGroup(*bestLayoutIt, got);
//    return *tileGroupToTileLayout_.at(got);
//}

} // namespace lightdb::tiles