#ifndef LIGHTDB_SLICESEGMENTLAYER_H
#define LIGHTDB_SLICESEGMENTLAYER_H

#include "Nal.h"
#include "Headers.h"
#include "BitStream.h"
#include "BitArray.h"
#include "Emulation.h"


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
                  data_(RemoveEmulationPrevention(data, GetHeaderSize(), kMaxHeaderLength)),
                  headers_(std::move(headers)),
                  metadata_(data_.begin(), data_.begin() + GetHeaderSizeInBits()),
                  address_(0)
        { }

        /**
        * Sets the address of this segment to be address. Should only be called once -
        * if the address is set, should not be reset
        * @param address The new address of this segment
        */
        void SetAddress(size_t address);

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
            return AddEmulationPreventionAndMarker(data_, GetHeaderSize(), kMaxHeaderLength);
        }

        void InsertPicOutputFlag(bool value);

        SliceSegmentLayer(const SliceSegmentLayer& other) = default;
        SliceSegmentLayer(SliceSegmentLayer&& other) = default;
        ~SliceSegmentLayer() = default;

    protected:
        inline BitStream& GetBitStream() {
            return metadata_;
        }

        HeadersMetadata headersMetadata_;

    private:
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
}; //namespace lightdb::hevc

#endif //LIGHTDB_SLICESEGMENTLAYER_H
