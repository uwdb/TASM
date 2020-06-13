#include "SmartTileConfigurationProvider.h"

#include <iostream>

namespace tasm {

std::shared_ptr<const TileLayout> SmartTileConfigurationProviderSingleSelection::tileLayoutForFrame(unsigned int frame) {
    auto gop = fineGrainedWorkloadCostEstimator_->gopForFrame(frame);
    if (gopToLayout_.count(gop))
        return gopToLayout_.at(gop);

    // Tile if it significantly reduces the number of pixels processed. Otherwise don't tile.
    bool shouldTile = fineGrainedLayoutCostByGOP_->at(gop).numPixels <= pixelThreshold_ * untiledCostByGOP_->at(gop).numPixels;
    if (!shouldTile)
        std::cout << "Not tiling GOP " << gop << std::endl;
    auto layout = shouldTile ? fineGrainedLayoutProvider_->tileLayoutForFrame(frame) : singleTileLayoutProvider_->tileLayoutForFrame(frame);
    gopToLayout_[gop] = layout;
    return layout;
}

} // namespace tasm