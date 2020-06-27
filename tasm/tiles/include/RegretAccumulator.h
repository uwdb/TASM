#ifndef TASM_REGRETACCUMULATOR_H
#define TASM_REGRETACCUMULATOR_H

#include "TileConfigurationProvider.h"
#include "WorkloadCostEstimator.h"
#include <unordered_set>

namespace tasm {
class SemanticIndex;

class RegretAccumulator {
public:
    RegretAccumulator(std::shared_ptr<SemanticIndex> semanticIndex, const std::string &metadataIdentifier,
            unsigned int width, unsigned int height, unsigned int gopLength, double threshold = 1.0)
        : semanticIndex_(semanticIndex), metadataIdentifier_(metadataIdentifier),
        width_(width), height_(height), gopLength_(gopLength), threshold_(threshold),
        gopSizeInPixels_(width_ * height_ * gopLength_),
        gopTilingCost_(estimateCostToEncodeGOP(gopSizeInPixels_)),
        noTilesConfiguration_(new SingleTileConfigurationProvider(width_, height_)) {}

    void addRegretForQuery(std::shared_ptr<Workload> workload, std::shared_ptr<TileLayoutProvider> currentLayout);
    std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<TileLayoutProvider>>> getNewGOPLayouts();

private:
    bool shouldRetileGOP(unsigned int gop, std::string &layoutIdentifier);
    void resetRegretForGOP(unsigned int gop);
    std::shared_ptr<TileLayoutProvider> configurationProviderForIdentifier(const std::string &identifier);

    void addRegretForHistoricalQueries(const std::vector<std::string> &objects);
    std::shared_ptr<TileLayoutProvider> tileLayoutForObjects(const std::vector<std::string> &objects);
    bool costsAreForGOPsThatHaveNotBeenRetiled(unsigned int iteration, std::shared_ptr<std::unordered_map<unsigned int, CostElements>> baselineCosts);
    void addRegretForWorkload(
            unsigned int iteration,
            std::shared_ptr<Workload> workload,
            std::shared_ptr<std::unordered_map<unsigned int, CostElements>> baselineCosts,
            const std::vector<std::string> layouts);
    void addRegretToGOP(unsigned int gop, double regret, const std::string &layoutIdentifier);
    double estimateCostToEncodeGOP(long long int sizeInPixels) const {
        static double pixelCoef = 3.206e-06;
        static double pixelIntercept = 2.592;

        return pixelCoef * gopSizeInPixels_ + pixelIntercept;
    }

    std::shared_ptr<SemanticIndex> semanticIndex_;
    const std::string &metadataIdentifier_;

    unsigned int width_;
    unsigned int height_;
    unsigned int gopLength_;

    double threshold_;
    std::vector<std::string> labels_;
    std::unordered_map<std::string, std::shared_ptr<TileLayoutProvider>> idToConfig_;

    long long int gopSizeInPixels_;
    double gopTilingCost_;
    std::unordered_map<unsigned int, std::unordered_map<std::string, double>> gopToRegret_;

    unsigned int queryIteration_;
    std::unordered_map<unsigned int, unsigned int> gopToClearedIteration_;
    std::unordered_map<unsigned int, std::shared_ptr<Workload>> iterationToWorkload_;
    std::unordered_map<unsigned int, std::shared_ptr<std::unordered_map<unsigned int, CostElements>>> iterationToBaselineCosts_;
    std::unordered_set<std::string> allObjects_;
    std::unordered_set<std::string> singleObjects_;

    std::shared_ptr<SingleTileConfigurationProvider> noTilesConfiguration_;
};

} // namespace tasm

#endif //TASM_REGRETACCUMULATOR_H
