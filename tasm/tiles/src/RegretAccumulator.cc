#include "RegretAccumulator.h"

#include "SemanticDataManager.h"
#include <iostream>

namespace tasm {

static std::string combineStrings(const std::vector<std::string> &strings) {
    static const std::string connector = "_";
    std::string combined = "";
    auto lastIndex = strings.size() - 1;
    for (auto i = 0u; i < strings.size(); ++i) {
        combined += strings[i];
        if (i < lastIndex)
            combined += connector;
    }
    return combined;
}

void RegretAccumulator::addRegretForQuery(std::shared_ptr<Workload> workload,
                                          std::shared_ptr<TileLayoutProvider> currentLayout) {
    ++queryIteration_;
    auto &queryObjects = workload->semanticDataManagerForQuery(0)->labelsInQuery();

    addRegretForHistoricalQueries(queryObjects);

    // Generate baseline costs based on the current layout.
    WorkloadCostEstimator baselineCostEstimator(currentLayout, workload, gopLength_);
    auto baselineCosts = std::make_shared<std::unordered_map<unsigned int, CostElements>>();
    baselineCostEstimator.estimateCostForQuery(0, baselineCosts.get());

    addRegretForWorkload(queryIteration_, workload, baselineCosts, labels_);

    iterationToWorkload_[queryIteration_] = workload;
    iterationToBaselineCosts_[queryIteration_] = baselineCosts;
}

bool RegretAccumulator::shouldRetileGOP(unsigned int gop, std::string &layoutIdentifier) {
    long long int maxRegret = 0;
    std::string labelWithMaxRegret;

    for (auto it = gopToRegret_[gop].begin(); it != gopToRegret_[gop].end(); ++it) {
        if (it->second > maxRegret) {
            maxRegret = it->second;
            labelWithMaxRegret = it->first;
        }
    }

    if (maxRegret > threshold_ * gopTilingCost_) {
        layoutIdentifier = labelWithMaxRegret;
        std::cout << "Retile GOP " << gop << " to " << layoutIdentifier << std::endl;
        return true;
    } else
        return false;
}

void RegretAccumulator::resetRegretForGOP(unsigned int gop) {
    for (auto it = gopToRegret_[gop].begin(); it != gopToRegret_[gop].end(); ++it)
        it->second = 0;

    gopToClearedIteration_[gop] = queryIteration_;
}

std::shared_ptr<TileLayoutProvider> RegretAccumulator::configurationProviderForIdentifier(const std::string &identifier) {
    return idToConfig_.at(identifier);
}

void RegretAccumulator::addRegretForHistoricalQueries(const std::vector<std::string> &objects) {
    auto combinedObjects = combineStrings(objects);
    if (allObjects_.count(combinedObjects))
        return;

    allObjects_.insert(combinedObjects);
    singleObjects_.insert(objects.begin(), objects.end());

    // Add a layout for new objects and new combined objects.
    labels_.push_back(combinedObjects);
    idToConfig_[combinedObjects] = tileLayoutForObjects(objects);
    std::vector<std::string> newLayouts{combinedObjects};

    if (labels_.size() > 1) {
        std::vector<std::string> newAllObjects(singleObjects_.begin(), singleObjects_.end());
        auto newAllObjectsLabel = combineStrings(newAllObjects);
        labels_.push_back(newAllObjectsLabel);
        idToConfig_[newAllObjectsLabel] = tileLayoutForObjects(newAllObjects);
        newLayouts.push_back(newAllObjectsLabel);
    }

    // Go through historical queries.
    // If all GOPs affected by a query have been re-tiled, remove these costs from the dictionary.
    for (auto i = 1u; i < queryIteration_; ++i) {
        if (!iterationToWorkload_.count(i))
            continue;

        if (!costsAreForGOPsThatHaveNotBeenRetiled(i, iterationToBaselineCosts_.at(i))) {
            iterationToWorkload_.erase(i);
            iterationToBaselineCosts_.erase(i);
            continue;
        }

        auto workload = iterationToWorkload_.at(i);
        addRegretForWorkload(i, workload, iterationToBaselineCosts_.at(i), newLayouts);
    }
}

std::shared_ptr<TileLayoutProvider> RegretAccumulator::tileLayoutForObjects(const std::vector<std::string> &objects) {
    auto metadataSelection = std::make_shared<OrMetadataSelection>(objects);
    auto semanticDataManager = std::make_shared<SemanticDataManager>(semanticIndex_, metadataIdentifier_, metadataSelection);
    return std::make_shared<FineGrainedTileConfigurationProvider>(
            gopLength_,
            semanticDataManager,
            width_,
            height_);
}

bool RegretAccumulator::costsAreForGOPsThatHaveNotBeenRetiled(unsigned int iteration,
                                                              std::shared_ptr<std::unordered_map<unsigned int, CostElements>> baselineCosts) {
    for (auto it = baselineCosts->begin(); it != baselineCosts->end(); ++it) {
        auto gop = it->first;
        // If a GOP hasn't been cleared, it's still relevant.
        if (!gopToClearedIteration_.count(gop))
            return true;

        // If a GOP was cleared before this query, it's still relevant.
        if (gopToClearedIteration_.at(gop) < iteration)
            return true;
    }
    return false;
}

void RegretAccumulator::addRegretForWorkload(unsigned int iteration, std::shared_ptr<Workload> workload,
                                             std::shared_ptr<std::unordered_map<unsigned int, CostElements>> baselineCosts,
                                             const std::vector<std::string> layouts) {
    static const double pixelCostWeight = 1.608e-06;
    static const double tileCostWeight = 1.703e-01;

    WorkloadCostEstimator noTilesLayoutEstimator(noTilesConfiguration_, workload, gopLength_);
    auto noTilesCosts = std::make_unique<std::unordered_map<unsigned int, CostElements>>();
    noTilesLayoutEstimator.estimateCostForQuery(0, noTilesCosts.get());
    
    for (const auto &layoutId : layouts) {
        WorkloadCostEstimator proposedLayoutEstimator(idToConfig_.at(layoutId), workload, gopLength_);
        auto proposedCosts = std::make_unique<std::unordered_map<unsigned int, CostElements>>();
        proposedLayoutEstimator.estimateCostForQuery(0, proposedCosts.get());
        
        assert(baselineCosts->size() == proposedCosts->size());
        
        for (auto curIt = baselineCosts->begin(); curIt != baselineCosts->end(); ++curIt) {
            auto gop = curIt->first;
            if (gopToClearedIteration_.count(gop) && gopToClearedIteration_.at(gop) >= iteration)
                continue;
            
            auto curCosts = curIt->second;
            auto possibleCosts = proposedCosts->at(gop);
            double regret = pixelCostWeight *
                    (long long int)(curCosts.numPixels - possibleCosts.numPixels) +
                    tileCostWeight * (int)(curCosts.numTiles - possibleCosts.numTiles);
            if (possibleCosts.numPixels >= 0.8 * noTilesCosts->at(gop).numPixels)
                regret = std::numeric_limits<double>::lowest();

            addRegretToGOP(gop, regret, layoutId);
        }
    }
}

void RegretAccumulator::addRegretToGOP(unsigned int gop, double regret, const std::string &layoutIdentifier) {
    if (!gopToRegret_[gop].count(layoutIdentifier))
        gopToRegret_[gop][layoutIdentifier] = 0;

    gopToRegret_[gop][layoutIdentifier] += regret;
}

} // namespace tasm
