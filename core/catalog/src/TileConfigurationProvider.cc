#include "TileConfigurationProvider.h"

#include "Files.h"
#include "Gpac.h"

namespace lightdb::tiles {

void TileLayoutsManager::loadAllTileConfigurations() {
    std::scoped_lock lock(mutex_);

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
        tileDirectoryToLayout_[tileDirectoryPath] = tileLayout;
    }
}

std::list<TileLayout> TileLayoutsManager::tileLayoutsForFrame(unsigned int frameNumber) const {
    std::scoped_lock lock(mutex_);

    // Find the intervals that contain the frame number, and splice their lists together.
    std::list<TileLayout> layouts;

    // Returns the first interval whose start is > frameNumber. Pass UINT32_MAX as the end to guarantee that the start of the returned
    // interval is greater than the frame number we are looking for.
    auto it = intervalToAvailableTileLayouts_.upper_bound(Interval(frameNumber, UINT32_MAX));
    if (it == intervalToAvailableTileLayouts_.end())
        std::advance(it, -1);

    // Once the interval's end is less than the frame number, the frame cannot be in that interval.
    // This breaks for the case where we have one large interval that covers e.g. 0->max, but then shorter intervals between.
    for (; ; --it) {
        if (it->first.contains(frameNumber))
            layouts.insert(layouts.end(), it->second.begin(), it->second.end());

        if (it == intervalToAvailableTileLayouts_.begin())
            break;
    }

    assert(!layouts.empty());
    return layouts;
}

unsigned int TileLayoutsManager::maximumNumberOfTilesForFrames(const std::vector<int> &orderedFrames) const {
    std::scoped_lock lock(mutex_);

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
}

std::filesystem::path TileLayoutsManager::locationOfTileForFrameAndConfiguration(unsigned int tileNumber,
                                                                                 unsigned int frame,
                                                                                 const lightdb::tiles::TileLayout &tileLayout) const {
    std::scoped_lock lock(mutex_);

    // TODO: Figure out how to go from layout -> intervals -> directory.
    // For now, assume that each frame is covered by a single tile configuration.
    // TODO: Make finding correct interval faster.
    auto it = intervalToTileDirectory_.upper_bound(Interval(frame, UINT32_MAX));
    if (it == intervalToTileDirectory_.end())
        std::advance(it, -1);

    // This needs to double check that the tile layout is the same.
    for (; ; --it) {
        if (it->first.contains(frame) && tileDirectoryToLayout_.at(it->second) == tileLayout) {
            return catalog::TileFiles::tileFilename(it->second, tileNumber);
        }

        if (it == intervalToTileDirectory_.begin())
            break;
    }
    // We should always find it.
    assert(false);
    return {};
}

void MetadataTileConfigurationProvider::computeTileConfigurations() {
    // Look at the pixels that are requested in each GOP.
    // Means that the frames for metadata have to be split up by GOP.
    // We know that by the way we set up our keyframes, all frames with an object will be sequential starting from a keyframe.


}

void MetadataTileConfigurationProvider::pickKeyframes() {
    /*
     * Use the keyframes that are picked such that every sequence of objects in the metadata
     * start with a keyframe.
     * Also include the first frame that we are considering in case it is not picked to be a keyframe when
     * all frames in the video are considered.
     * This is maybe not ideal because you could end up with keyframes very close together. For example, if og keyframes
     * are {15, 30, 90}, and first frame is 13, you would end up with keyframes of {13, 15, 30, 60, 90}.
     */

    // Find ideal keyframes within the frames to consider, then tile each GOP.
    // If a GOP is large (>30 frames), split it up.
    auto unorderedKeyframes = metadataManager_->idealKeyframesForMetadata();
    for (auto &keyframe : unorderedKeyframes) {
        if (static_cast<unsigned int>(keyframe) >= firstFrameToConsider_ && static_cast<unsigned int>(keyframe) <= lastFrameToConsider_)
            keyframes_.insert(keyframe);
    }
    keyframes_.insert(firstFrameToConsider_);
    assert(keyframes_.size());

    // Go through the keyframes. If any two are >= n apart, insert a keyframe that is += n.
    static int maximumGOP = 30;
    auto keyframeIt = keyframes_.begin();
    auto previousKeyframeValue = *keyframeIt++;
    while (keyframeIt != keyframes_.end()) {
        if (*keyframeIt - previousKeyframeValue > maximumGOP) {
            previousKeyframeValue += maximumGOP;
            keyframeIt = keyframes_.insert(keyframeIt, previousKeyframeValue)++;
        } else {
            previousKeyframeValue = *keyframeIt++;
        }
    }
}

