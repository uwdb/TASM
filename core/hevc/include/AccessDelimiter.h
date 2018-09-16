#ifndef LIGHTDB_ACCESSDELIMITER_H
#define LIGHTDB_ACCESSDELIMITER_H

#include "Nal.h"

namespace lightdb {

    class AccessDelimiter : public Nal {
    public:
        AccessDelimiter(const Context &context, bytestring const &data) : Nal(context, data) {}

        inline bytestring GetBytes() const override {
            bytestring data = Nal::GetBytes();
            data.insert(data.begin(), kNalMarker.begin(), kNalMarker.end());
            return data;
        }
    };

}

#endif //LIGHTDB_ACCESSDELIMITER_H
