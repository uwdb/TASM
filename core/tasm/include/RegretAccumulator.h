#ifndef LIGHTDB_REGRETACCUMULATOR_H
#define LIGHTDB_REGRETACCUMULATOR_H

#include "TileConfigurationProvider.h"
#include "WorkloadCostEstimator.h"

namespace lightdb::logical {
class RegretAccumulator {
public:
    virtual void addRegretToGOP(unsigned int gop, double regret, const std::string &layoutIdentifier) = 0;

    virtual void addRegretForQuery(std::shared_ptr<Workload> workload,
                                   std::shared_ptr<std::unordered_map<unsigned int, CostElements>> baselineCosts) = 0;

    virtual bool shouldRetileGOP(unsigned int gop, std::string &layoutIdentifier) = 0;

    virtual void resetRegretForGOP(unsigned int gop) = 0;

    virtual std::shared_ptr<tiles::TileConfigurationProvider>
    configurationProviderForIdentifier(const std::string &identifier) = 0;

    virtual const std::vector<std::string> layoutIdentifiers() = 0;

    virtual ~RegretAccumulator() {}
};

class TASMRegretAccumulator : public RegretAccumulator {
public:
    TASMRegretAccumulator(const std::string &video, unsigned int width, unsigned int height, unsigned int gopLength,
                          double threshold,
                          bool readEntireGOPs = false)
            : video_(video),
              width_(width),
              height_(height),
              gopLength_(gopLength),
              threshold_(threshold),
              readEntireGOPs_(readEntireGOPs),
              queryIteration_(0) {
        gopSizeInPixels_ = width * height * gopLength;
        gopTilingCost_ = estimateCostToEncodeGOP(gopSizeInPixels_);

        noTilesConfiguration_.reset(new tiles::SingleTileConfigurationProvider(width_, height_));
    }

    void addRegretToGOP(unsigned int gop, double regret, const std::string &layoutIdentifier) override {
        if (!gopToRegret_[gop].count(layoutIdentifier))
            gopToRegret_[gop][layoutIdentifier] = 0;

        gopToRegret_[gop][layoutIdentifier] += regret;
    }

    void addRegretForQuery(std::shared_ptr<Workload> workload,
                           std::shared_ptr<std::unordered_map<unsigned int, CostElements>> baselineCosts) override {
        queryIteration_ += 1;
        auto queryObjects = workload->metadataManagerForQuery(0)->metadataSpecification().objects();

        addRegretForHistoricalQueries(queryObjects);

        addRegretForWorkload(queryIteration_, workload, baselineCosts, labels_);

        iterationToWorkload_[queryIteration_] = workload;
        iterationToBaselineCosts_[queryIteration_] = baselineCosts;
    }

    bool shouldRetileGOP(unsigned int gop, std::string &layoutIdentifier) override {
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
            std::cout << video_ << ": Retile GOP " << gop << " to " << layoutIdentifier << std::endl;
            StatsCollector::instance().addStat("retile-gop-" + std::to_string(gop), layoutIdentifier);
            return true;
        } else {
            return false;
        }
    }

    void resetRegretForGOP(unsigned int gop) override {
        for (auto it = gopToRegret_[gop].begin(); it != gopToRegret_[gop].end(); ++it)
            it->second = 0;

        gopToClearedIteration_[gop] = queryIteration_;
    }

    std::shared_ptr<tiles::TileConfigurationProvider>
    configurationProviderForIdentifier(const std::string &identifier) override {
        return idToConfig_.at(identifier);
    }

    const std::vector<std::string> layoutIdentifiers() override {
        return labels_;
    }

    static std::string combineStrings(const std::vector<std::string> &strings, std::string connector = "_") {
        std::string combined = "";
        auto lastIndex = strings.size() - 1;
        for (auto i = 0u; i < strings.size(); ++i) {
            combined += strings[i];
            if (i < lastIndex)
                combined += connector;
        }
        return combined;
    }

    static std::shared_ptr<MetadataElement> MetadataElementForObjects(const std::vector<std::string> &objects, unsigned int firstFrame = 0, unsigned int lastFrame = UINT32_MAX) {
        std::vector<std::shared_ptr<MetadataElement>> componentElements(objects.size());
        std::transform(objects.begin(), objects.end(), componentElements.begin(),
                       [&](auto obj) {
                           return std::make_shared<SingleMetadataElement>("label", obj, firstFrame, lastFrame);
                       });
        return componentElements.size() == 1 ? componentElements.front() : std::make_shared<OrMetadataElement>(componentElements);
    }

private:
    double estimateCostToEncodeGOP(long long int sizeInPixels) const {
        static double pixelCoef = 3.206e-06;
        static double pixelIntercept = 2.592;

        return pixelCoef * gopSizeInPixels_ + pixelIntercept;
    }

