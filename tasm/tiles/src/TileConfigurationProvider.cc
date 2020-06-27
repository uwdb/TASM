#include "TileConfigurationProvider.h"

#include "SemanticDataManager.h"

namespace tasm {

std::vector<unsigned int> FineGrainedTileConfigurationProvider::tileDimensions(const std::vector<interval::Interval<int>> &sortedIntervals, int minDistance, int totalDimension) {
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

    return dimensions;
}

std::shared_ptr<const TileLayout> FineGrainedTileConfigurationProvider::tileLayoutForFrame(unsigned int frame) {
    unsigned int tileGroupForFrame = frame / tileLayoutDuration_;
    if (tileGroupToTileLayout_.count(tileGroupForFrame))
        return tileGroupToTileLayout_.at(tileGroupForFrame);

    // Get rectangles that are in the tile group.
    auto firstFrameInGroup = tileGroupForFrame * tileLayoutDuration_;
    auto lastFrameInGroupExclusive = (tileGroupForFrame + 1) * tileLayoutDuration_;
    auto rectanglesForGroup = semanticDataManager_->rectanglesForFrames(firstFrameInGroup, lastFrameInGroupExclusive);

    // Compute horizontal and vertical intervals for all of the rectangles.
    std::vector<interval::Interval<int>> horizontalIntervals(rectanglesForGroup->size());
    std::transform(rectanglesForGroup->begin(), rectanglesForGroup->end(), horizontalIntervals.begin(), [](Rectangle &rect) {
        return interval::Interval<int>(rect.x, rect.x + rect.width);
    });
    std::sort(horizontalIntervals.begin(), horizontalIntervals.end());
    auto tileWidths = horizontalIntervals.size() ? tileDimensions(horizontalIntervals, 256, frameWidth_) : std::vector<unsigned int>({ frameWidth_ });

    std::vector<interval::Interval<int>> verticalIntervals(rectanglesForGroup->size());
    std::transform(rectanglesForGroup->begin(), rectanglesForGroup->end(), verticalIntervals.begin(), [](Rectangle &rect) {
        return interval::Interval<int>(rect.y, rect.y + rect.height);
    });
    std::sort(verticalIntervals.begin(), verticalIntervals.end());
    auto tileHeights = verticalIntervals.size() ? tileDimensions(verticalIntervals, 160, frameHeight_) : std::vector<unsigned int>({ frameHeight_ });

    tileGroupToTileLayout_[tileGroupForFrame] = std::make_shared<const TileLayout>(tileWidths.size(), tileHeights.size(), tileWidths, tileHeights);
    return tileGroupToTileLayout_.at(tileGroupForFrame);
}

} // namespace tasm