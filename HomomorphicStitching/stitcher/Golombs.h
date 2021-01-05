//
// Created by sophi on 5/12/2018.
//

#ifndef LIGHTDB_TILES_GOLOMBS_H
#define LIGHTDB_TILES_GOLOMBS_H

#include "BitArray.h"
#include "BitStream.h"

namespace lightdb {

        /**
         * Encodes the values in golombs as golombs, in the order provided
         * @param ints The integers to be encoded as ints
         * @returns A BitArray holding the encoded contents
         */
        utility::BitArray EncodeGolombs(const std::vector<unsigned int> &golombs);

        /**
         * Decodes the next golomb in the stream, returning the value as a long
         * @param stream The stream that contains the bits of the golomb, with its iterator
         * set to the first bit
         * @return The golomb
         */
        unsigned long DecodeGolomb(utility::BitStream &stream);

}

#endif //LIGHTDB_TILES_GOLOMBS_H
