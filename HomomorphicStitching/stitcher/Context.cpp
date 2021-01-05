//
// Created by sophi on 5/3/2018.
//

#include "Context.h"

namespace lightdb::tiles {


    Context::Context(const int *tile_dimensions, const int *video_dimensions, const int *video_display_dimensions,
            bool should_use_uniform_tiles,
            const std::vector<unsigned int> &heights_of_tiles,
            const std::vector<unsigned int> &widths_of_tiles,
            unsigned int pps_id)
        : should_use_uniform_tiles_(should_use_uniform_tiles),
          heights_of_tiles_(heights_of_tiles),
          widths_of_tiles_(widths_of_tiles),
          pps_id_(pps_id) {
        tile_dimensions_[0] = tile_dimensions[0];
        tile_dimensions_[1] = tile_dimensions[1];

        video_dimensions_[0] = video_dimensions[0]; // * tile_dimensions[0];
        video_dimensions_[1] = video_dimensions[1]; // * tile_dimensions[1];

        video_display_dimensions_[0] = video_display_dimensions[0];
        video_display_dimensions_[1] = video_display_dimensions[1];
    }

    int* Context::GetTileDimensions() {
        return tile_dimensions_;
    }

    int* Context::GetVideoDimensions() {
        return video_dimensions_;
    }

} // namespace lightdb::tiles
