#ifndef LIGHTDB_SLICESEGMENTLAYER_H
#define LIGHTDB_SLICESEGMENTLAYER_H

#include "Nal.h"
#include "Headers.h"
#include "BitStream.h"
#include "BitArray.h"
#include "Emulation.h"
#include <bitset>


namespace lightdb::hevc {
    // Defined in 7.3.6.1 and 7.3.2.9

    class SliceSegmentLayerMetadata {
    public:
        SliceSegmentLayerMetadata(BitStream &metadata, HeadersMetadata headersMetadata)
            : metadata_(metadata),
            headersMetadata_(headersMetadata)
        {}

//        inline unsigned long PicOutputFlagOffset() const {
//            return metadata_.GetValue("pic_output_flag_offset");
//        }
//
//        inline unsigned long StartOfByteAlignmentBitsOffset() const {
//            return metadata_.GetValue("trailing_bits_offset");
//        };
//
//        inline unsigned long GetEndOffset() const {
//            return metadata_.GetValue("end");
//        }

        void InsertPicOutputFlag(BitArray &data, bool value);

        virtual ~SliceSegmentLayerMetadata() = default;

    protected:
        BitStream &GetBitStream() { return metadata_; }

        BitStream &metadata_;
        HeadersMetadata headersMetadata_;
    };

    class SliceSegmentLayer : public Nal {
    public:

        /**
        * Interprets data as a byte stream representing a SliceSegmentLayer
        * @param context The context surrounding the Nal
        * @param data The byte stream
        * @param headers The headers associated with this segment
        */
        SliceSegmentLayer(const StitchContext &context, const bytestring &data, Headers headers)
                : Nal(context, data),
                  headersMetadata_(headers.GetPicture()->pictureParameterSetMetadata(), headers.GetSequence()->sequenceParameterSetMetadata()),
                  numberOfTranslatedBytes_(std::min((unsigned long)kMaxHeaderLength, data.size())),
                  data_(RemoveEmulationPrevention(data, GetHeaderSize(), numberOfTranslatedBytes_)),
                  headers_(std::move(headers)),
                  metadata_(data_.begin(), data_.begin() + GetHeaderSizeInBits()),
                  address_(0)
        { }

        /**
        * Sets the address of this segment to be address. Should only be called once -
        * if the address is set, should not be reset
        * @param address The new address of this segment
        */
        unsigned long SetAddressAndPPSId(size_t address, unsigned int pps_id);

        /**
        *
        * @return This segment's address
        */
        inline size_t GetAddress() const {
            return address_;
        }

        /**
        *
        * @return A string with the bytes of this Nal
        */
        inline bytestring GetBytes() const override {
            auto headerBytes = AddEmulationPreventionAndMarker(data_, GetHeaderSize(), numberOfTranslatedBytes_ + ceil(numberOfAddedBits_ / 8));
            auto headerBytesSize = headerBytes.size();

            bytestring combinedData(headerBytesSize + byte_data_.size() - numberOfTranslatedBytes_);
            std::move(headerBytes.begin(), headerBytes.end(), combinedData.begin());
            std::copy(byte_data_.begin() + numberOfTranslatedBytes_, byte_data_.end(), combinedData.begin() + headerBytesSize);
            return combinedData;
        }

        inline bytestring GetHeaderBytes() {
            auto end = metadata_.GetValue("end");
            if (metadata_.ValueExists("updated-end-bits"))
                end = metadata_.GetValue("updated-end-bits");

            return AddEmulationPreventionAndMarker(data_, GetHeaderSize(), end, true);
        }

        unsigned long numberOfBytesInHeaderWithUpdatedAddress() const {
            return metadata_.GetValue("updated-end-bits") / 8;
        }

        unsigned long numberOfOriginalBytesInHeader() const {
            return numberOfTranslatedBytes_;
        }

        void InsertPicOutputFlag(bool value);

        SliceSegmentLayer(const SliceSegmentLayer& other) = default;
        SliceSegmentLayer(SliceSegmentLayer&& other) = default;
        ~SliceSegmentLayer() = default;

        unsigned int getEnd() {
            return GetBitStream().GetValue("end");
        }

        unsigned int getAddrOffset() {
            return GetBitStream().GetValue("address_offset");
        }

        unsigned int originalOffsetOfPicOrderCnt() {
            return GetBitStream().GetValue("slice_pic_order_cnt_lsb");
        }

        std::vector<bool> headerUpToPicOrderCntLsb() {
            // Get data_ up to location.
            return std::vector<bool>(data_.begin(), data_.begin() + GetBitStream().GetValue("slice_pic_order_cnt_lsb"));
        }

        std::vector<bool> headerAfterPicOrderCntLsb() {
            return std::vector<bool>(data_.begin() + GetBitStream().GetValue("after_slice_pic_order_cnt_lsb"), data_.begin() + GetBitStream().GetValue("end"));
        }

    protected:
        inline BitStream& GetBitStream() {
            return metadata_;
        }

        HeadersMetadata headersMetadata_;
        unsigned long numberOfAddedBits_;

    private:
        // For the original data as bytes.
        unsigned long numberOfTranslatedBytes_;

