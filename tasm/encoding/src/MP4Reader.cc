#include "MP4Reader.h"

std::unique_ptr<std::vector<char>> MP4Reader::dataForSamples(unsigned int firstSampleToRead, unsigned int lastSampleToRead) const {
    unsigned long size = 0;

    // First read to get sizes.
    for (auto i = firstSampleToRead; i <= lastSampleToRead; i++) {
        GF_ISOSample *sample = gf_isom_get_sample_info(file_, trackNumber_, i, NULL, NULL);
        size += sample->dataLength;
        gf_isom_sample_del(&sample);
    }

    // Then read to get the actual data.
    std::unique_ptr<std::vector<char>> videoData(new std::vector<char>);
    videoData->reserve(size);
    for (auto i = firstSampleToRead; i <= lastSampleToRead; i++) {
        GF_ISOSample *sample = gf_isom_get_sample(file_, trackNumber_, i, NULL);
        videoData->insert(videoData->end(), sample->data, sample->data + sample->dataLength);
        gf_isom_sample_del(&sample);
    }

    return videoData;
}