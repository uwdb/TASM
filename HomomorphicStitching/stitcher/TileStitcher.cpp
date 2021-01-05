//
// Created by sophi on 4/9/2018.
//

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <algorithm>

#include "TileStitcher.h"
#include "Nal.h"
#include "Context.h"
#include "SliceSegmentLayer.h"
#include "SequenceParameterSet.h"
#include "VideoParameterSet.h"
#include "PictureParameterSet.h"
#include "Headers.h"
#include "Golombs.h"

using std::list;
using std::vector;
using std::string;

namespace lightdb::tiles {

  // TODO: Figure out return type for stitcher, fix the fact that the vector and list are holding
  // nals when they should hold whatever Tile returns, fix the fact that I'm using references
  // in that case, fix the fact that getsequence is called a bunch

    Stitcher::Stitcher(const tiles::Context &context, vector<bytestring> &data) : tiles_(data), context_(context), headers_(context_, GetNals(tiles_.front()))  {
    }

    vector<bytestring> Stitcher::GetNals(const bytestring &tile) {
        vector<bytestring> nals;
        auto zero_count = 0u;
        bool first = true;
        auto start = tile.begin();
        for (bytestring::const_iterator it = tile.begin(); it != tile.end(); it++) {
            auto c = static_cast<unsigned char>(*it);
            if (c == 1 && zero_count >= 3) {
                if (!first) {
                    // -3, not -4, because the constructor is exclusive [first, last)
                    nals.push_back(bytestring(start, it - 3));
                } else {
                    first = false;
                }
                zero_count = 0;
                start = it + 1;
            } else if (c == 0) {
                zero_count++;
            } else {
                zero_count = 0;
            }
        }
        nals.push_back(bytestring(start, tile.end()));
        return nals;
    }

    vector<bytestring> Stitcher::GetSegmentNals(const bytestring &tile) {
        vector<bytestring> nals = GetNals(tile);
        vector<bytestring> segments;
        for (vector<bytestring>::const_iterator it = nals.begin() + 1; it != nals.end(); it++) {
            if (IsSegment(*it)) {
                segments.push_back(*it);
            }
        }
        return segments;
    }

    bytestring Stitcher::GetStitchedSegments() {
        // Update conformance window before dimensions because they come later in the header.
        headers_.GetSequence()->SetConformanceWindow(context_.GetVideoDisplayWidth(), context_.GetVideoCodedWidth(),
                context_.GetVideoDisplayHeight(), context_.GetVideoCodedHeight());
        headers_.GetSequence()->SetDimensions(context_.GetVideoDimensions()); // appears to be video dimensions (command line arg)
        headers_.GetSequence()->SetGeneralLevelIDC(120);
        headers_.GetVideo()->SetGeneralLevelIDC(120);
        headers_.GetPicture()->SetTileDimensions(context_.GetTileDimensions()); // appears to be tile dimensions (command line arg)
        // Set PPS_Id after setting tile dimensions to avoid issues with the offsets changing.
        headers_.GetPicture()->SetPPSId(context_.GetPPSId());


        // how expensive is vector copy construction? does it use move?
        unsigned long tile_num = tiles_.size();
        list<vector<bytestring>> tile_nals;
        auto num_segments = 0u;

        // First, collect the segment nals from each tile
        for (auto &tile : tiles_) {
            auto segments = GetSegmentNals(tile);
            tile_nals.push_back(segments);
            num_segments += segments.size();
        }

        int tile_index = 0;
        vector<bytestring> stitched(num_segments);

        // Next, insert each segment nal from each tile in the appropriate
        // place in the final vector. If we have 3 tiles with 5 segments each,
        // then the segments for tile 0 will go in index 0, 3, 6, 9, 12, the
        // segments for tile 1 will go in index 1, 4, 7, 10, 11, the segments
        // j for tile i will go in index number of tiles * j + i
        for (auto &nals : tile_nals) {
          // Go through the segment nals for a given tile
          for (int i = 0; i < nals.size(); i++) {
            stitched[tile_num * i + tile_index] = nals[i];
          }
          // Update the tile index to move to the segment for the next tile
          tile_index++;
        }

        bytestring result;
        auto addresses = headers_.GetSequence()->GetAddresses();

        // First, add the header bytes
        auto header_bytes = headers_.GetBytes();
        result.insert(result.begin(), header_bytes.begin(), header_bytes.end());

        // Add GetActiveParameterSetsSEI if there is a new PPS / tile layout.
        if (context_.GetPPSId()) {
            auto activeParameterSEIBytes = GetActiveParameterSetsSEI();
            result.insert(result.end(), activeParameterSEIBytes.begin(), activeParameterSEIBytes.end());
        }

        // Now, add each slice segment
        bool first = true;
        vector<bytestring>::iterator it = stitched.begin();
        while (it != stitched.end()) {
          for (int i = 0; i < tile_num; i++) {
            SliceSegmentLayer *current_slice = reinterpret_cast<SliceSegmentLayer*>(Load(context_, *it, headers_));
            if (IsKeyframe(*it)) {
                if (first) {
                    if (i == tile_num - 1) {
                        first = false;
                    }
                } else if (i == 0) {
                    result.insert(result.end(), header_bytes.begin(), header_bytes.end());
                }
            }
            current_slice->SetAddressAndPPSId(addresses[i], context_.GetPPSId());
//            current_slice->SetPPSId(context_.GetPPSId()); // Call this after setting address to avoid messing up offsets.
            auto slice_bytes = current_slice->GetBytes();
            result.insert(result.end(), slice_bytes.begin(), slice_bytes.end());
            it++;
          }
        }
        return result;
    }