        BitArray data_;
        const Headers headers_;
        BitStream metadata_;
        size_t address_;

        static constexpr unsigned int kFirstSliceFlagOffset = 0;
        static constexpr unsigned int kMaxHeaderLength = 24;
    };

    class IDRSliceSegmentLayerMetadata : public SliceSegmentLayerMetadata {
    public:
        IDRSliceSegmentLayerMetadata(BitStream &metadata, HeadersMetadata headersMetadata);
    };

    class IDRSliceSegmentLayer : public SliceSegmentLayer {
    public:

        /**
        * Interprets data as a byte stream representing an IDRSliceSegmentLayer
        * @param context The context surrounding the Nal
        * @param data The byte stream
        * @param headers The headers associated with this segment
        */
        IDRSliceSegmentLayer(const StitchContext &context, const bytestring &data, const Headers &headers);

    private:
        IDRSliceSegmentLayerMetadata idrSliceSegmentLayerMetadata_;
    };

    class TrailRSliceSegmentLayerMetadata : public SliceSegmentLayerMetadata {
    public:
        TrailRSliceSegmentLayerMetadata(BitStream &metadata, HeadersMetadata headersMetadata);
    };

    class TrailRSliceSegmentLayer : public SliceSegmentLayer {
    public:

        /**
        * Interprets data as a byte stream representing a TrailRSliceSegmentLayer
        * @param context The context surrounding the Nal
        * @param data The byte stream
        * @param headers The headers associated with this segment
        */
        TrailRSliceSegmentLayer(const StitchContext &context, const bytestring &data, const Headers &headers);

    private:
        TrailRSliceSegmentLayerMetadata trailRMetadata_;

    };

    class SegmentAddressUpdater {
    public:
        SegmentAddressUpdater(unsigned int address,
                const StitchContext &context, const Headers &headers)
            : address_(address),
                context_(context),
                headers_(headers),
              sizeOfPicOrderGolomb_(headers_.GetSequence()->GetMaxPicOrder()),
              sizeOfAddress_(headers_.GetSequence()->GetAddressLength()),
              offsetOfPicOrder_(0),
            numberOfBytesInPFrameHeader_(0),
            pFrameNumber_(0)
        { }

        const bytestring &updatedSegmentHeader(const bytestring &segment, bool &isKeyframe) {
            isKeyframe = IsKeyframe(segment);
            auto current = Load(context_, segment, headers_);
            current.SetAddressAndPPSId(address_, context_.GetPPSId());
            pFrameHeaderBytes_ = std::move(current.GetBytes());
            numberOfBytesInPFrameHeader_ = current.getEnd() / 8;
            return pFrameHeaderBytes_;
//            if (IsKeyframe(segment)) {
//                // Do it normal because header is different.
//                // Also reset the pFrameHeaderBytes for the new GOP.
//                pFrameHeaderBytes_.clear();
//
//                auto current = Load(context_, segment, headers_);
//                current.SetAddressAndPPSId(address_, context_.GetPPSId());
//                iFrameBytes_ = std::move(current.GetBytes());
//
//                isKeyframe = true;
//                return iFrameBytes_;
//            } else if (!pFrameHeaderBytes_.size()) {
//                // Load the next segment and extract its header bytes.
//                auto pFrame = Load(context_, segment, headers_);
//                auto addedBits = pFrame.SetAddressAndPPSId(address_, context_.GetPPSId());
//
//                offsetOfPicOrder_ = pFrame.originalOffsetOfPicOrderCnt();
////                if (address_)
////                    offsetOfPicOrder_ += sizeOfAddress_; // The address is only added if it's not the first segment.
//                offsetOfPicOrder_ += addedBits;
//
//
//                // TODO: This doesn't account for whether GetHeaderBytes() adds emulation prevention bytes.
//                pFrameHeaderBytes_ = std::move(pFrame.GetHeaderBytes());
//                assert(!(pFrame.getEnd() % 8));
//                numberOfBytesInPFrameHeader_ = pFrame.getEnd() / 8;
//                pFrameNumber_ = 1;
//
//                return pFrameHeaderBytes_;
//            } else {
//                // Update pic_order_lsb.
//                ++pFrameNumber_;
//                updatePicOrderInPFrameBytes();
//                return pFrameHeaderBytes_;
//            }
        }

        unsigned int offsetIntoOriginalPFrameData() const {
            return numberOfBytesInPFrameHeader_;
        }

    private:
        void updatePicOrderInPFrameBytes() {
            UpdatePicOrderCntLsb(pFrameHeaderBytes_, offsetOfPicOrder_, pFrameNumber_, sizeOfPicOrderGolomb_, true);
        }

        const unsigned int address_;
        const StitchContext &context_;
        const Headers &headers_;
        const unsigned int sizeOfPicOrderGolomb_;
        unsigned int sizeOfAddress_;

        unsigned int offsetOfPicOrder_;
        unsigned int numberOfBytesInPFrameHeader_;
        unsigned int pFrameNumber_;
        bytestring pFrameHeaderBytes_;
        bytestring iFrameBytes_;
    };
}; //namespace lightdb::hevc

#endif //LIGHTDB_SLICESEGMENTLAYER_H
