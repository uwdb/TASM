#include "SmartTileConfigurationProvider.h"

#include <iostream>

namespace tasm {

std::shared_ptr<TileLayout> SmartTileConfigurationProviderSingleSelection::tileLayoutForFrame(unsigned int frame) {
    auto gop = fineGrainedWorkloadCostEstimator_->gopForFrame(frame);
    if (gopToLayout_.count(gop))
        return gopToLayout_.at(gop);

    // Tile if it significantly reduces the number of pixels processed. Otherwise don't tile.
    // A GOP won't be in fineGrainedLayoutCostByGOP_ if it doesn't have metadata. In that case, we won't tile regardless.
    bool shouldTile = fineGrainedLayoutCostByGOP_->count(gop)
            ? fineGrainedLayoutCostByGOP_->at(gop).numPixels <= pixelThreshold_ * untiledCostByGOP_->at(gop).numPixels
            : false;
    if (!shouldTile)
        std::cout << "Not tiling GOP " << gop << std::endl;
    auto layout = shouldTile ? fineGrainedLayoutProvider_->tileLayoutForFrame(frame) : singleTileLayoutProvider_->tileLayoutForFrame(frame);
    gopToLayout_[gop] = layout;
    return layout;
}

} // namespace tasm