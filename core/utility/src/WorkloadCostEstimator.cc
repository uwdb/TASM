#include "WorkloadCostEstimator.h"

namespace lightdb{

WorkloadCostEstimator::CostElements WorkloadCostEstimator::estimateCostForQuery(unsigned int queryNum, unsigned int &sawMultipleLayouts, std::unordered_map<unsigned int, CostElements> *costByGOP) {
    auto metadataManager = workload_.metadataManagerForQuery(queryNum);
    auto start = metadataManager->orderedFramesForMetadata().begin();
    // FIXME: prev() is necessary due to a bug in Select() that caused the last frame to be skipped.
    auto end = std::prev(metadataManager->orderedFramesForMetadata().end(), 1);

    unsigned long long totalNumberOfPixels = 0;
    unsigned long long totalNumberOfTiles = 0;

    while (start != end) {
        auto costElements = estimateCostForNextGOP(start, end, metadataManager, sawMultipleLayouts);
        totalNumberOfPixels += costElements.second.numPixels;
        totalNumberOfTiles += costElements.second.numTiles;

        if (costByGOP)
            costByGOP->emplace(costElements);
    }

    return WorkloadCostEstimator::CostElements(totalNumberOfPixels, totalNumberOfTiles);
}

std::pair<int, WorkloadCostEstimator::CostElements> WorkloadCostEstimator::estimateCostForNextGOP(std::vector<int>::const_iterator &currentFrame,
                                                           std::vector<int>::const_iterator end,
                                                           std::shared_ptr<metadata::MetadataManager> metadataManager,
                                                           unsigned int &sawMultipleLayouts) {
    if (currentFrame == end)
        return std::make_pair(-1, CostElements(0, 0));

    auto gopNum = gopForFrame(*currentFrame);
    auto keyframe = keyframeForFrame(*currentFrame);
    auto layoutForGOP = tileLayoutProvider_->tileLayoutForFrame(*currentFrame);

    // Find the frames that have an object overlapping the tiles.
    auto numberOfTiles = layoutForGOP.numberOfTiles();
    std::vector<int> maxFrameOverlappingTile(numberOfTiles, -1);
    while (currentFrame != end && gopForFrame(*currentFrame) == gopNum) {
        auto &rectanglesForFrame = metadataManager->rectanglesForFrame(*currentFrame);
        for (auto i = 0u; i < numberOfTiles; ++i) {
            auto tileRect = layoutForGOP.rectangleForTile(i);
            bool anyIntersect = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rectangle) {
                return tileRect.intersects(rectangle);
            });
            if (anyIntersect) {
                maxFrameOverlappingTile[i] = *currentFrame;

                if (!firstTileDimensions_)
                    firstTileDimensions_ = std::make_unique<Rectangle>(tileRect);
                else if (!sawMultipleLayouts && !tileRect.hasEqualDimensions(*firstTileDimensions_))
                    sawMultipleLayouts = 1;
            }
        }
        ++currentFrame;
    }

    unsigned int totalNumPixels = 0;
    unsigned int totalNumTiles = 0;
    for (auto i = 0u; i < numberOfTiles; ++i) {
        if (maxFrameOverlappingTile[i] < 0)
            continue;

        unsigned int numTiles = maxFrameOverlappingTile[i] - keyframe + 1;
        totalNumTiles += numTiles;
        totalNumPixels += layoutForGOP.rectangleForTile(i).area() * numTiles;
    }
    return std::make_pair(gopNum, CostElements(totalNumPixels, totalNumTiles));
}

} // namespace lightdb
