//
// Created by sophi on 4/14/2018.
//

#include <algorithm>
#include <string>
#include <vector>
#include <cassert>

#include "BitStream.h"
#include "BitArray.h"
#include "PictureParameterSet.h"
#include "Emulation.h"
#include "Golombs.h"

using std::vector;
using std::string;
using lightdb::utility::BitStream;
using lightdb::utility::BitArray;

namespace lightdb {

    PictureParameterSet::PictureParameterSet(const tiles::Context &context, const bytestring &data) : Nal(context, data),
                                             data_(RemoveEmulationPrevention(data, GetHeaderSize(), data.size())),
                                             metadata_(data_.begin(), data_.begin() + GetHeaderSizeInBits()) {

        metadata_.MarkPosition("pps_pic_parameter_set_id_offset");
        metadata_.CollectGolomb("pps_pic_parameter_set_id");
        metadata_.SkipExponentialGolomb();
        metadata_.SkipBits(6);
        metadata_.CollectValue("cabac_init_present_flag");
        metadata_.SkipExponentialGolomb();
        metadata_.SkipExponentialGolomb();
        metadata_.SkipExponentialGolomb();
        metadata_.SkipBits(2);
        metadata_.CollectValue("cu_qp_delta_enabled_flag");
        metadata_.SkipExponentialGolombs("cu_qp_delta_enabled_flag", 1);
        metadata_.SkipExponentialGolomb();
        metadata_.SkipExponentialGolomb();
        metadata_.SkipBits(4);
        metadata_.MarkPosition("tiles_enabled_flag_offset");
        metadata_.CollectValue("tiles_enabled_flag");
        metadata_.CollectValue("entropy_coding_sync_enabled_flag");
        metadata_.MarkPosition("tile_dimensions_offset");
        metadata_.ByteAlign();
        metadata_.MarkPosition("end");

        assert (metadata_.GetValue("end") % 8 == 0);
    }

    bytestring PictureParameterSet::GetBytes() const {
        //std::cerr << "picture" << std::endl;
        for (auto b : AddEmulationPreventionAndMarker(data_, GetHeaderSize(), data_.size() / 8)) {
            //std::cerr << static_cast<int>(static_cast<unsigned char>(b)) << std::endl;
        }
        return AddEmulationPreventionAndMarker(data_, GetHeaderSize(), data_.size() / 8);
    }

    bool PictureParameterSet::HasEntryPointOffsets() const {
        return metadata_.GetValue("tiles_enabled_flag") ||
               metadata_.GetValue("entropy_coding_sync_enabled_flag");
    }

    bool PictureParameterSet::CabacInitPresentFlag() const {
        return static_cast<bool>(metadata_.GetValue("cabac_init_present_flag"));
    }

    void PictureParameterSet::SetTileDimensions(int *dimensions, bool loop_filter_enabled) {
        if (dimensions[0] == tile_dimensions_[0] && dimensions[1] == tile_dimensions_[1]) {
            return;
        }

        assert(dimensions[0] > 0 && dimensions[1] > 0);
        assert(!metadata_.GetValue("tiles_enabled_flag"));
//        assert(tile_dimensions_[0] == 0 && tile_dimensions_[1] == 0);

        // The dimensions are width first, then height, so we must reverse the
        // order from our height, width
        // Next comes a one, and then a 1 if the filter is enabled, 0 if not
        vector<unsigned int> new_dimensions(2);
        new_dimensions[0] = dimensions[1] - 1;
        new_dimensions[1] = dimensions[0] - 1;

        BitArray dimensions_bits = EncodeGolombs(new_dimensions);
        bool using_uniform_tiles = GetContext().GetShouldUseUniformTiles();
        dimensions_bits.Insert(dimensions_bits.size(), using_uniform_tiles ? 1 : 0, 1);

        if (!using_uniform_tiles) {
            // Need to insert bits for widths and heights of tiles.
            // Need to subtract 1 from every width and height.
            auto widths = GetContext().GetWidthsOfTiles();
            std::for_each(widths.begin(), widths.end(), [](unsigned int &w) { --w; });
            auto heights = GetContext().GetHeightsOfTiles();
            std::for_each(heights.begin(), heights.end(), [](unsigned int &h) { --h; });
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
        data_[metadata_.GetValue("tiles_enabled_flag_offset")] = true;
        data_.insert(data_.begin() + metadata_.GetValue("tile_dimensions_offset"), dimensions_bits.begin(), dimensions_bits.end());
        data_.ByteAlignWithoutRemoval();

        tile_dimensions_[0] = dimensions[0];
        tile_dimensions_[1] = dimensions[1];

    }

    void PictureParameterSet::SetPPSId(unsigned int pps_id) {
        // Assume the video already has a pps_id of 0.
        auto current_pps_id = metadata_.GetValue("pps_pic_parameter_set_id");
        if (current_pps_id == pps_id)
            return;

        // Replace the current pps_id with the new one, then re-byte-align.
        auto offset = metadata_.GetValue("pps_pic_parameter_set_id_offset");
        auto current_encoded_pps_id = EncodeGolombs({ current_pps_id });
        auto length_of_current_encoded_pps_id = current_encoded_pps_id.size();

        auto new_encoded_pps_id = EncodeGolombs({ pps_id });

        data_.Replace(offset, offset + length_of_current_encoded_pps_id, new_encoded_pps_id);
        data_.ByteAlign();
    }

    int *PictureParameterSet::GetTileDimensions() {
        return tile_dimensions_;
    }
}

