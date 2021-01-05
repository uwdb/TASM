//
// Created by sophi on 4/9/2018.
//

#include <cassert>
#include <iostream>
#include <string>
#include <cassert>
#include "Nal.h"
#include "NalType.h"
#include "Context.h"
#include "SequenceParameterSet.h"
#include "PictureParameterSet.h"
#include "VideoParameterSet.h"
#include "SliceSegmentLayer.h"
#include "Opaque.h"
#include "AccessDelimiter.h"


namespace lightdb {

    class Headers;

    Nal::Nal(const tiles::Context &context, const bytestring &data) : context_(context), byte_data_(data),
                                                               type_(PeekType(data)), is_header_(IsHeader()) {
        assert (ForbiddenZero() == 0);
    }

    bytestring Nal::GetData() const {
        return byte_data_;
    }

    tiles::Context Nal::GetContext() const {
        return context_;
    }

    int Nal::GetType() const {
        return type_;
    }

    bool Nal::IsSequence() const {
        return type_ == NalUnitSPS;
    }

    bool Nal::IsVideo() const {
        return type_ == NalUnitVPS;
    }

    bool Nal::IsPicture() const {
        return type_ == NalUnitPPS;
    }
    bool Nal::IsHeader() const {
        return type_ == NalUnitSPS || type_ == NalUnitPPS || type_ == NalUnitVPS;
    }

    bool Nal::IsSegment() const {
        return type_ == NalUnitCodedSliceIDRWRADL || type_ == NalUnitCodedSliceTrailR;
    }

    int Nal::ForbiddenZero() const {
      return (byte_data_[kForbiddenZeroIndex] & kForbiddenZeroMask);
    }

    bytestring GetNalMarker() {
        bytestring nal_marker(3);
        nal_marker[0] = static_cast<char>(0x00);
        nal_marker[1] = static_cast<char>(0x00);
        nal_marker[2] = static_cast<char>(0x01);
        return nal_marker;
    }

    bool IsSegment(const bytestring &data) {
        int type = PeekType(data);
        return type == NalUnitCodedSliceIDRWRADL ||
        type == NalUnitCodedSliceTrailR;
    }

    bool IsKeyframe(const bytestring &data) {
        return PeekType(data) == NalUnitCodedSliceIDRWRADL;
    }

    size_t GetHeaderSize() {
        return kNalHeaderSize - kNalMarkerSize;
    }

    size_t  GetHeaderSizeInBits() {
        return GetHeaderSize() * CHAR_BIT;
    }

    int PeekType(const bytestring &data) {
        assert(data.size() > 0);
        return (static_cast<int>(static_cast<unsigned char>(data[0])) & 0x7F) >> 1;
    }


    Nal *Load(const tiles::Context &context, const bytestring &data) {
        int type = PeekType(data);
        if (type == NalUnitSPS) {
            return new SequenceParameterSet(context, data);
        } else if (type == NalUnitPPS) {
            return new PictureParameterSet(context, data);
        } else if (type == NalUnitVPS) {
            return new VideoParameterSet(context, data);
        } else if (type == NalUnitAccessUnitDelimiter) {
            return new AccessDelimiter(context, data);
        } else {
            return new Opaque(context, data);
        }
    }

    Nal *Load(const tiles::Context &context, const bytestring &data, const Headers &headers) {
        int type = PeekType(data);
        if (type == NalUnitCodedSliceIDRWRADL) {
            return new IDRSliceSegmentLayer(context, data, headers);
        } else if (type == NalUnitCodedSliceTrailR) {
            return new TrailRSliceSegmentLayer(context, data, headers);
        } else {
            return new Opaque(context, data);
        }
    }
}
