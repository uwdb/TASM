#include "PictureParameterSet.h"

#include <algorithm>

namespace stitching {
    void PictureParameterSet::SetTileDimensions(const std::pair<unsigned long, unsigned long>& dimensions, const bool loop_filter_enabled) {
        if (dimensions.first == tile_dimensions_.first &&
            dimensions.second == tile_dimensions_.second) {
            return;
        }

        assert(dimensions.first > 0 && dimensions.second > 0);
        assert(!getMetadataValue("tiles_enabled_flag"));
//        assert(tile_dimensions_.first == 0 && tile_dimensions_.second == 0);

        // The dimensions are width first, then height, so we must reverse the
        // order from our height, width
        // Next comes a one, and then a 1 if the filter is enabled, 0 if not
        std::vector<unsigned long> new_dimensions{dimensions.second - 1, dimensions.first - 1};

        auto dimensions_bits = EncodeGolombs(new_dimensions);
        bool using_uniform_tiles = GetContext().GetShouldUseUniformTiles();
        dimensions_bits.Insert(dimensions_bits.size(), using_uniform_tiles ? 1 : 0, 1);

        if (!using_uniform_tiles) {
            // Need to insert bits for widths and heights of tiles.
            // Need to subtract 1 from every width and height.
            std::vector<unsigned long> widths(GetContext().GetWidthsOfTiles().begin(), GetContext().GetWidthsOfTiles().end());
            std::for_each(widths.begin(), widths.end(), [](auto &w) { --w; });
            std::vector<unsigned long> heights(GetContext().GetHeightsOfTiles().begin(), GetContext().GetHeightsOfTiles().end());
            std::for_each(heights.begin(), heights.end(), [](auto &h) { --h; });
            BitArray tile_width_bits = EncodeGolombs(widths);
            BitArray tile_height_bits = EncodeGolombs(heights);

            dimensions_bits.insert(dimensions_bits.end(), tile_width_bits.begin(), tile_width_bits.end());
            dimensions_bits.insert(dimensions_bits.end(), tile_height_bits.begin(), tile_height_bits.end());
        }

        if (loop_filter_enabled) {
            dimensions_bits.Insert(dimensions_bits.size(), 1, 1);
        } else {
            dimensions_bits.Insert(dimensions_bits.size(), 0, 1);
        }

        // Set the tiles enabled flag to true
        data_[getMetadataValue("tiles_enabled_flag_offset")] = true;
        data_.insert(data_.begin() + getMetadataValue("tile_dimensions_offset"), make_move_iterator(dimensions_bits.begin()), make_move_iterator(dimensions_bits.end()));
        data_.ByteAlignWithoutRemoval();

        tile_dimensions_ = dimensions;
    }

    bool PictureParameterSet::TryToTurnOnOutputFlagPresentFlag() {
        bool outputFlagPresent = getMetadataValue("output_flag_present_flag");
        if (outputFlagPresent)
            return false;

        // Flip flag to 1.
        assert(!data_[getMetadataValue("output_flag_present_flag_offset")]);
        data_[getMetadataValue("output_flag_present_flag_offset")] = true;
        assert(data_[getMetadataValue("output_flag_present_flag_offset")]);

        return true;
    }

    bool PictureParameterSet::HasOutputFlagPresentFlagEnabled() {
            return data_[getMetadataValue("output_flag_present_flag_offset")];
    }

void PictureParameterSet::SetPPSId(unsigned int pps_id) {
    // Assume the video already has a pps_id of 0.
    auto current_pps_id = getMetadataValue("pps_pic_parameter_set_id");
    if (current_pps_id == pps_id)
        return;

    // Replace the current pps_id with the new one, then re-byte-align.
    auto offset = getMetadataValue("pps_pic_parameter_set_id_offset");
    auto current_encoded_pps_id = EncodeGolombs({ current_pps_id });
    auto length_of_current_encoded_pps_id = current_encoded_pps_id.size();

    auto new_encoded_pps_id = EncodeGolombs({ pps_id });

    data_.Replace(offset, offset + length_of_current_encoded_pps_id, new_encoded_pps_id);
    data_.ByteAlign();
}
}; //namespace stitching

