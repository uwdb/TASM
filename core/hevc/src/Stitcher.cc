#include "Stitcher.h"
#include "SliceSegmentLayer.h"
#include <list>

#include "timer.h"

namespace lightdb::hevc {

    //TODO typedef nested type
    std::vector<std::vector<bytestring>> Stitcher::GetNals() {
        tile_nals_.reserve(tiles_.size());
        for (auto tile : tiles_) {
            std::vector<bytestring> nals;
            auto zero_count = 0u;
            auto first = true;
            auto start = tile.begin();
            for (auto it = tile.begin(); it != tile.end(); it++) {
                auto c = static_cast<unsigned char>(*it);
                // We found the nal marker "0001"
                if (c == 1 && zero_count >= 3) {
                    // Since each stream will start with 0001, the first segment will always be empty,
                    // so we want to just discard it
                    if (!first) {
                        // -3, not -4, because the constructor is exclusive [first, last)
                        nals.push_back(move(bytestring(make_move_iterator(start), make_move_iterator(it - 3))));
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
            nals.push_back(move(bytestring(make_move_iterator(start), make_move_iterator(tile.end()))));
            tile_nals_.push_back(nals);
        }
        return tile_nals_;
    }

    std::vector<bytestring> Stitcher::GetSegmentNals(const unsigned long tile_num, unsigned long *num_bytes, unsigned long *num_keyframes, bool first) {
        auto nals = tile_nals_[tile_num];
        std::vector<bytestring> segments;
        for (auto nal : nals) {
            if (IsSegment(nal)) {
                if (IsKeyframe(nal) && first) {
                    (*num_keyframes)++;
                }
                *num_bytes += nal.size();
                segments.push_back(move(nal));

            }
        }
        return segments;
    }

    bytestring Stitcher::GetStitchedSegments() {
        headers_.GetSequence()->SetDimensions(context_.GetVideoDimensions());
        headers_.GetSequence()->SetGeneralLevelIDC(120);
        headers_.GetVideo()->SetGeneralLevelIDC(120);
        headers_.GetPicture()->SetTileDimensions(context_.GetTileDimensions());

        auto tile_num = tiles_.size();
        std::list<std::vector<bytestring>> segment_nals;
        auto num_segments = 0u;
        unsigned long num_bytes = 0u;
        unsigned long num_keyframes = 0u;

        // First, collect the segment nals from each tile
        for (auto i = 0u; i < tiles_.size(); i++) {
            auto segments = GetSegmentNals(i, &num_bytes, &num_keyframes, i == 0u);
            segment_nals.push_back(segments);
            num_segments += segments.size();
        }

        auto tile_index = 0u;
        std::vector<bytestring> stitched(num_segments);

        // Next, insert each segment nal from each tile in the appropriate
        // place in the final vector. If we have 3 tiles with 5 segments each,
        // then the segments for tile 0 will go in index 0, 3, 6, 9, 12, the
        // segments for tile 1 will go in index 1, 4, 7, 10, 11, the segments
        // j for tile i will go in index number of tiles * j + i
        for (auto &nals : segment_nals) {
          // Go through the segment nals for a given tile
          for (auto i = 0u; i < nals.size(); i++) {
            stitched[tile_num * i + tile_index] = move(nals[i]);
          }
          // Update the tile index to move to the segment for the next tile
          tile_index++;
        }

        auto addresses = headers_.GetSequence()->GetAddresses();
        auto header_bytes = headers_.GetBytes();
        // Note: this isn't the exact size of the final results since the segments will be extended
        // with the addresses, but this is about as close as we can get. Unfortunately, since it's not
        // the exact size, std::copy cannot be used and we must use insert
        bytestring result(header_bytes.size() * num_keyframes + num_bytes);

        auto it = stitched.begin();
        while (it != stitched.end()) {
          for (auto i = 0u; i < tile_num; i++) {
            auto current = Load(context_, *it, headers_);
            if (IsKeyframe(*it)) {
                // We want to insert the header bytes before each GOP, which is indicated by a keyframe.
                // So, the before the first keyframe we encounter in each GOP, we will insert the header
                // bytes
                if (i == 0) {
                    result.insert(result.end(), header_bytes.begin(), header_bytes.end());
                }
            }
            current.SetAddress(addresses[i]);
            auto slice_bytes = current.GetBytes();
            // Insert the bytes of the slice
            result.insert(result.end(), slice_bytes.begin(), slice_bytes.end());
            it++;
          }
        }
        return result;
    }

    void PicOutputFlagAdder::addPicOutputFlagToGOP(bytestring &gopData) {
        bytestring nalPattern = { 0, 0, 0, 1 };

//        Timer timer;
//        // Convert bytestring to bits.
//        // Dont' have to convert the entire bytestring, just the header bits I guess.
//        // Can flip the bit in PPS without converting.
//        timer.startSection("ConvertToBits");
//        BitArray gopBits(gopData.size() * 8);
//        auto bitIndex = 0u;
//        for (const auto &c: gopData) {
//            for (auto i = 0u; i < 8; i++)
//                gopBits[bitIndex++] = (c << i) & 128;
//        }
//        timer.endSection("ConvertToBits");

        auto currentStart = gopData.begin();

        bool nalsAlreadyHaveOutputFlagPresentFlag = false;
        while (currentStart != gopData.end()) {
            // Move the iterator past the nal header.
            currentStart += 4;
            auto nalType = PeekType(*currentStart);

            // Advance past the header.
            currentStart += GetHeaderSize();

            if (nalType == NalUnitPPS) {
                // Flip bit if it's not already set.
                // Need to find it first …

                // Converting 2 bytes to bits should be sufficient to get to output_flag_present_flag.
                BitArray headerBits(16);
                auto indexOfStartOfHeader = std::distance(gopData.begin(), currentStart);
                auto bitIndex = 0;
                for (auto i = 0; i < 2; ++i) {
                    auto c = *currentStart++;
                    for (auto j = 0; j < 8; j++) {
                        headerBits[bitIndex++] = (c << j) & 128;
                    }
                }
                BitStream parser(headerBits.begin(), headerBits.begin());
                parser.SkipExponentialGolomb(); // pic_parameter_set_id
                parser.SkipExponentialGolomb(); // seq_parameter_set_id
                parser.SkipBits(1); // dependent_slice_segments_enabled_flag
                parser.MarkPosition("output_flag_present_flag_offset");
                parser.CollectValue("output_flag_present_flag");

                if (parser.GetValue("output_flag_present_flag"))
                    nalsAlreadyHaveOutputFlagPresentFlag = true;
                else {
                    headerBits[parser.GetValue("output_flag_present_flag_offset")] = true;

                    // Convert back into bytes and replace in gopData.
                    for (auto i = 0; i < 2; ++i)
                        gopData[indexOfStartOfHeader + i] = headerBits.GetByte(i);
                }

            } else if (nalType == NalUnitCodedSliceIDRWRADL) {
                // Insert bit and re-byte-align.


            } else if (nalType == NalUnitCodedSliceTrailR) {
                // Insert bit and re-byte-align.
            }

            currentStart = std::search(currentStart, gopData.end(), nalPattern.begin(), nalPattern.end());
        }

//        // Translate back into a bytestring.
//        timer.startSection("ConvertToBytes");
//        assert(!(gopBits.size() % 8));
//        auto numberOfBytes = gopBits.size() / 8;
//        bytestring bytes(numberOfBytes);
//        for (auto i = 0; i < numberOfBytes; ++i)
//            bytes[i] = gopBits.GetByte(i);
//        timer.endSection("ConvertToBytes");
//
//        timer.printAllTimes();
    }

    void Stitcher::addPicOutputFlagIfNecessaryKeepingFrames(const std::unordered_set<int> &framesToKeep) {
        for (const auto &nals : tile_nals_) {
            Timer timer;
            timer.startSection("CreateNals");
            std::vector<std::unique_ptr<Nal>> nalObjects;
            nalObjects.reserve(nals.size());
            for (const auto &nalData : nals) {
                nalObjects.emplace_back(LoadNal(context_, nalData, headers_));
            }
            timer.endSection("CreateNals");

            // Now go through nals.
            // Flip bit in PPS, if necessary.
            // Add bit in slices, if necessary.
            timer.startSection("AddPicOutputFlagToNals");
            auto currentFrameNumber = 0;
            bool outputFlagIsNotPresentInGOP = false;
            for (auto it = nalObjects.begin(); it != nalObjects.end(); it++) {
                if ((*it)->IsSequence())
                    continue;

                else if ((*it)->IsVideo())
                    continue;

                else if ((*it)->IsPicture()) {
//                    auto pps = dynamic_cast<PictureParameterSet&>(**it);
                    outputFlagIsNotPresentInGOP = static_cast<PictureParameterSet*>(it->get())->TryToTurnOnOutputFlagPresentFlag();
                } else if (((*it)->IsIDRSegment() || (*it)->IsTrailRSegment()) && outputFlagIsNotPresentInGOP) {
                    // Insert pic_output_bit.
                    static_cast<SliceSegmentLayer*>(it->get())->InsertPicOutputFlag(framesToKeep.count(currentFrameNumber++));
                } else
                    assert(false);
            }
            timer.endSection("AddPicOutputFlagToNals");
            timer.printAllTimes();
            formattedNals_.push_back(std::move(nalObjects));
        }
    }

    bytestring Stitcher::combinedNalsForTile(unsigned int tileNumber) const {
        unsigned int totalSize = 0;
        std::vector<bytestring> bytes(formattedNals_[tileNumber].size());
        for (auto i = 0; i < formattedNals_[tileNumber].size(); i++) {
            auto nalBytes = formattedNals_[tileNumber][i]->GetBytes();
            bytes[i].resize(nalBytes.size());
            totalSize += nalBytes.size();
            std::move(nalBytes.begin(), nalBytes.end(), bytes[i].begin());
        }

        bytestring allData(totalSize);
        auto currentEnd = allData.begin();
        for (auto &nalBytes : bytes) {
            currentEnd = std::move(nalBytes.begin(), nalBytes.end(), currentEnd);
        }

        return allData;
    }
}; //namespace lightdb::hevc
