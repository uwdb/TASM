//
// Created by sophi on 4/11/2018.
//

#ifndef LIGHTDB_SLICESEGMENTLAYER_H
#define LIGHTDB_SLICESEGMENTLAYER_H

#include <vector>
#include <string>
#include "Nal.h"
#include "Headers.h"
#include "BitStream.h"
#include "BitArray.h"

//TODO: Fix that fact that I'm calling new and immediately discarding, might need to add destructor/assignment

namespace lightdb {
    // Defined in 7.3.6.1 and 7.3.2.9
    class SliceSegmentLayer : public Nal {
    public:

          /**
           * Interprets data as a byte stream representing a SliceSegmentLayer
           * @param context The context surrounding the Nal
           * @param data The byte stream
           * @param headers The headers associated with this segment
           */
          SliceSegmentLayer(const Context &context, const bytestring &data, const Headers &headers);

          /**
           * Sets the address of this segment to be address. Should only be called once -
           * if the address is set, should not be reset
           * @param address The new address of this segment
           */
          void SetAddressAndPPSId(size_t address, unsigned int pps_id);

//          void SetPPSId(unsigned int pps_id);

          /**
           *
           * @return This segment's address
           */
          size_t GetAddress() const;

          /**
           *
           * @return A string with the bytes of this Nal
           */
          bytestring GetBytes() const override;

    protected:
        utility::BitStream& GetBitStream();

        unsigned long numberOfAddedBits_;

    private:
        utility::BitArray data_;
        Headers headers_;
        utility::BitStream metadata_;
        size_t address_;

        static const int kFirstSliceFlagOffset = 0;
        static const int kMaxHeaderLength = 24;
    };

    class IDRSliceSegmentLayer : public SliceSegmentLayer {
    public:

      /**
       * Interprets data as a byte stream representing an IDRSliceSegmentLayer
       * @param context The context surrounding the Nal
       * @param data The byte stream
       * @param headers The headers associated with this segment
       */
      IDRSliceSegmentLayer(const Context &context, const bytestring &data, const Headers &headers);
    };

    class TrailRSliceSegmentLayer : public SliceSegmentLayer {
    public:

      /**
       * Interprets data as a byte stream representing a TrailRSliceSegmentLayer
       * @param context The context surrounding the Nal
       * @param data The byte stream
       * @param headers The headers associated with this segment
       */
      TrailRSliceSegmentLayer(const Context &context, const bytestring &data, const Headers &headers);

    };
}

#endif //LIGHTDB_SLICESEGMENTLAYER_H
