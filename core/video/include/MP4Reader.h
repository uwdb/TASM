#ifndef LIGHTDB_MP4READER_H
#define LIGHTDB_MP4READER_H

#include "Color.h"
#include "gpac/isomedia.h"
#include "gpac/internal/isomedia_dev.h"
#include "gpac/list.h"
#include <filesystem>

class MP4Reader {
public:
    explicit MP4Reader(const std::filesystem::path &filename)
        : filename_(filename),
        invalidFile_(false)
    {
        if (filename_.extension() != ".mp4") {
            invalidFile_ = true;
            file_ = NULL;
            return;
        }

        setUpGFIsomFile();

        GF_TrackBox *trak = gf_isom_get_track_from_file2(file_, trackNumber_);
        GF_SyncSampleBox *sampleBox = trak->Media->information->sampleTable->SyncSample;
        // If !sampleBox, then every frame is a keyframe.
        if (!sampleBox)
            keyframeNumbers_ = {};
        else {
            keyframeNumbers_.resize(sampleBox->nb_entries);
            for (unsigned int i = 0; i < sampleBox->nb_entries; ++i)
                keyframeNumbers_[i] = sampleBox->sampleNumbers[i] - 1;
        }

        numberOfSamples_ = gf_isom_get_sample_count(file_, trackNumber_);
    }

    /*
     *
     *    static const unsigned int trackNumber_ = 1;
    std::filesystem::path filename_;
    GF_ISOFile *file_;
    std::vector<int> keyframeNumbers_;
    unsigned int numberOfSamples_;
    unsigned int numberOfSamplesRead_ = 0;
    bool invalidFile_;
    bool forDecoding_;
     */
    MP4Reader(const MP4Reader &other)
        : filename_(other.filename_),
        keyframeNumbers_(other.keyframeNumbers_),
        numberOfSamples_(other.numberOfSamples_),
        numberOfSamplesRead_(other.numberOfSamplesRead_),
        invalidFile_(other.invalidFile_)
    {
        other.closeFile();
        if (invalidFile_)
            file_ = NULL;
        else
            setUpGFIsomFile();
    }

    ~MP4Reader() {
        // Was getting segfaults here. Seems like there may be threading issues but have to look more into it.
        if (file_) {
            gf_isom_close(file_);
            file_ = NULL;
        }
    }

    void closeFile() const {
        if (file_) {
            gf_isom_close(file_);
            file_ = NULL;
        }
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

    std::pair<std::vector<int>, std::vector<int>> frameSequencesInSequentialGOPsAndNonSequentialGOPs(const std::vector<int> &frames) const;

    bool allFrameSequencesBeginWithKeyframe(const std::vector<int> &frames) const;

    std::unique_ptr<lightdb::bytestring> dataForSamples(unsigned int firstSampleToRead, unsigned int lastSampleToRead) const;

private:
    void setUpGFIsomFile() {
        file_ = gf_isom_open(filename_.c_str(), GF_ISOM_OPEN_READ, nullptr);
        u32 flags = GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG | GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG;
        // I think the ANNEXB flag adds AUD NALS.
        auto result = gf_isom_set_nalu_extract_mode(file_, 1, flags);
        assert(result == GF_OK);
    }

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
    mutable GF_ISOFile *file_;
    std::vector<int> keyframeNumbers_;
    unsigned int numberOfSamples_;
    unsigned int numberOfSamplesRead_ = 0;
    bool invalidFile_;
};

#endif //LIGHTDB_MP4READER_H
