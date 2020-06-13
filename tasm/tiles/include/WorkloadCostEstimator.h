#ifndef TASM_WORKLOADCOSTESTIMATOR_H
#define TASM_WORKLOADCOSTESTIMATOR_H

#include "TileConfigurationProvider.h"

namespace tasm {
class SemanticDataManager;

class Workload {
public:
    Workload(std::shared_ptr<SemanticDataManager> selection)
        : selections_{selection},
        queryCounts_{1} {}

    Workload(const std::vector<std::shared_ptr<SemanticDataManager>> &selections, const std::vector<unsigned int> &queryCounts)
            : selections_(selections),
              queryCounts_(queryCounts) {
        assert(selections_.size() == queryCounts_.size());
    }

    std::shared_ptr<SemanticDataManager> semanticDataManagerForQuery(unsigned int queryNum) const {
        return selections_[queryNum];
    }

    unsigned int numberOfTimesQueryIsExecuted(unsigned int queryNum) const {
        return queryCounts_[queryNum];
    }

    unsigned int numberOfQueries() const {
        return queryCounts_.size();
    }

private:
    std::vector<std::shared_ptr<SemanticDataManager>> selections_;
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
    WorkloadCostEstimator(std::shared_ptr<TileLayoutProvider> tileLayoutProvider,
            std::shared_ptr<Workload> workload,
            unsigned int gopLength)
            : tileLayoutProvider_(tileLayoutProvider),
            workload_(workload),
            gopLength_(gopLength),
            totalNumberOfPixels_(0),
            totalNumberOfTiles_(0) {}

    CostElements estimateCostForQuery(unsigned int queryNum, std::unordered_map<unsigned int, CostElements> *costByGOP = nullptr);

    unsigned int gopForFrame(unsigned int frameNum) const {
        return frameNum / gopLength_;
    }
private:
    unsigned int keyframeForFrame(unsigned int frameNum) const {
        return gopForFrame(frameNum) * gopLength_;
    }

    std::pair<int, CostElements> estimateCostForNextGOP(std::vector<int>::const_iterator &start,
                                                        std::vector<int>::const_iterator end,
                                                        std::shared_ptr<SemanticDataManager> metadataManager);

    std::shared_ptr<TileLayoutProvider> tileLayoutProvider_;
    std::shared_ptr<Workload> workload_;
    unsigned int gopLength_;
    unsigned int totalNumberOfPixels_;
    unsigned int totalNumberOfTiles_;
};

} // namespace tasm

#endif //TASM_WORKLOADCOSTESTIMATOR_H
