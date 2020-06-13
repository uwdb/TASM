#ifndef TASM_SMARTTILECONFIGURATIONPROVIDER_H
#define TASM_SMARTTILECONFIGURATIONPROVIDER_H

#include "TileConfigurationProvider.h"
#include "WorkloadCostEstimator.h"

namespace tasm {


class SmartTileConfigurationProviderSingleSelection : public TileLayoutProvider {
public:
    SmartTileConfigurationProviderSingleSelection(
            unsigned int tileLayoutDuration,
            std::shared_ptr<SemanticDataManager> semanticDataManager,
            unsigned int frameWidth,
            unsigned int frameHeight)
            : fineGrainedLayoutProvider_(new FineGrainedTileConfigurationProvider(tileLayoutDuration, semanticDataManager, frameWidth, frameHeight)),
            singleTileLayoutProvider_(new SingleTileConfigurationProvider(frameWidth, frameHeight)),
            workload_(new Workload(semanticDataManager)),
            fineGrainedWorkloadCostEstimator_(new WorkloadCostEstimator(fineGrainedLayoutProvider_, workload_, tileLayoutDuration)),
            untiledWorkloadCostEstimator_(new WorkloadCostEstimator(singleTileLayoutProvider_, workload_, tileLayoutDuration)),
            fineGrainedLayoutCostByGOP_(new std::unordered_map<unsigned int, CostElements>()),
            untiledCostByGOP_(new std::unordered_map<unsigned int, CostElements>()) {
        // TODO: Do this work incrementally rather than in constructor.
        fineGrainedWorkloadCostEstimator_->estimateCostForQuery(0, fineGrainedLayoutCostByGOP_.get());
        untiledWorkloadCostEstimator_->estimateCostForQuery(0, untiledCostByGOP_.get());
    }

    std::shared_ptr<const TileLayout> tileLayoutForFrame(unsigned int frame) override;

private:
    std::shared_ptr<FineGrainedTileConfigurationProvider> fineGrainedLayoutProvider_;
    std::shared_ptr<SingleTileConfigurationProvider> singleTileLayoutProvider_;
    std::shared_ptr<Workload> workload_;
    std::shared_ptr<WorkloadCostEstimator> fineGrainedWorkloadCostEstimator_;
    std::shared_ptr<WorkloadCostEstimator> untiledWorkloadCostEstimator_;

    std::unordered_map<unsigned int, std::shared_ptr<const TileLayout>> gopToLayout_;
    constexpr static const double pixelThreshold_ = 0.8;

    std::unique_ptr<std::unordered_map<unsigned int, CostElements>> fineGrainedLayoutCostByGOP_;
    std::unique_ptr<std::unordered_map<unsigned int, CostElements>> untiledCostByGOP_;
};

} // namespace tasm

#endif //TASM_SMARTTILECONFIGURATIONPROVIDER_H
