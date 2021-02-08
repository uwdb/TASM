#include "Emulation.h"
#include "BitStream.h"
#include "PictureParameterSet.h"
#include "SequenceParameterSet.h"
#include "SliceSegmentLayer.h"

namespace stitching {

    unsigned long SliceSegmentLayer::SetAddressAndPPSId(const size_t address, const unsigned int pps_id) {
        unsigned long numberOfAddedBitsBeforePicOrder_ = 0;

        address_ = address;
        // Make sure it's byte aligned
        assert (metadata_.GetValue("end") % 8 == 0);

        // The header is the end of the metadata, so move the portion before that into
        // the header array
        auto header_end = metadata_.GetValue("end");
        BitArray header_bits(header_end);
        move(data_.begin(), data_.begin() + header_end, header_bits.begin());

        // Convert the header size from bytes to bits, add the offset
        header_bits[GetHeaderSize() * 8 + kFirstSliceFlagOffset] = address == 0;

        auto address_length = headers_.GetSequence()->GetAddressLength();
        // If the address is 0, we have nothing to insert (it's the first slice)
        if (address) {
            // Note that BitArray has to convert the address to binary and pad it
            header_bits.Insert(metadata_.GetValue("address_offset"), address, address_length);
            numberOfAddedBits_ += address_length;
            numberOfAddedBitsBeforePicOrder_ += address_length;
        }

        if (!headers_.GetPicture()->HasEntryPointOffsets()) {
            auto location = metadata_.GetValue("entry_point_offset");
            if (address) {
                location += address_length;
            }
            // Insert a 0 as an exponential golomb, meaning the one bit '1'
            header_bits.Insert(location, 1, 1);
            ++numberOfAddedBits_;
        }

        // Now do PPS setting.
        auto current_pps_id = metadata_.GetValue("slice_pic_parameter_set_id");
        if (current_pps_id != pps_id) {
            auto size_of_current_encoded_pps_id = EncodeGolombs({current_pps_id}).size();
            auto current_pps_id_offset = metadata_.GetValue("slice_pic_parameter_set_id_offset");
            auto new_encoded_pps_id = EncodeGolombs({pps_id});

            header_bits.Replace(current_pps_id_offset, current_pps_id_offset + size_of_current_encoded_pps_id, new_encoded_pps_id);
            auto addedBits = new_encoded_pps_id.size() - size_of_current_encoded_pps_id;
            numberOfAddedBits_ += addedBits;
            numberOfAddedBitsBeforePicOrder_ += addedBits;
        }

        header_bits.ByteAlign();
        assert(!(header_bits.size() % 8));

        // Keep track of where the updated end of header is.
        metadata_.SetValue("updated-end-bits", header_bits.size());

        // The new size of the data is the size of the new header plus the size of the data
        // minus the old header size
        BitArray new_data(header_bits.size() + data_.size() - header_end);
        move(header_bits.begin(), header_bits.end(), new_data.begin());
        move(data_.begin() + header_end, data_.end(), new_data.begin() + header_bits.size());
        data_ = new_data;

        return numberOfAddedBitsBeforePicOrder_;
    }

    void SliceSegmentLayer::InsertPicOutputFlag(bool value) {
        auto position = metadata_.GetValue("pic_output_flag_offset");
        data_.insert(data_.begin() + position, value);

        auto startOfByteAlignmentBits = metadata_.GetValue("trailing_bits_offset");
        auto indexFollowingTrailing1 = startOfByteAlignmentBits + 2; // +1 because we inserted a bit, +1 to get next element.
        auto existingNumberOfTrailing0s = 8 - 1 - (startOfByteAlignmentBits % 8);

        if (existingNumberOfTrailing0s)
            data_.erase(data_.begin() + indexFollowingTrailing1);
        else
            data_.insert(data_.begin() + indexFollowingTrailing1, 7, false);

    }

