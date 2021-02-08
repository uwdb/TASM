#ifndef HOMOMORPHIC_STITCHING_ACCESSDELIMITER_H
#define HOMOMORPHIC_STITCHING_ACCESSDELIMITER_H

#include "Nal.h"

namespace stitching {

    class AccessDelimiter : public Nal {
    public:
        AccessDelimiter(const StitchContext &context, bytestring const &data) : Nal(context, data) {}

        inline bytestring GetBytes() const override {
            bytestring data = Nal::GetBytes();
            data.insert(data.begin(), kNalMarker.begin(), kNalMarker.end());
            return data;
        }
    };

} //namespace stitching

#endif //HOMOMORPHIC_STITCHING_ACCESSDELIMITER_H
