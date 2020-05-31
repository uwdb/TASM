//
// Created by sophi on 4/10/2018.
//

#ifndef LIGHTDB_HEADERS_H
#define LIGHTDB_HEADERS_H

#include<vector>
#include<string>

#include "Context.h"

using bytestring = std::vector<char>;

namespace lightdb {

    class VideoParameterSet;
    class PictureParameterSet;
    class Nal;
    class SequenceParameterSet;

    class Headers {
    public:
        // TODO: fix how inefficient get bytes is. should reserve a string
        // of the right size ahead of time, should add a get size method

        /**
         * Extracts the three headers from nals
         * @param context The context of the nals
         * @param nals The byte streams of the nals
         */
        Headers(const Context &context, std::vector<bytestring> nals);

        /**
         *
         * @return The bytes of the headers, in the order that the headers were provided
         * in the "nals" argument to the constructor
         */
        bytestring GetBytes() const;

        /**
         *
         * @return The VideoParameterSet header
         */
        VideoParameterSet* GetVideo() const;

        /**
         *
         * @return The SequenceParameterSet header
         */
        SequenceParameterSet* GetSequence() const;

        /**
         *
         * @return The PictureParameterSet header
         */
        PictureParameterSet* GetPicture() const;

        static const int kNumHeaders = 3;

    private:
        VideoParameterSet *video_;
        SequenceParameterSet *sequence_;
        PictureParameterSet *picture_;
        std::vector<Nal*> headers_;
    };
}

#endif //LIGHTDB_HEADERS_H