    static bytestring GetPrefixSEINut() {
        auto prefixBytes = GetNalMarker();
        unsigned int prefixType = 39;
        auto prefixByte = prefixType << 1;

//        unsigned int nuh_layer_id = 0;
        unsigned int nuh_temporal_id_plus1 = 1;

        prefixBytes.insert(prefixBytes.end(), prefixByte);
        prefixBytes.insert(prefixBytes.end(), nuh_temporal_id_plus1);

        return prefixBytes;
    }

    bytestring Stitcher::GetActiveParameterSetsSEI() {
        unsigned int payloadType = 129;
        unsigned int payloadSize = 2;

        unsigned int active_video_parameter_set_id = 0;
        unsigned int self_contained_cvs_flag = 0;
        unsigned int no_parameter_set_update_flag = 0;
        unsigned int num_sps_ids_minus1 = 0;
        unsigned int active_seq_parameter_set_id = 0;
        unsigned int layer_sps_idx = 0;

        utility::BitArray payloadBits(8);
        payloadBits.SetByte(0, payloadType);

        payloadBits.Insert(payloadBits.size(), payloadSize, 8);
        payloadBits.Insert(payloadBits.size(), active_video_parameter_set_id, 4);
        payloadBits.Insert(payloadBits.size(), self_contained_cvs_flag, 1);
        payloadBits.Insert(payloadBits.size(), no_parameter_set_update_flag, 1);

        utility::BitArray numSpsIdsMinus1Bits = EncodeGolombs({ num_sps_ids_minus1 });
        payloadBits.insert(payloadBits.end(), numSpsIdsMinus1Bits.begin(), numSpsIdsMinus1Bits.end());

        utility::BitArray activeSeqParameterSetBits = EncodeGolombs({ active_seq_parameter_set_id });
        payloadBits.insert(payloadBits.end(), activeSeqParameterSetBits.begin(), activeSeqParameterSetBits.end());

        utility::BitArray layerSpsIdxBits = EncodeGolombs({ layer_sps_idx });
        payloadBits.insert(payloadBits.end(), layerSpsIdxBits.begin(), layerSpsIdxBits.end());

        payloadBits.Insert(payloadBits.size(), 1, 1); // Payload bits I guess?
        payloadBits.ByteAlignWithoutRemoval();

        payloadBits.Insert(payloadBits.size(), 1, 1); // 1-bit of RBSP trailing bits.
        payloadBits.ByteAlignWithoutRemoval();

        // Convert to bytes.
        auto numberOfBytes = payloadBits.size() / 8;
        bytestring payloadBytes(numberOfBytes);
        for (auto i = 0u; i < numberOfBytes; i++)
            payloadBytes[i] = payloadBits.GetByte(i);

        auto nalUnitBytes = GetPrefixSEINut();
        nalUnitBytes.insert(nalUnitBytes.end(), payloadBytes.begin(), payloadBytes.end());

        return nalUnitBytes;
    }
} // namespace lightdb::tiles