    void SliceSegmentLayerMetadata::InsertPicOutputFlag(BitArray &data, bool value) {
        // Insert pic_output_flag_offset.
        // Need to decode bytes from pic_output_flag_offset -> end.
        // Find byte offset for pic_output_flag_offset & byte offset for end.
        // Translate into bits.
        // Insert bit for pic_output_flag.
        // Truncate from trailing bits.
        // Translate back to bytes and update in data.

        auto picOutputFlagOffset = metadata_.GetValue("pic_output_flag_offset");
        data.insert(data.begin() + picOutputFlagOffset, value);

        auto startOfByteAlignmentBits = metadata_.GetValue("trailing_bits_offset");
        auto indexFollowingTrailing1 = startOfByteAlignmentBits + 2; // +1 because we inserted a bit, +1 to get next element.
        auto existingNumberOfTrailing0s = 8 - 1 - (startOfByteAlignmentBits % 8);

        if (existingNumberOfTrailing0s)
            data.erase(data.begin() + indexFollowingTrailing1);
        else
            data.insert(data.begin() + indexFollowingTrailing1, 7, false);
    }

    IDRSliceSegmentLayerMetadata::IDRSliceSegmentLayerMetadata(BitStream& metadata, HeadersMetadata headersMetadata)
        : SliceSegmentLayerMetadata(metadata, headersMetadata) {
        GetBitStream().CollectValue("first_slice_segment_in_pic_flag", 1, true);
        GetBitStream().SkipBits(1); // no_output_of_prior_pics_flag
        GetBitStream().MarkPosition("slice_pic_parameter_set_id_offset");
        GetBitStream().CollectGolomb("slice_pic_parameter_set_id");
        GetBitStream().MarkPosition("address_offset");
        GetBitStream().SkipExponentialGolomb(); // slice_type
        GetBitStream().MarkPosition("pic_output_flag_offset");
        GetBitStream().SkipBits(2);
        GetBitStream().SkipExponentialGolomb();
        GetBitStream().SkipBits(1);
        GetBitStream().MarkPosition("entry_point_offset");
        GetBitStream().SkipEntryPointOffsets(headersMetadata.GetPicture().HasEntryPointOffsets());
        GetBitStream().MarkPosition("trailing_bits_offset");
        GetBitStream().CollectValue("trailing_one", 1, true);
        GetBitStream().ByteAlign(0);
        GetBitStream().MarkPosition("end");
    }

    IDRSliceSegmentLayer::IDRSliceSegmentLayer(const StitchContext &context, const bytestring &data, const Headers &headers)
            : SliceSegmentLayer(context, data, headers),
            idrSliceSegmentLayerMetadata_(GetBitStream(), headersMetadata_){
    }

    TrailRSliceSegmentLayerMetadata::TrailRSliceSegmentLayerMetadata(BitStream &metadata, stitching::HeadersMetadata headersMetadata)
        : SliceSegmentLayerMetadata(metadata, headersMetadata) {
        GetBitStream().CollectValue("first_slice_segment_in_pic_flag", 1, true);
        GetBitStream().MarkPosition("slice_pic_parameter_set_id_offset");
        GetBitStream().CollectGolomb("slice_pic_parameter_set_id");
        GetBitStream().MarkPosition("address_offset");
        GetBitStream().SkipExponentialGolomb(); // slice_type
        GetBitStream().MarkPosition("pic_output_flag_offset");
        GetBitStream().MarkPosition("slice_pic_order_cnt_lsb");
        GetBitStream().SkipBits(headersMetadata.GetSequence().GetMaxPicOrder()); // slice_pic_order_cnt_lsb.
        GetBitStream().MarkPosition("after_slice_pic_order_cnt_lsb");
        GetBitStream().SkipTrue();
        GetBitStream().SkipBits(2);
        GetBitStream().SkipFalse();
        GetBitStream().SkipBits(1, headersMetadata.GetPicture().CabacInitPresentFlag());
        GetBitStream().SkipExponentialGolomb();
        GetBitStream().SkipExponentialGolomb();
        GetBitStream().SkipBits(1);
        GetBitStream().MarkPosition("entry_point_offset");
        GetBitStream().SkipEntryPointOffsets(headersMetadata.GetPicture().HasEntryPointOffsets());
        GetBitStream().MarkPosition("trailing_bits_offset");
        GetBitStream().CollectValue("trailing_one", 1, true);
        GetBitStream().ByteAlign(0);
        GetBitStream().MarkPosition("end");
    }

    TrailRSliceSegmentLayer::TrailRSliceSegmentLayer(const StitchContext &context, const bytestring &data, const Headers &headers)
            : SliceSegmentLayer(context, data, headers),
            trailRMetadata_(GetBitStream(), headersMetadata_){
    }
}; //namespace stitching
