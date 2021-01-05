//
// Created by sophi on 4/9/2018.
//

#ifndef LIGHTDB_TILES_TILES_STITCHER_H
#define LIGHTDB_TILES_TILES_STITCHER_H

#include <string>
#include <vector>
#include "Headers.h"
#include "Context.h"
#include <memory>

namespace lightdb::tiles {
    template <template<typename> class SequenceContainerType>
    class Stitcher {
     public:

        // Note: all that we actually use is gop_size, dimensions, _stitched, tiles, segment_id
        // (for tiles), context (also for tiles + assertions)
        Stitcher(const Context &context, SequenceContainerType<std::unique_ptr<bytestring>> &data);

        // Should the return type be an array or a vector?
        std::unique_ptr<bytestring> GetStitchedSegments();

        static bytestring GetActiveParameterSetsSEI();

    private:

        std::list<std::shared_ptr<bytestring>> GetNals(const bytestring &tile);
        std::list<std::shared_ptr<bytestring>> GetSegmentNals(const bytestring &tile);

        SequenceContainerType<std::unique_ptr<bytestring>> tiles_;
        Context context_;
        Headers headers_;

        //static string kNalDelimiter = static_cast<char>(0x00) + GetNalMarker();
    };

} // namespace lightdb::stitcher

#endif //LIGHTDB_TILES_TILES_STITCHER_H
