

#include <string>
#include "Opaque.h"
#include "Nal.h"

using std::string;

namespace lightdb {

    Opaque::Opaque(const tiles::Context &context, const bytestring &data) : Nal(context, data) {}

    bytestring Opaque::GetBytes() const {
        bytestring data = GetData();
        bytestring nal_marker = GetNalMarker();
        data.insert(data.begin(), nal_marker.begin(), nal_marker.end());
        return data;
    }

}