void MetadataTileConfigurationProvider::setUpGOPs() {
    assert(keyframes_.size());
    auto numberOfGOPs = keyframes_.size();
    gopFramesThatContainObject_.resize(numberOfGOPs);

    // Walk through the frames for the metadata.
    auto &orderedFrames = metadataManager_->orderedFramesForMetadata();

    // Find the first frame that is within the range we are considering and contains an object.
    auto frameIt = std::lower_bound(orderedFrames.begin(), orderedFrames.end(), firstFrameToConsider_);
    auto keyframeIt = keyframes_.begin();
    ++keyframeIt;
    for (auto i = 0u; i < numberOfGOPs; ++i, ++keyframeIt) {
        // If there are no more keyframes, add the rest of the frames.
        if (keyframeIt == keyframes_.end()) {
            while (frameIt != orderedFrames.end() && static_cast<unsigned int>(*frameIt) <= lastFrameToConsider_)
                gopFramesThatContainObject_[i].push_back(*frameIt++);
        } else {
            while (frameIt != orderedFrames.end() && *frameIt <= *keyframeIt && static_cast<unsigned int>(*frameIt) <= lastFrameToConsider_)
                gopFramesThatContainObject_[i].push_back(*frameIt++);
        }

        // No more frames will be considered because they are all past the last frame to consider.
        if (frameIt != orderedFrames.end() && static_cast<unsigned int>(*frameIt) > lastFrameToConsider_)
            break;
    }
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

const TileLayout &IdealTileConfigurationProvider::tileLayoutForFrame(unsigned int frame) {
    static constexpr unsigned int const &minimumTileWidth = 256; // The spec says minimum width is 256, but decoder can do >= 136.
    static constexpr unsigned int const &minimumTileHeight = 136; // The spec says that the minimum height is 64, but the decoder can only decode >= 136.
    static unsigned int maximumNumberOfTiles = 0;

    if (frameToTileLayout_.count(frame))
        return frameToTileLayout_.at(frame);

    // Get rectangles for frame.
    auto &rects = metadataManager_->rectanglesForFrame(frame);
    if (!rects.size())
        return EmptyTileLayout;

    // For each dimension, create intervals from rectangles.
    CoalescedIntervals<unsigned int> verticalRectIntervals;
    CoalescedIntervals<unsigned int> horizontalRectIntervals;

    for (auto &rect : rects) {
        verticalRectIntervals.addInterval({rect.y, std::min(rect.y + rect.height, frameHeight_)});
        horizontalRectIntervals.addInterval({rect.x, std::min(rect.x + rect.width, frameWidth_)});
    }

    // Compute tile dimensions given the intervals.
    auto rowHeights = tileDimensionsForIntervals(verticalRectIntervals, minimumTileHeight, frameHeight_);
    auto columnWidths = tileDimensionsForIntervals(horizontalRectIntervals, minimumTileWidth, frameWidth_);

    TileLayout layoutForFrame(columnWidths.size(), rowHeights.size(), columnWidths, rowHeights);

    for (auto &rect : rects) {
        auto tilesThatIntersect = layoutForFrame.tilesForRectangle(rect);
//        assert(tilesThatIntersect.size() == 1);
    }
    if (layoutForFrame.numberOfTiles() > maximumNumberOfTiles)
        maximumNumberOfTiles = layoutForFrame.numberOfTiles();

    frameToTileLayout_.emplace(frame, layoutForFrame);
//    frameToTileLayout_.emplace(std::pair<unsigned int, TileLayout>(std::piecewise_construct,
//                                std::forward_as_tuple(frame),
//                                std::forward_as_tuple(columnWidths.size(), rowHeights.size(), columnWidths, rowHeights)));
    return frameToTileLayout_.at(frame);
}

const TileLayout &CustomJacksonSquareTileConfigurationProvider::tileLayoutForFrame(unsigned int frame) {
    // total dimensions are 640 x 512.
    static TileLayout zeroToNine(2, 2, {226, 414}, {136, 376});
    static TileLayout tenTo89(2, 2, {175, 465}, {136, 376});
    static TileLayout eightyNineTo99(1, 2, {640}, {230, 282});
    static TileLayout oneHundredTo103(2, 2, {230, 410}, {216, 296});
    static TileLayout one04To105(1, 2, {640}, {180, 332});
    static TileLayout one06To116(2, 2, {320, 320}, {136, 376});
    static TileLayout one17To126(2, 1, {360, 280}, {512});
    static TileLayout one27To130(2, 2, {400, 240}, {372, 140});
    static TileLayout one31To139(2, 1, {412, 228}, {512});
    static TileLayout one40To159(2, 2, {504, 136}, {376, 136});
    static TileLayout one60To165(1, 2, {640}, {300, 212});
    static TileLayout one66To168(1, 1, {640}, {512});
    static TileLayout one69To170(2, 1, {140, 500}, {512});


    return zeroToNine;
}

const TileLayout &GroupingTileConfigurationProvider::tileLayoutForFrame(unsigned int frame) {
    unsigned int gopForFrame = frame / gop_;
    if (gopToTileLayout_.count(gopForFrame))
        return gopToTileLayout_.at(gopForFrame);

    // Get rectangles that are in the gop.
    std::unique_ptr<std::list<Rectangle>> rectanglesForGOPPtr = metadataManager_->rectanglesForFrames(gopForFrame * gop_, (gopForFrame + 1) * gop_);
    auto &rectanglesForGOP = *rectanglesForGOPPtr;

    // Compute horizontal and vertical intervals for all of the rectangles.
    std::vector<Interval<int>> horizontalIntervals(rectanglesForGOP.size());
    std::transform(rectanglesForGOP.begin(), rectanglesForGOP.end(), horizontalIntervals.begin(), [](Rectangle &rect) {
        return Interval<int>(rect.x, rect.x + rect.width);
    });
    std::sort(horizontalIntervals.begin(), horizontalIntervals.end());
    auto tileWidths = horizontalIntervals.size() ? tileDimensions(horizontalIntervals, 256, frameWidth_) : std::vector<unsigned int>({ frameWidth_ });

    std::vector<Interval<int>> verticalIntervals(rectanglesForGOP.size());
    std::transform(rectanglesForGOP.begin(), rectanglesForGOP.end(), verticalIntervals.begin(), [](Rectangle &rect) {
        return Interval<int>(rect.y, rect.y + rect.height);
    });
    std::sort(verticalIntervals.begin(), verticalIntervals.end());
    auto tileHeights = verticalIntervals.size() ? tileDimensions(verticalIntervals, 136, frameHeight_) : std::vector<unsigned int>({ frameHeight_ });

    gopToTileLayout_.emplace(std::pair<unsigned int, TileLayout>(
            std::piecewise_construct,
            std::forward_as_tuple(gopForFrame),
            std::forward_as_tuple(tileWidths.size(), tileHeights.size(), tileWidths, tileHeights)));

    return gopToTileLayout_.at(gopForFrame);
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

    for (auto index = 0u; index < sortedIntervals.size(); ++index) {
        auto &interval = sortedIntervals[index];
        bool isLastInterval = index == sortedIntervals.size() - 1;
        if (interval.start() - lastOffset >= minDistance && !doesOffsetStrictlyIntersect(interval.start(), index))
            tryAppend(interval.start(), true, index);

        if (interval.end() - lastOffset >= minDistance and totalDimension - interval.end() >= minDistance) {
            if (!doesOffsetStrictlyIntersect(interval.end(), index)) {
                tryAppend(interval.end());
            }
        } else if (!isLastInterval
                    && lastOffset + minDistance >= interval.end()
                    && sortedIntervals[index + 1].start() - (lastOffset + minDistance) >= minDistance) {
            tryAppend(lastOffset + minDistance);
        } else if (isLastInterval
                    && lastOffset < interval.end()
                    && totalDimension - (lastOffset + minDistance) >= minDistance
                    && lastOffset + minDistance >= interval.end()) {
            tryAppend(lastOffset + minDistance);
        }
        intervalEnds.push_back(interval.end());
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

void MultiTileLocationProvider::insertTileLayoutForGOP(TileLayout &layout, unsigned int gop) const {
    std::scoped_lock lock(mutex_);

    if (!tileLayoutReferences_.count(layout))
        tileLayoutReferences_[layout] = std::make_shared<const TileLayout>(layout);

    gopToTileLayout_[gop] = tileLayoutReferences_.at(layout);
}

const TileLayout &MultiTileLocationProvider::tileLayoutForFrame(unsigned int frame) const {
    std::scoped_lock lock(mutex_);

    auto gop = gopForFrame(frame);
    if (gopToTileLayout_.count(gop))
        return *gopToTileLayout_.at(gop);

    auto layouts = tileLayoutsManager_->tileLayoutsForFrame(frame);

    if (layouts.size() == 1) {
        insertTileLayoutForGOP(layouts.front(), gop);
        return *gopToTileLayout_.at(gop);
    }

    // Else we have to pick between the layouts.
    // Get the boxes that are in this GOP from the metadata manager.
    // Find the overlapping rectangles so we can compare those vs. the tile layouts.
    RectangleMerger rectangleMerger(metadataManager_->rectanglesForFrames(gop * gopSize_, (gop + 1) * gopSize_));

    // For each layout:
        // For each merged rectangle, compute which tiles intersect it.
        // Compute the difference in area between the intersecting tiles and the rectangle.
        // The cost for the layout is the sum of these costs over all rectangles.
    unsigned int minCost = UINT32_MAX;
    auto bestLayoutIt = layouts.end();
    for (auto layoutIt = layouts.begin(); layoutIt != layouts.end(); ++layoutIt) {
        unsigned int layoutCost = 0;
        for (auto &rectangle : rectangleMerger.rectangles()) {
            auto intersectingTiles = layoutIt->tilesForRectangle(rectangle);
            auto areaOfIntersectingTiles = 0u;
            for (auto &intersectingTile : intersectingTiles)
                areaOfIntersectingTiles += layoutIt->rectangleForTile(intersectingTile).area();

            layoutCost += areaOfIntersectingTiles;
            layoutCost -= rectangle.area();

            // Cost will only go up.
            if (layoutCost > minCost)
                break;
        }

        if (layoutCost < minCost) {
            minCost = layoutCost;
            bestLayoutIt = layoutIt;
        }
    }

    // Pick the layout with the lowest cost.
    insertTileLayoutForGOP(*bestLayoutIt, gop);
    return *gopToTileLayout_.at(gop);
}

} // namespace lightdb::tiles