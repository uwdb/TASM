#include "Stitcher.h"
#include "SliceSegmentLayer.h"
#include <list>

#include "timer.h"

namespace lightdb::hevc {

    //TODO typedef nested type
    const std::vector<std::list<bytestring>> &Stitcher::GetNals(std::vector<bytestring> &data) {
        tile_nals_.resize(data.size());
        auto index = 0u;
        for (auto &tile : data) {
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
                        tile_nals_[index].push_back(move(bytestring(make_move_iterator(start), make_move_iterator(it - 3))));
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
            tile_nals_[index].push_back(move(bytestring(make_move_iterator(start), make_move_iterator(tile.end()))));
            ++index;
        }
        return tile_nals_;
    }

    std::list<bytestring> Stitcher::GetSegmentNals(const unsigned long tile_num, unsigned long *num_bytes, unsigned long *num_keyframes, bool first) {
        auto &nals = tile_nals_[tile_num];
        std::list<bytestring> segments;
        for (auto &nal : nals) {
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
                assert(current.getEnd() == (unsigned int)endLocationForP);
                assert(current.getAddrOffset() == (unsigned int)addrLocationForP);
                assert(current.headerUpToPicOrderCntLsb() == headerUpToPicOrder);
                assert(current.headerAfterPicOrderCntLsb() == headerAfterPicOrder);
            }
        }
    }

    std::shared_ptr<bytestring> GetPrefixSEINut() {
        auto marker = Nal::GetNalMarker();
        std::shared_ptr<bytestring> prefixBytes(new bytestring(marker.begin(), marker.end()));
        unsigned int prefixType = 39;
        auto prefixByte = prefixType << 1;

    //        unsigned int nuh_layer_id = 0;
        unsigned int nuh_temporal_id_plus1 = 1;

        prefixBytes->insert(prefixBytes->end(), prefixByte);
        prefixBytes->insert(prefixBytes->end(), nuh_temporal_id_plus1);

        return prefixBytes;
    }

    std::shared_ptr<bytestring> Stitcher::Stitcher::GetActiveParameterSetsSEI() {
        if (activeParameterSetsSEI_)
            return activeParameterSetsSEI_;

        unsigned int payloadType = 129;
        unsigned int payloadSize = 2;

        unsigned int active_video_parameter_set_id = 0;
        unsigned int self_contained_cvs_flag = 0;
        unsigned int no_parameter_set_update_flag = 0;
        unsigned int num_sps_ids_minus1 = 0;
        unsigned int active_seq_parameter_set_id = 0;
        unsigned int layer_sps_idx = 0;

        BitArray payloadBits(8);
        payloadBits.SetByte(0, payloadType);

        payloadBits.Insert(payloadBits.size(), payloadSize, 8);
        payloadBits.Insert(payloadBits.size(), active_video_parameter_set_id, 4);
        payloadBits.Insert(payloadBits.size(), self_contained_cvs_flag, 1);
        payloadBits.Insert(payloadBits.size(), no_parameter_set_update_flag, 1);

        BitArray numSpsIdsMinus1Bits = EncodeGolombs({ num_sps_ids_minus1 });
        payloadBits.insert(payloadBits.end(), numSpsIdsMinus1Bits.begin(), numSpsIdsMinus1Bits.end());

        BitArray activeSeqParameterSetBits = EncodeGolombs({ active_seq_parameter_set_id });
        payloadBits.insert(payloadBits.end(), activeSeqParameterSetBits.begin(), activeSeqParameterSetBits.end());

        BitArray layerSpsIdxBits = EncodeGolombs({ layer_sps_idx });
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

        auto activeParameterSetsSEI_ = GetPrefixSEINut();
        activeParameterSetsSEI_->insert(activeParameterSetsSEI_->end(), payloadBytes.begin(), payloadBytes.end());

        return activeParameterSetsSEI_;
    }

    std::unique_ptr<bytestring> Stitcher::GetStitchedSegments() {
        headers_.GetSequence()->SetConformanceWindow(context_.GetVideoDisplayWidth(), context_.GetVideoCodedWidth(),
                                                     context_.GetVideoDisplayHeight(), context_.GetVideoCodedHeight());
        headers_.GetSequence()->SetDimensions(context_.GetVideoDimensions());
        headers_.GetSequence()->SetGeneralLevelIDC(120);
        headers_.GetVideo()->SetGeneralLevelIDC(120);
        headers_.GetPicture()->SetTileDimensions(context_.GetTileDimensions());
        // Set PPS_Id after setting tile dimensions to avoid issues with the offsets changing.
        headers_.GetPicture()->SetPPSId(context_.GetPPSId());

        auto numberOfTiles = tile_nals_.size();
        std::list<std::list<bytestring>> segment_nals;
        auto num_segments = 0u;
        unsigned long num_bytes = 0u;
        unsigned long num_keyframes = 0u;

        auto addresses = headers_.GetSequence()->GetAddresses();
        auto header_bytes = headers_.GetBytes();

        // First, collect the segment nals from each tile
        // Create a SegmentAddressUpdater for each tile.
        std::vector<SegmentAddressUpdater> updaters;
        std::vector<std::list<bytestring>::iterator> segmentDataIterators;
        updaters.reserve(numberOfTiles);
        segmentDataIterators.reserve(numberOfTiles);
        for (auto i = 0u; i < numberOfTiles; i++) {
            segment_nals.emplace_back(GetSegmentNals(i, &num_bytes, &num_keyframes, i == 0u));
            num_segments += segment_nals.back().size();

            updaters.emplace_back(addresses[i], context_, headers_);
            segmentDataIterators.emplace_back(segment_nals.back().begin());
        }

        std::unique_ptr<bytestring> result(new bytestring);
        auto seiSize = context_.GetPPSId() ? GetActiveParameterSetsSEI()->size() : 0;
        result->reserve(header_bytes.size() * num_keyframes + num_bytes + seiSize);

        auto it = segment_nals.front().begin();
        bool first = true;
        while (it != segment_nals.front().end()) {
          for (auto i = 0u; i < numberOfTiles; i++) {
            bool isKeyframe = false;
            auto &headerData = updaters[i].updatedSegmentHeader(*segmentDataIterators[i], isKeyframe);
            if (isKeyframe) {
                if (!i) {
                    result->insert(result->end(), header_bytes.begin(), header_bytes.end());

                    if (first && context_.GetPPSId()) {
                        first = false;
                        auto activeParameterSEIBytes = GetActiveParameterSetsSEI();
                        result->insert(result->end(), activeParameterSEIBytes->begin(), activeParameterSEIBytes->end());
                    }
                }

                // For keyframes, insert all of the data.
                result->insert(result->end(), headerData.begin(), headerData.end());
            } else {
                result->insert(result->end(), headerData.begin(), headerData.end());
                //  TODO: compensate for size containing header nals / emulation bytes.
                result->insert(result->end(), segmentDataIterators[i]->begin() + updaters[i].offsetIntoOriginalPFrameData(), segmentDataIterators[i]->end());
            }
            ++segmentDataIterators[i];
          }
          it++;
        }
        return result;
    }

    static BitArray createBitArray(const bytestring &data, bytestring::iterator &currentByte, unsigned int numberOfBytesToTranslate) {
        BitArray bits(0);
        bits.reserve(numberOfBytesToTranslate * 8);
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

    SliceSegmentLayer Stitcher::loadPFrameSegment(lightdb::bytestring &data) {
        return TrailRSliceSegmentLayer(context_, data, headers_);
    }

void IdenticalFrameRetriever::getPFrameData(Stitcher &stitcher) {
    unsigned long numBytes;
    unsigned long numKeyframes;
    auto segments = stitcher.GetSegmentNals(0, &numBytes, &numKeyframes, false);
    assert(segments.size() == 2);
    assert(!IsKeyframe(segments.back()));

    auto pFrameSegment = stitcher.loadPFrameSegment(segments.back());

    picOutputCntLsbBitOffset_ = pFrameSegment.originalOffsetOfPicOrderCnt();
    numberOfBitsForPicOutputCntLsb_ = stitcher.headers_.GetSequence()->GetMaxPicOrder();

    pFrameHeader_ = pFrameSegment.GetHeaderBytes();

    auto endOfHeader = pFrameSegment.getEnd() / 8;
    auto sizeOfData = segments.back().size() - endOfHeader;
    pFrameData_.reserve(sizeOfData);
    std::copy(segments.back().begin() + endOfHeader, segments.back().end(), pFrameData_.begin());
}
}; //namespace lightdb::hevc
