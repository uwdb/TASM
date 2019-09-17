#ifndef LIGHTDB_STITCHER_H
#define LIGHTDB_STITCHER_H

#include "Headers.h"
#include "StitchContext.h"
#include <unordered_set>
#include <vector>

#include "SliceSegmentLayer.h"

namespace lightdb::hevc {

    class PicOutputFlagAdder {
    public:
        PicOutputFlagAdder(const std::unordered_set<int> &framesToKeep)
        : framesToKeep_(framesToKeep),
        frameNumber_(0)
        {}

        void addPicOutputFlagToGOP(bytestring &gopData, int firstFrameIndex, const std::unordered_set<int> &framesToKeep);

    private:
        const std::unordered_set<int> framesToKeep_;
        unsigned int frameNumber_;
    };

    class Stitcher {
        friend class IdenticalFrameRetriever;
     public:

        /**
         * Creates a Stitcher object, which involves splitting all of the tiles into their component nals. It also moves all of the data from
         * the tiles passed in, rendering "data" useless after the constructor
         * @param context The context of the video data
         * @param data A vector with each element being the bytestring of a tile. All data is moved from this vector, rendering it useless post
         * processing
         */
        Stitcher(StitchContext context, std::vector<bytestring> &data)
                : context_(std::move(context)), headers_(context_, GetNals(data).front())
        { }

        /**
         *
         * @return A bytestring with all tiles passed into the constructor stitched together
         */
        std::unique_ptr<bytestring> GetStitchedSegments();

        void addPicOutputFlagIfNecessaryKeepingFrames(const std::unordered_set<int> &framesToKeep);
        bytestring combinedNalsForTile(unsigned int tileNumber) const;

        SliceSegmentLayer loadPFrameSegment(bytestring &data);

     private:

        /**
         *
         * @return The tile_nals_ field populated with the nals of each tile. Each element of the outer vector is a tile, and each element of the inner
         * vector is a nal for tha tile
         */
        const std::vector<std::list<bytestring>> &GetNals(std::vector<bytestring> &data);

        /**
         * Returns the nals that are segments for a given tile. Note that this moves the corresponding nal from the tile_nals_ field, rendering the
         * index for each affected nal useless in the field
         * @param tile_num The index of the tile in the tile_nals_ vector
         * @param num_bytes A running count of the number of bytes the segment nals of all the tiles occupy. This is incremented by the number of bytes
         * the segments of this tile_num occupy
         * @param num_keyframes A running count of the number of keyframes the tiles in this stitch have. This is incremented if "first" is true (to avoid
         * incrementing the count for each tile, since we only need to count the number in one tile to know the number for the final stitch)
         * @param first Whether or not this is the first tile being processed
         * @return The nals that are segments for this tile
         */
        std::list<bytestring> GetSegmentNals(unsigned long tile_num, unsigned long *num_bytes, unsigned long *num_keyframes, bool first);

//        const std::vector<bytestring> tiles_;
        std::vector<std::list<bytestring>> tile_nals_;
        const StitchContext context_;
        const Headers headers_;
        std::vector<std::vector<std::unique_ptr<Nal>>> formattedNals_;
    };

    class IdenticalFrameRetriever {
    public:
        IdenticalFrameRetriever(std::unique_ptr<bytestring> iFrameBytesWithHeaders, std::unique_ptr<bytestring> pFrameData) {
            StitchContext context{{1, 1}, {1, 1}};
            std::vector<bytestring> vectorOfCombinedData(1);
            auto &combinedData = vectorOfCombinedData[0];
            combinedData.resize(iFrameBytesWithHeaders->size() + pFrameData->size());
            std::copy(iFrameBytesWithHeaders->begin(), iFrameBytesWithHeaders->end(), combinedData.begin());
            std::copy(pFrameData->begin(), pFrameData->end(), combinedData.begin() + iFrameBytesWithHeaders->size());
            iFrameDataWithHeaders_ = std::move(iFrameBytesWithHeaders);

            Stitcher stitcher(context, vectorOfCombinedData);
            getPFrameData(stitcher);
        }

        const bytestring &iFrameData() const {
            return *iFrameDataWithHeaders_;
        }

        const bytestring &pFrameData() const {
            return pFrameData_;
        }

        const bytestring &pFrameHeaderForPicOrder(unsigned int picOrder) {
            UpdatePicOrderCntLsb(pFrameHeader_, picOutputCntLsbBitOffset_, picOrder, numberOfBitsForPicOutputCntLsb_, true);
            return pFrameHeader_;
        }

    private:
        void getPFrameData(Stitcher &stitcher);

        std::unique_ptr<bytestring> iFrameDataWithHeaders_;
        bytestring pFrameHeader_;
        bytestring pFrameData_;

        unsigned int picOutputCntLsbBitOffset_;
        unsigned int numberOfBitsForPicOutputCntLsb_;
    };

}; //namespace lightdb::hevc

#endif //LIGHTDB_STITCHER_H
