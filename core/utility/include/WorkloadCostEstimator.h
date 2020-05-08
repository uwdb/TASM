#ifndef LIGHTDB_WORKLOADCOSTESTIMATOR_H
#define LIGHTDB_WORKLOADCOSTESTIMATOR_H

#include "TileConfigurationProvider.h"
#include "MetadataLightField.h"

#include <vector>

namespace lightdb {

class Workload {
public:
    Workload(const std::string &metadataIdentifier, MetadataSpecification selection)
        : metadataIdentifier_(metadataIdentifier),
        selections_{selection},
        queryCounts_{1} {}

    Workload(const std::string &metadataIdentifier,
            const std::vector<MetadataSpecification> &selections,
            const std::vector<unsigned int> &queryCounts):
            metadataIdentifier_(metadataIdentifier),
            selections_(selections),
            queryCounts_(queryCounts) {}

    std::shared_ptr<metadata::MetadataManager> metadataManagerForQuery(unsigned int queryNum) const {
        return std::make_shared<metadata::MetadataManager>(metadataIdentifier_, selections_[queryNum]);
    }

    unsigned int numberOfTimesQueryIsExecuted(unsigned int queryNum) const {
        return queryCounts_[queryNum];
    }

    unsigned int numberOfQueries() const {
        return queryCounts_.size();
    }

private:
    std::string metadataIdentifier_;
    std::vector<MetadataSpecification> selections_;
    std::vector<unsigned int> queryCounts_;
};

struct CostElements {
    CostElements(unsigned long long numPixels, unsigned long long numTiles):
            numPixels(numPixels),
            numTiles(numTiles) {}

    unsigned long long numPixels;
    unsigned long long numTiles;
};

class WorkloadCostEstimator {
public:
    WorkloadCostEstimator(std::shared_ptr<tiles::TileLayoutProvider> tileLayoutProvider,
        Workload workload,
        unsigned int gopLength,
        double pixelCostWeight,
        double tileCostWeight,
        double decodeCostWeight)
        : tileLayoutProvider_(tileLayoutProvider),
        workload_(std::move(workload)),
        gopLength_(gopLength),
        totalNumberOfPixels_(0),
        totalNumberOfTiles_(0),
        pixelCostWeight_(pixelCostWeight),
        tileCostWeight_(tileCostWeight),
        decodingStrategyWeight_(decodeCostWeight) {
//        Not including decode strategy: coef: [1.49791333e-06 1.24557786e-01]
//        With decode strategy: coef: [1.49375947e-06 1.33864548e-01 3.69633770e+03]
    }

    CostElements estimateCostForQuery(unsigned int queryNum, unsigned int &sawMultipleLayouts, std::unordered_map<unsigned int, CostElements> *costByGOP = nullptr);



    double estimatedCostForWorkload() {
        double totalCost = 0;
        for (auto i = 0u; i < workload_.numberOfQueries(); ++i) {
            auto sawMultipleLayouts = 0u;
            firstTileDimensions_ = nullptr;
            auto costElementsForQuery = estimateCostForQuery(i, sawMultipleLayouts);
            totalCost += workload_.numberOfTimesQueryIsExecuted(i) *
                            (pixelCostWeight_ * costElementsForQuery.numPixels
                            + tileCostWeight_ * costElementsForQuery.numTiles
                            + decodingStrategyWeight_ * sawMultipleLayouts);
        }
        return totalCost;
    }

    bool queryRequiresMultipleLayouts(unsigned int queryNum) {
        auto sawMultipleLayouts = 0u;
        estimateCostForQuery(queryNum, sawMultipleLayouts);
        return sawMultipleLayouts;
    }

    unsigned int gopForFrame(unsigned int frameNum) const {
        return frameNum / gopLength_;
    }

private:
    unsigned int keyframeForFrame(unsigned int frameNum) const {
        return gopForFrame(frameNum) * gopLength_;
    }
    std::pair<int, CostElements> estimateCostForNextGOP(std::vector<int>::const_iterator &start,
            std::vector<int>::const_iterator end,
            std::shared_ptr<metadata::MetadataManager> metadataManager,
            unsigned int &sawMultipleLayouts);

    std::shared_ptr<tiles::TileLayoutProvider> tileLayoutProvider_;
    Workload workload_;
    unsigned int gopLength_;
    unsigned int totalNumberOfPixels_;
    unsigned int totalNumberOfTiles_;
    double pixelCostWeight_;
    double tileCostWeight_;
    double decodingStrategyWeight_;

    std::unique_ptr<Rectangle> firstTileDimensions_;
};

} // namespace lightdb

#endif //LIGHTDB_WORKLOADCOSTESTIMATOR_H
