#include "WorkloadCostEstimator.h"

#include "SemanticDataManager.h"

namespace tasm {

CostElements WorkloadCostEstimator::estimateCostForQuery(unsigned int queryNum, std::unordered_map<unsigned int, CostElements> *costByGOP) {
    auto semanticDataManager = workload_->semanticDataManagerForQuery(queryNum);
    auto start = semanticDataManager->orderedFrames().begin();
    auto end = semanticDataManager->orderedFrames().end();

    unsigned long long totalNumberOfPixels = 0;
    unsigned long long totalNumberOfTiles = 0;

    while (start != end) {
        auto costElements = estimateCostForNextGOP(start, end, semanticDataManager);
        totalNumberOfPixels += costElements.second.numPixels;
        totalNumberOfTiles += costElements.second.numTiles;

        if (costByGOP)
            costByGOP->emplace(costElements);
    }

    auto multiplier = workload_->numberOfTimesQueryIsExecuted(queryNum);
    return CostElements(multiplier * totalNumberOfPixels, multiplier * totalNumberOfTiles);
}

CostElements WorkloadCostEstimator::estimateCostForWorkload() {
    CostElements results(0, 0);
    for (auto i = 0u; i < workload_->numberOfQueries(); ++i) {
        auto queryResults = estimateCostForQuery(i);
        results.add(queryResults);
    }
    return results;
}

std::pair<int, CostElements> WorkloadCostEstimator::estimateCostForNextGOP(std::vector<int>::const_iterator &currentFrame,
                                                                           std::vector<int>::const_iterator end,
                                                                           std::shared_ptr<SemanticDataManager> metadataManager) {
    if (currentFrame == end)
        return std::make_pair(-1, CostElements(0, 0));

    auto gopNum = gopForFrame(*currentFrame);
    auto keyframe = keyframeForFrame(*currentFrame);
    auto layoutForGOP = tileLayoutProvider_->tileLayoutForFrame(*currentFrame);

    // Find the frames that have an object overlapping the tiles.
    auto numberOfTiles = layoutForGOP->numberOfTiles();
    std::vector<int> maxFrameOverlappingTile(numberOfTiles, -1);
    while (currentFrame != end && gopForFrame(*currentFrame) == gopNum) {
        auto &rectanglesForFrame = metadataManager->rectanglesForFrame(*currentFrame);
        for (auto i = 0u; i < numberOfTiles; ++i) {
            auto tileRect = layoutForGOP->rectangleForTile(i);
            bool anyIntersect = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rectangle) {
                return tileRect.intersects(rectangle);
            });
            if (anyIntersect)
                maxFrameOverlappingTile[i] = *currentFrame;
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
        totalNumPixels += layoutForGOP->rectangleForTile(i).area() * numTiles;
    }
    return std::make_pair(gopNum, CostElements(totalNumPixels, totalNumTiles));
}

} // namespace tasm