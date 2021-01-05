//
// Created by sophi on 4/9/2018.
//

#ifndef LIGHTDB_TILES_TILES_STITCHER_H
#define LIGHTDB_TILES_TILES_STITCHER_H

#include <string>
#include <vector>
#include "Headers.h"
#include "Context.h"

namespace lightdb::tiles {
    class Stitcher {
     public:

        // Note: all that we actually use is gop_size, dimensions, _stitched, tiles, segment_id
        // (for tiles), context (also for tiles + assertions)
        Stitcher(const Context &context, std::vector<bytestring> &data);

        // Should the return type be an array or a vector?
        bytestring GetStitchedSegments();

        static bytestring GetActiveParameterSetsSEI();

    private:

        std::vector<bytestring> GetNals(const bytestring &tile);
        std::vector<bytestring> GetSegmentNals(const bytestring &tile);

        std::vector<bytestring> tiles_;
        Context context_;
        Headers headers_;

        //static string kNalDelimiter = static_cast<char>(0x00) + GetNalMarker();
    };

} // namespace lightdb::stitcher

#endif //LIGHTDB_TILES_TILES_STITCHER_H
