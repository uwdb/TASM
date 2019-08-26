#ifndef LIGHTDB_PICTUREPARAMETERSET_H
#define LIGHTDB_PICTUREPARAMETERSET_H

#include "Nal.h"
#include "BitArray.h"
#include "BitStream.h"
#include "Emulation.h"

namespace lightdb::hevc {
    // Defined in 7.3.2.3
    // Should there be a "metadata" base class for all nal types?
    class PictureParameterSetMetadata {
    public:
        PictureParameterSetMetadata(BitStream metadata)
            : metadata_(metadata)
        {
            metadata_.SkipExponentialGolomb();
            metadata_.SkipExponentialGolomb();
            metadata_.SkipBits(1); // dependent_slice_segments_enabled_flag
            metadata_.MarkPosition("output_flag_present_flag_offset");
            metadata_.CollectValue("output_flag_present_flag");
            metadata_.SkipBits(4);
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

            CHECK_EQ(metadata_.GetValue("end") % 8, 0);
        }

        const BitStream &metadata() const { return metadata_; }

        inline bool HasEntryPointOffsets() const {
            return metadata_.GetValue("tiles_enabled_flag") ||
                   metadata_.GetValue("entropy_coding_sync_enabled_flag");
        }

        inline bool CabacInitPresentFlag() const {
            return static_cast<bool>(metadata_.GetValue("cabac_init_present_flag"));
        }

        inline bool OutputFlagPresentFlag() const {
            return metadata_.GetValue("output_flag_present_flag");
        }

        inline unsigned long GetOutputFlagPresentFlagOffset() const {
            return metadata_.GetValue("output_flag_present_flag_offset");
        }

    private:
        // Ideal would be for Metadata to own metadata object, and PPS accesses it through it.
        // This is fast for seeing if idea works.
        BitStream metadata_;
    };

    class PictureParameterSet : public Nal {
    public:
        /**
         * Interprets data as a byte stream representing a PictureParameterSet
         * @param context The context surrounding the Nal
         * @param data The byte stream
         */
        PictureParameterSet(const StitchContext &context, const bytestring &data)
                : Nal(context, data),
                  data_(RemoveEmulationPrevention(data, GetHeaderSize(), data.size())),
                  ppsMetadata_({data_.begin(), data_.begin() + GetHeaderSizeInBits()}),
                  tile_dimensions_{0, 0} {

        }

        /**
         * Sets the tile dimensions in the byte stream to be dimensions
         * @param dimensions A length two array, the first element being the height (num of rows) and the second
         * being the width (num of columns)
         * @param loop_filter_enabled Set to false unless otherwise specified
         */
        void SetTileDimensions(const std::pair<unsigned long, unsigned long>& dimensions,
                               bool loop_filter_enabled = false);

        /**
         *
         * @return An array representing the tile dimensions, height first, then width.
         * Note that changing this array changes this header's tile dimensions
         */
        inline const std::pair<unsigned long, unsigned long>& GetTileDimensions() const {
            return tile_dimensions_;
        }

        /**
         *
         * @return True if this byte stream has entry point offsets, false otherwise
         */
        inline bool HasEntryPointOffsets() const {
            return ppsMetadata_.HasEntryPointOffsets();
//            return metadata_.GetValue("tiles_enabled_flag") ||
//                   metadata_.GetValue("entropy_coding_sync_enabled_flag");
        }

        /**
         *
         * @return True if the cabac_init_present_flag is set to true in this byte stream.
         * false otherwise
         */
	    inline bool CabacInitPresentFlag() const {
	        return ppsMetadata_.HasEntryPointOffsets();
//            return static_cast<bool>(metadata_.GetValue("cabac_init_present_flag"));
        }

        /**
         *
         * @return A string wtih the bytes of this Nal
         */
        inline bytestring GetBytes() const override {
            return AddEmulationPreventionAndMarker(data_, GetHeaderSize(), data_.size() / 8);
        }

        /**
         *
         * @return True if the output flag was not already enabled. False if the the output flag is already present.
         */
        bool TryToTurnOnOutputFlagPresentFlag();

        bool HasOutputFlagPresentFlagEnabled();

        inline PictureParameterSetMetadata pictureParameterSetMetadata() const { return ppsMetadata_; }

    private:
        unsigned long getMetadataValue(const std::string &key) const {
            return ppsMetadata_.metadata().GetValue(key);
        }

        BitArray data_;
        PictureParameterSetMetadata ppsMetadata_;
        std::pair<unsigned long, unsigned long> tile_dimensions_;
    };
}; //namespace lightdb::hevc

#endif //LIGHTDB_PICTUREPARAMETERSET_H
