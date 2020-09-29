#ifndef TASM_TILECONFIGURATIONPROVIDER_H
#define TASM_TILECONFIGURATIONPROVIDER_H

#include "Configuration.h"
#include "Interval.h"
#include "TileLayout.h"

namespace tasm {
class SemanticDataManager;

class TileLayoutProvider {
public:
    virtual std::shared_ptr<TileLayout> tileLayoutForFrame(unsigned int frame) = 0;
    virtual ~TileLayoutProvider() {}
};

class SingleTileConfigurationProvider: public TileLayoutProvider {
public:
    SingleTileConfigurationProvider(unsigned int totalWidth, unsigned int totalHeight)
            : totalWidth_(totalWidth), totalHeight_(totalHeight), layout_(new TileLayout(1, 1, {totalWidth_}, {totalHeight_}))
    { }

    std::shared_ptr<TileLayout> tileLayoutForFrame(unsigned int frame) override {
        return layout_;
    }

private:
    unsigned int totalWidth_;
    unsigned  int totalHeight_;
    std::shared_ptr<TileLayout> layout_;
};

class UniformTileconfigurationProvider: public TileLayoutProvider {
public:
    UniformTileconfigurationProvider(unsigned int numRows, unsigned int numColumns, Configuration configuration)
        : numRows_(numRows),
        numColumns_(numColumns),
        configuration_(configuration)
    {}

    std::shared_ptr<TileLayout> tileLayoutForFrame(unsigned int frame) override {
        if (layoutPtr)
            return layoutPtr;

        layoutPtr = std::make_shared<TileLayout>(numColumns_, numRows_,
                tile_dimensions(configuration_.codedWidth, configuration_.displayWidth, numColumns_),
                tile_dimensions(configuration_.codedHeight, configuration_.displayHeight, numRows_));
        return layoutPtr;
    }

private:
    std::vector<unsigned int> tile_dimensions(unsigned int codedDimension, unsigned int displayDimension, unsigned int numTiles) {
//        static unsigned int CTBS_SIZE_Y = 32;
        std::vector<unsigned int> dimensions(numTiles);
        unsigned int total = 0;
        for (auto i = 0u; i < numTiles; ++i) {
            unsigned int proposedDimension = ((i + 1) * codedDimension / numTiles) - (i * codedDimension / numTiles);
            if (total + proposedDimension > displayDimension)
                proposedDimension = displayDimension - total;

            dimensions[i] = proposedDimension;
            total += proposedDimension;
        }
        return dimensions;
    }

    unsigned int numRows_;
    unsigned int numColumns_;
    Configuration configuration_;
    std::shared_ptr<TileLayout> layoutPtr;
};

class FineGrainedTileConfigurationProvider : public TileLayoutProvider {
public:
    FineGrainedTileConfigurationProvider(unsigned int tileLayoutDuration,
                                        std::shared_ptr<SemanticDataManager> semanticDataManager,
                                        unsigned int frameWidth,
                                        unsigned int frameHeight)
        : tileLayoutDuration_(tileLayoutDuration),
        semanticDataManager_(semanticDataManager),
        frameWidth_(frameWidth),
        frameHeight_(frameHeight) {}

    std::shared_ptr<TileLayout> tileLayoutForFrame(unsigned int frame) override;

private:
    std::vector<unsigned int> tileDimensions(const std::vector<interval::Interval<int>> &sortedIntervals, int minDistance, int totalDimension);

    unsigned int tileLayoutDuration_;
    std::shared_ptr<SemanticDataManager> semanticDataManager_;
    unsigned int frameWidth_;
    unsigned int frameHeight_;
    std::unordered_map<unsigned int, std::shared_ptr<TileLayout>> tileGroupToTileLayout_;
};

class ConglomerationTileConfigurationProvider : public TileLayoutProvider {
public:
    ConglomerationTileConfigurationProvider(std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<TileLayoutProvider>>> gopToLayoutProvider, unsigned int gopLength)
        : gopToLayoutProvider_(std::move(gopToLayoutProvider)),
        gopLength_(gopLength) {}

    std::shared_ptr<TileLayout> tileLayoutForFrame(unsigned int frame) override {
        return gopToLayoutProvider_->at(frame / gopLength_)->tileLayoutForFrame(frame);
    }

private:
    std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<TileLayoutProvider>>> gopToLayoutProvider_;
    unsigned int gopLength_;
};

} // namespace tasm

#endif //TASM_TILECONFIGURATIONPROVIDER_H
