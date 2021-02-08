#include "Nal.h"
#include "SequenceParameterSet.h"
#include "PictureParameterSet.h"
#include "VideoParameterSet.h"
#include "SliceSegmentLayer.h"
#include "AccessDelimiter.h"
#include "Opaque.h"

namespace stitching {
    std::shared_ptr<Nal> Load(const StitchContext &context, const bytestring &data) {
        switch(PeekType(data)) {
            case NalUnitSPS:
                return std::make_shared<SequenceParameterSet>(context, data);
            case NalUnitPPS:
                return std::make_shared<PictureParameterSet>(context, data);
            case NalUnitVPS:
                return std::make_shared<VideoParameterSet>(context, data);
            case NalUnitAccessUnitDelimiter:
                return std::make_shared<AccessDelimiter>(context, data);
            default:
                return std::make_shared<Opaque>(context, data);
        }
    }

    SliceSegmentLayer Load(const StitchContext &context, const bytestring &data, const Headers &headers) {
        switch(PeekType(data)) {
            case NalUnitCodedSliceIDRWRADL:
                return IDRSliceSegmentLayer(context, data, headers);
            case NalUnitCodedSliceTrailR:
                return TrailRSliceSegmentLayer(context, data, headers);
            default:
                std::cerr << std:: string("Unrecognized SliceSegmentLayer type ") << std::to_string(PeekType(data)) << "data" << std::endl;
                assert(false);
        }
    }

    std::unique_ptr<Nal> LoadNal(const StitchContext &context, const bytestring &data, const Headers &headers) {
        switch(PeekType(data)) {
            case NalUnitSPS:
                return std::make_unique<SequenceParameterSet>(context, data);
            case NalUnitPPS:
                return std::make_unique<PictureParameterSet>(context, data);
            case NalUnitVPS:
                return std::make_unique<VideoParameterSet>(context, data);
            case NalUnitCodedSliceIDRWRADL:
                return std::make_unique<IDRSliceSegmentLayer>(context, data, headers);
            case NalUnitCodedSliceTrailR:
                return std::make_unique<TrailRSliceSegmentLayer>(context, data, headers);
            case NalUnitAccessUnitDelimiter:
                return std::make_unique<AccessDelimiter>(context, data);
            default:
                return std::make_unique<Opaque>(context, data);
        }
    }
}; //namespace stitching