    std::shared_ptr<tiles::GroupingTileConfigurationProvider>
    tileLayoutForObjects(const std::vector<std::string> &objects) {
        auto metadataManager = std::make_shared<metadata::MetadataManager>(video_,
                                                                           MetadataSpecification("labels",
                                                                                                 MetadataElementForObjects(
                                                                                                         objects)));
        return std::make_shared<tiles::GroupingTileConfigurationProvider>(
                gopLength_, metadataManager, width_, height_);
    }

    bool costsAreForGOPsThatHaveNotBeenRetiled(unsigned int iteration,
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

    void addRegretForWorkload(unsigned int iteration, std::shared_ptr<Workload> workload,
                              std::shared_ptr<std::unordered_map<unsigned int, CostElements>> baselineCosts,
                              const std::vector<std::string> layouts) {
        static const double pixelCostWeight = 1.608e-06;
        static const double tileCostWeight = 1.703e-01;

        unsigned int sawMultipleLayouts;

        WorkloadCostEstimator noTilesLayoutEstimator(noTilesConfiguration_, *workload, gopLength_, 0, 0, 0, readEntireGOPs_);
        std::unique_ptr<std::unordered_map<unsigned int, CostElements>> noTilesCosts(
                new std::unordered_map<unsigned int, CostElements>());
        noTilesLayoutEstimator.estimateCostForQuery(0, sawMultipleLayouts, noTilesCosts.get());

        for (const auto &layoutId : layouts) {
            WorkloadCostEstimator proposedLayoutEstimator(idToConfig_.at(layoutId), *workload, gopLength_, 0, 0, 0, readEntireGOPs_);
            std::unique_ptr<std::unordered_map<unsigned int, CostElements>> proposedCosts(
                    new std::unordered_map<unsigned int, CostElements>());
            proposedLayoutEstimator.estimateCostForQuery(0, sawMultipleLayouts, proposedCosts.get());

            assert(baselineCosts->size() == proposedCosts->size());

            for (auto curIt = baselineCosts->begin(); curIt != baselineCosts->end(); ++curIt) {
                auto gop = curIt->first;
                if (gopToClearedIteration_.count(gop) && gopToClearedIteration_.at(gop) >= iteration)
                    continue;

                auto curCosts = curIt->second;
                auto possibleCosts = proposedCosts->at(gop);
                double regret = pixelCostWeight *
                                (long long int) (curCosts.numPixels - possibleCosts.numPixels) +
                                tileCostWeight * (int) (curCosts.numTiles - possibleCosts.numTiles);
                if (possibleCosts.numPixels >= 0.8 * noTilesCosts->at(gop).numPixels) {
                    std::cout << "ALERT: pixel threshold crossed for " << layoutId << std::endl;
                    regret = std::numeric_limits<double>::lowest();
                }

                addRegretToGOP(gop, regret, layoutId);
            }
        }
    }

    void addRegretForHistoricalQueries(const std::vector<std::string> &objects) {
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
        // If all GOPs affected by a query have been re-tiled, remove those costs from the dictionary.
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

    std::string video_;
    unsigned int width_;
    unsigned int height_;
    unsigned int gopLength_;

    double threshold_;
    bool readEntireGOPs_;
    std::vector<std::string> labels_;
    std::unordered_map<std::string, std::shared_ptr<tiles::TileConfigurationProvider>> idToConfig_;
    long long int gopSizeInPixels_;
    double gopTilingCost_;
    std::unordered_map<unsigned int, std::unordered_map<std::string, double>> gopToRegret_;

    unsigned int queryIteration_;
    std::unordered_map<unsigned int, unsigned int> gopToClearedIteration_;
    std::unordered_map<unsigned int, std::shared_ptr<Workload>> iterationToWorkload_;
    std::unordered_map<unsigned int, std::shared_ptr<std::unordered_map<unsigned int, CostElements>>> iterationToBaselineCosts_;
    std::unordered_set<std::string> allObjects_;
    std::unordered_set<std::string> singleObjects_;

    std::shared_ptr<tiles::SingleTileConfigurationProvider> noTilesConfiguration_;
};

} // namespace lightdb::logical

#endif //LIGHTDB_REGRETACCUMULATOR_H
