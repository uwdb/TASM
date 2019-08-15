#ifndef LIGHTDB_MP4READER_H
#define LIGHTDB_MP4READER_H

#include "Catalog.h"
#include "gpac/isomedia.h"
#include "gpac/internal/isomedia_dev.h"
#include "gpac/list.h"
#include <experimental/filesystem>

class MP4Reader {
public:
    static std::vector<int> keyframesForFile(const std::filesystem::path &filename) {
        if (filename.extension() != ".mp4")
            return {};

        static const int trackNumber = 1;

        GF_ISOFile *file = gf_isom_open(filename.c_str(), GF_ISOM_OPEN_READ, nullptr);
        GF_TrackBox *trak = gf_isom_get_track_from_file2(file, trackNumber);
        GF_SyncSampleBox *sampleBox = trak->Media->information->sampleTable->SyncSample;

        if (!sampleBox)
            return {};

        std::vector<int> keyframes(sampleBox->nb_entries);
        for (auto i = 0; i < sampleBox->nb_entries; ++i)
            keyframes[i] = sampleNumberToFrameNumber(sampleBox->sampleNumbers[i]);

        return keyframes;
    }

    explicit MP4Reader(const std::filesystem::path &filename)
        : filename_(filename),
        invalidFile_(false)
    {
        if (filename_.extension() != ".mp4") {
            invalidFile_ = true;
            return;
        }

        file_ = gf_isom_open(filename_.c_str(), GF_ISOM_OPEN_READ, nullptr);
        assert(gf_isom_set_nalu_extract_mode(file_, 1, GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG | GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG) == GF_OK);

        GF_TrackBox *trak = gf_isom_get_track_from_file2(file_, trackNumber_);
        GF_SyncSampleBox *sampleBox = trak->Media->information->sampleTable->SyncSample;
        // If !sampleBox, then every frame is a keyframe.
        if (!sampleBox)
            keyframeNumbers_ = {};
        else {
            keyframeNumbers_.resize(sampleBox->nb_entries);
            for (auto i = 0; i < sampleBox->nb_entries; ++i)
                keyframeNumbers_[i] = sampleBox->sampleNumbers[i] - 1;
        }

        numberOfSamples_ = gf_isom_get_sample_count(file_, trackNumber_);
    }

    ~MP4Reader() {
        return;
//        gf_isom_close(file_);
    }

    const std::vector<int> &keyframeNumbers() const {
        return keyframeNumbers_;
    }

    unsigned int numberOfSamples() const {
        return numberOfSamples_;
    }

    bool allFramesAreKeyframes() const {
        return filename_.extension() == ".mp4" && keyframeNumbers_.empty();
    }

    static int sampleNumberToFrameNumber(unsigned int sampleNumber) {
        return sampleNumber - 1;
    }

    static unsigned int frameNumberToSampleNumber(int frameNumber) {
        return frameNumber + 1;
    }

    bool allFrameSequencesBeginWithKeyframe(const std::vector<int> &frames) const {
        if (frames.empty())
            return true;

        if (allFramesAreKeyframes())
            return true;

        auto keyframeIterator = keyframeNumbers_.begin();
        auto frameIterator = frames.begin();
        auto startOfFrameSequence = *frameIterator;
        while (true) {
            // See if current frame sequence aligns with a keyframe.
            while (keyframeIterator != keyframeNumbers_.end() && *keyframeIterator < startOfFrameSequence)
                keyframeIterator++;

            // See if we weren't able to find a keyframe >= the frame we are interested in.
            if (keyframeIterator == keyframeNumbers_.end())
                return false;

            // The keyframe we found is past the start of the frame sequence.
            if (*keyframeIterator != startOfFrameSequence)
                return false;

            while (frameIterator + 1 != frames.end() && *(frameIterator + 1) == *frameIterator + 1)
                frameIterator++;

            // We have no more frames to look at. If we've made it this far, then we're good.
            if (frameIterator + 1 == frames.end())
                break;

            // frameIterator is pointing to last frame in sequence. Move it forward, and set it to the start of a new frame sequence.
            startOfFrameSequence = *(++frameIterator);
        }
        return true;
    }

    lightdb::bytestring dataForSamples(unsigned int firstSampleToRead, unsigned int lastSampleToRead) const {
        unsigned long size = 0;

        // First read to get sizes.
        for (auto i = firstSampleToRead; i <= lastSampleToRead; i++) {
            GF_ISOSample *sample = gf_isom_get_sample_info(file_, trackNumber_, i, NULL, NULL);
            size += sample->dataLength;
            gf_isom_sample_del(&sample);
        }

        // Then read to get the actual data.
        lightdb::bytestring videoData;
        videoData.reserve(size);
        for (auto i = firstSampleToRead; i <= lastSampleToRead; i++) {
            GF_ISOSample *sample = gf_isom_get_sample(file_, trackNumber_, i, NULL);
            videoData.insert(videoData.end(), sample->data, sample->data + sample->dataLength);
            gf_isom_sample_del(&sample);
        }

        return videoData;
    }

private:
    static GF_TrackBox *gf_isom_get_track_from_file2(GF_ISOFile *the_file, u32 trackNumber) {
        auto count = gf_list_count(the_file->moov->trackList);
        assert(trackNumber <= count);
        unsigned int position = 0;
        void *box = NULL;
        while ((box = gf_list_enum(the_file->moov->trackList, &position))) {
            if (reinterpret_cast<GF_TrackBox*>(box)->Header->trackID == trackNumber)
                break;
        }
        assert(box);

        return reinterpret_cast<GF_TrackBox*>(box);
    }

    static const unsigned int trackNumber_ = 1;
    std::filesystem::path filename_;
    GF_ISOFile *file_;
    std::vector<int> keyframeNumbers_;
    unsigned int numberOfSamples_;
    unsigned int numberOfSamplesRead_ = 0;
    bool invalidFile_;
};

#endif //LIGHTDB_MP4READER_H
