//
// Created by sophi on 5/3/2018.
//

#ifndef LIGHTDB_CONTEXT_H
#define LIGHTDB_CONTEXT_H

#include <vector>

namespace lightdb {

    class Context {
    public:

        /**
         * Constructs a context with the given tile and video dimensions
         * @param tile_dimensions The tile dimensions, first element being the height (number of rows)
         * and the second being the width (number of columns)
         * @param video_dimensions  The video dimensions, first element being the height and the second
         * the width
         */
        Context(const int *tile_dimensions, const int *video_dimensions, const int *video_display_dimensions, bool should_use_uniform_tiles,
                const std::vector<unsigned long> &heights_of_tiles, const std::vector<unsigned long> &widths_of_tiles,
                unsigned int pps_id);
        /**
         *
         * @return The tile dimensions. Note that this gives the user the ability to change the dimensions
         */
        int* GetTileDimensions();
        /**
         *
         * @return The video dimensions. Note that this gives the user the ability to change the dimensions
         */
        int* GetVideoDimensions();
        int GetVideoCodedWidth() const { return video_dimensions_[1]; }
        int GetVideoCodedHeight() const { return video_dimensions_[0]; }


        int GetVideoDisplayWidth() const { return video_display_dimensions_[1]; };
        int GetVideoDisplayHeight() const { return video_display_dimensions_[0]; };

        const std::vector<unsigned long> &GetHeightsOfTiles() const { return heights_of_tiles_; }
        const std::vector<unsigned long> &GetWidthsOfTiles() const { return widths_of_tiles_; }
        bool GetShouldUseUniformTiles() const { return should_use_uniform_tiles_; }

        unsigned int GetPPSId() const { return pps_id_; }

    private:

        int tile_dimensions_[2];
        int video_dimensions_[2];
        int video_display_dimensions_[2];

        bool should_use_uniform_tiles_;
        std::vector<unsigned long> heights_of_tiles_;
        std::vector<unsigned long> widths_of_tiles_;

        unsigned int pps_id_;
    };

}

#endif //LIGHTDB_CONTEXT_H
