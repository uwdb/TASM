//
// Created by sophi on 4/14/2018.
//

#include <string>
#include <vector>
#include <cassert>
#include <math.h>

#include "Emulation.h"
#include "BitStream.h"
#include "PictureParameterSet.h"
#include "SequenceParameterSet.h"
#include "SliceSegmentLayer.h"
//#include "NalType.h"
#include "BitArray.h"
#include "Headers.h"
#include "Golombs.h"

using std::string;
using std::vector;
using lightdb::utility::BitStream;
using lightdb::utility::BitArray;

namespace lightdb {

    SliceSegmentLayer::SliceSegmentLayer(const tiles::Context &context, const bytestring &data, const Headers &headers) : Nal(context, data),
                                        numberOfAddedBits_(0),
                                         data_(RemoveEmulationPrevention(GetData(), GetHeaderSize(), kMaxHeaderLength)),
                                         headers_(headers),
                                         metadata_(data_.begin(), data_.begin() + GetHeaderSizeInBits()){
    }

    bytestring SliceSegmentLayer::GetBytes() const {
        return AddEmulationPreventionAndMarker(data_, GetHeaderSize(), kMaxHeaderLength + ceil(numberOfAddedBits_ / 8));
    }

    size_t SliceSegmentLayer::GetAddress() const {
        return address_;
    }

    BitStream& SliceSegmentLayer::GetBitStream() {
        return metadata_;
    }

    void SliceSegmentLayer::SetAddressAndPPSId(size_t address, unsigned int pps_id) {
        numberOfAddedBits_ = 0u;

        address_ = address;
        // Make sure it's byte aligned
        assert (metadata_.GetValue("end") % 8 == 0);

        // The header is the end of the metadata, so move the portion before that into
        // the header array
        auto header_end = metadata_.GetValue("end");
        BitArray header_bits(0);
        header_bits.insert(header_bits.begin(), data_.begin(), data_.begin() + header_end);

        // Convert the header size from bytes to bits, add the offset
        header_bits[GetHeaderSize() * 8 + kFirstSliceFlagOffset] = address == 0;

        auto address_length = headers_.GetSequence()->GetAddressLength();
        // If the address is 0, we have nothing to insert (it's the first slice)
        if (address) {
            // Note that BitArray has to convert the address to binary and pad it
            header_bits.Insert(metadata_.GetValue("address_offset"), address, address_length);
            numberOfAddedBits_ += address_length;
        }

        if (!headers_.GetPicture()->HasEntryPointOffsets()) {
            auto location = metadata_.GetValue("entry_point_offset");
            if (address) {
                location += address_length;
            }
            // Insert a 0 as an exponential golomb, meaning the one bit '1'
            header_bits.Insert(location, 1, 1);
            numberOfAddedBits_ += 1;
        }

        // Now do PPS setting.
        auto current_pps_id = metadata_.GetValue("slice_pic_parameter_set_id");
        if (current_pps_id != pps_id) {
            auto size_of_current_encoded_pps_id = EncodeGolombs({current_pps_id}).size();
            auto current_pps_id_offset = metadata_.GetValue("slice_pic_parameter_set_id_offset");
            auto new_encoded_pps_id = EncodeGolombs({pps_id});

            header_bits.Replace(current_pps_id_offset, current_pps_id_offset + size_of_current_encoded_pps_id, new_encoded_pps_id);
            numberOfAddedBits_ += new_encoded_pps_id.size() - size_of_current_encoded_pps_id;
        }

        header_bits.ByteAlign();
        assert(!(header_bits.size() % 8));
        assert(header_bits.size() / 8 <= 24);

        // The new size of the data is the size of the new header plus the size of the data
        // minus the old header size
        //BitArray new_data(header_bits.size() + data_.size() - header_end);
        BitArray new_data(0);
        new_data.insert(new_data.end(), header_bits.begin(), header_bits.end());

        new_data.insert(new_data.end(), data_.begin() + header_end, data_.end());
        data_ = new_data;
    }

//    void SliceSegmentLayer::SetPPSId(unsigned int pps_id) {
//        auto current_pps_id = metadata_.GetValue("slice_pic_parameter_set_id");
//        if (current_pps_id == pps_id)
//            return;
//
//        auto current_encoded_pps_id = EncodeGolombs({ current_pps_id });
//        auto current_pps_id_offset = metadata_.GetValue("slice_pic_parameter_set_id_offset");
//        auto new_encoded_pps_id = EncodeGolombs({ pps_id });
//
//
//
//    }


    IDRSliceSegmentLayer::IDRSliceSegmentLayer(const tiles::Context &context, const bytestring &data, const Headers &headers) :
                                               SliceSegmentLayer(context, data, headers) {

        //assert(NalUnitCodedSliceBLAWLP <= GetType() <= NalUnitReservedIRAPVCL23);

        GetBitStream().CollectValue("first_slice_segment_in_pic_flag", 1, true);
        GetBitStream().SkipBits(1);
        GetBitStream().MarkPosition("slice_pic_parameter_set_id_offset");
        GetBitStream().CollectGolomb("slice_pic_parameter_set_id");
        GetBitStream().MarkPosition("address_offset");
        GetBitStream().SkipExponentialGolomb();
        GetBitStream().SkipBits(2);
        GetBitStream().SkipExponentialGolomb();
        GetBitStream().SkipBits(1); // I think this is slice_loop_filter_across_slices_enabled_flag.
        GetBitStream().MarkPosition("entry_point_offset");
        GetBitStream().SkipEntryPointOffsets(headers.GetPicture()->HasEntryPointOffsets());
        GetBitStream().CollectValue("trailing_one", 1, true);
        GetBitStream().ByteAlign(0);
        GetBitStream().MarkPosition("end");
    }

    TrailRSliceSegmentLayer:: TrailRSliceSegmentLayer(const tiles::Context &context, const bytestring &data, const Headers &headers) :
                                                      SliceSegmentLayer(context, data, headers) {

                GetBitStream().CollectValue("first_slice_segment_in_pic_flag", 1, true);
                GetBitStream().MarkPosition("slice_pic_parameter_set_id_offset");
                GetBitStream().CollectGolomb("slice_pic_parameter_set_id");
                GetBitStream().MarkPosition("address_offset");
                GetBitStream().SkipExponentialGolomb();
                GetBitStream().SkipBits(headers.GetSequence()->GetMaxPicOrder());
                GetBitStream().SkipTrue();
                GetBitStream().SkipBits(2);
                GetBitStream().SkipFalse();
                GetBitStream().SkipBits(1, headers.GetPicture()->CabacInitPresentFlag());
                GetBitStream().SkipExponentialGolomb();
                GetBitStream().SkipExponentialGolomb();
                GetBitStream().SkipBits(1); // I think this is slice_loop_filter_across_slices_enabled_flag
                GetBitStream().MarkPosition("entry_point_offset");
                GetBitStream().SkipEntryPointOffsets(headers.GetPicture()->HasEntryPointOffsets());
                GetBitStream().CollectValue("trailing_one", 1, true);
                GetBitStream().ByteAlign(0);
                GetBitStream().MarkPosition("end");
    }
}
