//
// Created by sophi on 4/17/2018.
//

#include <iostream>
#include <string>
#include <cassert>
#include "Nal.h"
#include "VideoParameterSet.h"
#include "Profile.h"
#include "Emulation.h"

using lightdb::utility::BitArray;

namespace lightdb {

    VideoParameterSet::VideoParameterSet(const tiles::Context &context, const bytestring &data) : Nal(context, data),
                                                                                           data_(data),
                                                                                           //data_(RemoveEmulationPrevention(data, GetHeaderSize(), data.size())),
                                                                                           profile_size_(GetSizeInBits(VPSMaxSubLayersMinus1())) {
        assert (profile_size_ % 8 == 0);
    }

    bytestring VideoParameterSet::GetBytes() const {

        //std::cerr << "video" << std::endl;
        /**
        for (auto b : AddEmulationPreventionAndMarker(data_, GetHeaderSize(), data_.size() / 8)) {
            //std::cerr << static_cast<int>(static_cast<unsigned char>(b)) << std::endl;
        }
        return AddEmulationPreventionAndMarker(data_, GetHeaderSize(), data_.size() / 8);  **/
        bytestring bytes;
        bytestring nal_marker = GetNalMarker();
        bytes.insert(bytes.begin(), nal_marker.begin(), nal_marker.end());
        bytes.insert(bytes.end(), data_.begin(), data_.end());
        for (auto b : bytes) {
            //std::cerr << static_cast<int>(static_cast<unsigned char>(b)) << std::endl;
        }
        return bytes;
    }

    int VideoParameterSet::VPSMaxSubLayersMinus1() const {
        return data_[GetHeaderSize() + kVPSMaxSubLayersMinus1Offset] & kVPSMaxSubLayersMinus1Mask;
    }

    void VideoParameterSet::SetGeneralLevelIDC(int value) {
        // Profile size is in bits, so must be converted to bytes
        //data_.SetByte(kSizeBeforeProfile + profile_size_ / 8 - kGeneralLevelIDCSize, static_cast<unsigned char>(value));
        data_[kSizeBeforeProfile + profile_size_ / 8 - kGeneralLevelIDCSize] = static_cast<unsigned char>(value);
    }

    int VideoParameterSet::GetGeneralLevelIDC() const {
        return data_[kSizeBeforeProfile + profile_size_ / 8 - kGeneralLevelIDCSize];
    }
}

