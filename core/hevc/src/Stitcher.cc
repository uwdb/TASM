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

    static void loadSegments(std::vector<bytestring> segments, StitchContext context, Headers headers) {
        auto endLocationForP = -1;
        auto addrLocationForP = -1;
        std::vector<bool> headerUpToPicOrder;
        std::vector<bool> headerAfterPicOrder;
        for (auto it = segments.begin(); it != segments.end(); ++it) {
            auto current = Load(context, *it, headers);
            // Look at current's metadata.
            if (IsKeyframe(*it)) {
                endLocationForP = -1;
                addrLocationForP = -1;
                headerUpToPicOrder.clear();
            } else if (endLocationForP == -1 && addrLocationForP == -1) {
                endLocationForP = current.getEnd();
                addrLocationForP = current.getAddrOffset();
                headerUpToPicOrder = current.headerUpToPicOrderCntLsb();
                headerAfterPicOrder = current.headerAfterPicOrderCntLsb();
            } else {
                assert(current.getEnd() == endLocationForP);
                assert(current.getAddrOffset() == addrLocationForP);
                assert(current.headerUpToPicOrderCntLsb() == headerUpToPicOrder);
                assert(current.headerAfterPicOrderCntLsb() == headerAfterPicOrder);
            }
        }
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

        auto addresses = headers_.GetSequence()->GetAddresses();
        auto header_bytes = headers_.GetBytes();

        // First, collect the segment nals from each tile
        // Create a SegmentAddressUpdater for each tile.
        std::vector<SegmentAddressUpdater> updaters;
        std::vector<std::vector<bytestring>::iterator> segmentDataIterators;
        updaters.reserve(tiles_.size());
        segmentDataIterators.reserve(tiles_.size());
        for (auto i = 0u; i < tiles_.size(); i++) {
            segment_nals.emplace_back(GetSegmentNals(i, &num_bytes, &num_keyframes, i == 0u));
            num_segments += segment_nals.back().size();

            updaters.emplace_back(addresses[i], context_, headers_);
            segmentDataIterators.emplace_back(segment_nals.back().begin());
        }

        // Check whether all p-frames in the same GOP have the same end location & address offset.
        // If so, we can memoize the new address & end.
        // Would be better to memoize entire header & then just fix up the slice_pic_order_cnt_lsb.
        // See if the headers are the same up to that flag, and then after.
//        {
//            unsigned int tileNumber = 0;
//            for (auto it = segment_nals.begin(); it != segment_nals.end(); ++it, ++tileNumber) {
//                loadSegments(*it, context_, headers_);
//            }
//        }

        // Note: this isn't the exact size of the final results since the segments will be extended
        // with the addresses, but this is about as close as we can get. Unfortunately, since it's not
        // the exact size, std::copy cannot be used and we must use insert
        bytestring result;
        result.reserve(header_bytes.size() * num_keyframes + num_bytes);

        auto it = segment_nals.front().begin();
        while (it != segment_nals.front().end()) {
          for (auto i = 0u; i < tile_num; i++) {
            bool isKeyframe = false;
            auto &headerData = updaters[i].updatedSegmentHeader(*segmentDataIterators[i], isKeyframe);
            if (isKeyframe) {
                if (!i)
                    result.insert(result.end(), header_bytes.begin(), header_bytes.end());

                // For keyframes, insert all of the data.
                result.insert(result.end(), headerData.begin(), headerData.end());
            } else {
                result.insert(result.end(), headerData.begin(), headerData.end());
                //  TODO: compensate for size containing header nals / emulation bytes.
                result.insert(result.end(), segmentDataIterators[i]->begin() + updaters[i].offsetIntoOriginalPFrameData(), segmentDataIterators[i]->end());
            }
            ++segmentDataIterators[i];
//
//            if (IsKeyframe(*it)) {
//                // We want to insert the header bytes before each GOP, which is indicated by a keyframe.
//                // So, the before the first keyframe we encounter in each GOP, we will insert the header
//                // bytes
//                if (i == 0) {
//                    result.insert(result.end(), header_bytes.begin(), header_bytes.end());
//                }
//            }
//            current.SetAddress(addresses[i]);
//            auto slice_bytes = current.GetBytes();
//            // Insert the bytes of the slice
//            result.insert(result.end(), slice_bytes.begin(), slice_bytes.end());
          }
          it++;
        }
        return result;
    }

    static BitArray createBitArray(const bytestring &data, bytestring::iterator &currentByte, unsigned int numberOfBytesToTranslate) {
        BitArray bits(0);
        bits.reserve(numberOfBytesToTranslate * 8);
//        auto bitIndex = 0;
        for (auto i = 0u; i < numberOfBytesToTranslate; i++) {
            unsigned char c = *currentByte++;
            if (c == 3)
                continue;

            for (auto j = 0; j < 8; j++)
                bits.push_back((c << j) & 128);
        }

        return bits;
    }

    static void updateBytes(bytestring &data, unsigned int startingByteIndex, unsigned int numberOfBytesToUpdate, const BitArray &updatedBits) {
        auto translatedByteIndex = 0u;
        for (auto i = 0u; i < numberOfBytesToUpdate; ++i) {
            if (data[startingByteIndex+ i] == 3)
                continue;

            data[startingByteIndex + i] = updatedBits.GetByte(translatedByteIndex++);
        }
        assert(translatedByteIndex == updatedBits.size() / 8);
    }

    static void insertPicOutputFlag(bytestring &data, bytestring::iterator startingByte, HeadersMetadata &headersMetadata, unsigned int nalType, bool value) {
        auto numberOfBytesToTranslate = 4;

        auto indexOfStartOfHeader = std::distance(data.begin(), startingByte);
        BitArray sliceHeaderBits = createBitArray(data, startingByte, numberOfBytesToTranslate);
        BitStream parser(sliceHeaderBits.begin(), sliceHeaderBits.begin());

        SliceSegmentLayerMetadata *sliceMetadata = nullptr;
        if (nalType == NalUnitCodedSliceIDRWRADL)
            sliceMetadata = new IDRSliceSegmentLayerMetadata(parser, headersMetadata);
        else
            sliceMetadata = new TrailRSliceSegmentLayerMetadata(parser, headersMetadata);

        sliceMetadata->InsertPicOutputFlag(sliceHeaderBits, value);
        updateBytes(data, indexOfStartOfHeader, numberOfBytesToTranslate, sliceHeaderBits);

        delete sliceMetadata;
    }

    void PicOutputFlagAdder::addPicOutputFlagToGOP(bytestring &gopData, int firstFrameIndex, const std::unordered_set<int> &framesToKeep) {
        bytestring nalPattern = { 0, 0, 0, 1 };

        auto currentFrameIndex = firstFrameIndex;
        auto currentStart = gopData.begin();

        std::unique_ptr<PictureParameterSetMetadata> ppsMetadata;
        std::unique_ptr<SequenceParameterSetMetadata> spsMetadata;
        std::unique_ptr<HeadersMetadata> headersMetadata;

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

                // Start of next nal = size of header.
                auto startOfNextNal = std::search(currentStart, gopData.end(), nalPattern.begin(), nalPattern.end());
                auto sizeOfPPS = std::distance(currentStart, startOfNextNal);
                auto indexOfStartOfHeader = std::distance(gopData.begin(), currentStart);

                BitArray headerBits = createBitArray(gopData, currentStart, sizeOfPPS);
                BitStream parser(headerBits.begin(), headerBits.begin());
                ppsMetadata = std::make_unique<PictureParameterSetMetadata>(parser);
                if (spsMetadata.get()) {
                    assert(!headersMetadata);
                    headersMetadata = std::make_unique<HeadersMetadata>(*ppsMetadata, *spsMetadata);
                }

                if (ppsMetadata->OutputFlagPresentFlag())
                    nalsAlreadyHaveOutputFlagPresentFlag = true;
                else {
                    headerBits[ppsMetadata->GetOutputFlagPresentFlagOffset()] = true;

                    // Convert back into bytes and replace in gopData.
                    updateBytes(gopData, indexOfStartOfHeader, sizeOfPPS, headerBits);
                }
            } else if (nalType == NalUnitSPS) {
                // Get metadata for SPS.
                auto startOfNextNal = std::search(currentStart, gopData.end(), nalPattern.begin(), nalPattern.end());
                auto sizeOfSPS = std::distance(currentStart, startOfNextNal);
                BitArray headerBits = createBitArray(gopData, currentStart, sizeOfSPS);
                BitStream parser(headerBits.begin(), headerBits.begin());
                spsMetadata = std::make_unique<SequenceParameterSetMetadata>(parser);
                if (ppsMetadata.get()) {
                    assert(!headersMetadata.get());
                    headersMetadata = std::make_unique<HeadersMetadata>(*ppsMetadata, *spsMetadata);
                }

            } else if (nalType == NalUnitCodedSliceIDRWRADL || nalType == NalUnitCodedSliceTrailR) {
                // Insert bit and re-byte-align.
                assert(headersMetadata.get());

                if (nalsAlreadyHaveOutputFlagPresentFlag)
                    continue;

                insertPicOutputFlag(gopData, currentStart, *headersMetadata, nalType, framesToKeep.count(currentFrameIndex++));
            }

            currentStart = std::search(currentStart, gopData.end(), nalPattern.begin(), nalPattern.end());
        }
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
        for (auto i = 0u; i < formattedNals_[tileNumber].size(); i++) {
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
