#ifndef LIGHTDB_TILECONFIGURATIONPROVIDER_H
#define LIGHTDB_TILECONFIGURATIONPROVIDER_H

#include "TileLayout.h"

namespace lightdb::tiles {
class TileConfigurationProvider {
public:
    virtual unsigned int maximumNumberOfTiles() = 0;
    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) = 0;
};

class NaiveTileConfigurationProvider : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 2;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout tileLayout = TileLayout(2, 1, {480, 480}, {576});
        return tileLayout;
    }
};

class GOP30TileConfigurationProvider : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 4;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout twoTileLayout = TileLayout(2, 1, {480, 480}, {576});
        static TileLayout fourTileLayout = TileLayout(2, 2, {480, 480}, {288, 288});

        if (frame < 30)
            return twoTileLayout;
        else if (30 <= frame && frame < 60)
            return fourTileLayout;
        else if (60 <= frame && frame < 90)
            return twoTileLayout;
        else if (frame >= 90)
            return fourTileLayout;
        else
            return twoTileLayout;
    }
};
} // namespace lightdb::tiles

#endif //LIGHTDB_TILECONFIGURATIONPROVIDER_H
