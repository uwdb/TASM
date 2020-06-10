#ifndef TASM_TILECONFIGURATIONPROVIDER_H
#define TASM_TILECONFIGURATIONPROVIDER_H

#include "TileLayout.h"

namespace tasm {

class TileLayoutProvider {
public:
    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) = 0;
    virtual ~TileLayoutProvider() {}
};

class SingleTileConfigurationProvider: public TileLayoutProvider {
public:
    SingleTileConfigurationProvider(unsigned int totalWidth, unsigned int totalHeight)
            : totalWidth_(totalWidth), totalHeight_(totalHeight), layout_(1, 1, {totalWidth_}, {totalHeight_})
    { }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        return layout_;
    }

private:
    unsigned int totalWidth_;
    unsigned  int totalHeight_;
    TileLayout layout_;
};

} // namespace tasm

#endif //TASM_TILECONFIGURATIONPROVIDER_H
