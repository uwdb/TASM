#include "MP4Reader.h"

std::pair<std::vector<int>, std::vector<int>> MP4Reader::frameSequencesInSequentialGOPsAndNonSequentialGOPs(const std::vector<int> &frames) const {
    std::vector<int> framesInSequentialGOPs;
    std::vector<int> framesNotInSequentialGOPs;

    auto frameIt = frames.begin();
    for (auto keyframeIt = keyframeNumbers_.begin(); keyframeIt != keyframeNumbers_.end() && frameIt != frames.end(); keyframeIt++) {
        // Find frames that start at keyframeIt.
        while (frameIt != frames.end() && *frameIt < *keyframeIt)
            framesNotInSequentialGOPs.push_back(*frameIt++);

        if (frameIt == frames.end())
            break;

        // See if the current frame is a keyframe.
        if (*frameIt == *keyframeIt) {
            // See if all of the frames in this GOP are sequential.
            auto sequentialIt = frameIt;
            auto lastInSequence = *sequentialIt++;
            while (sequentialIt != frames.end() && *sequentialIt == lastInSequence + 1)
                lastInSequence = *sequentialIt++;

            // sequentialIt points one past the sequence. If it's in the same GOP, then the frames in this GOP are non-sequential,
            // so add all of the frames to the non-sequential list. This will happen the next iteration around the loop.
            // Move forward the keyframe iterator so we can see where the next keyframe is.
            auto nextKeyframe = -1;
            if (keyframeIt + 1 != keyframeNumbers_.end())
                nextKeyframe = *(keyframeIt + 1);

            if (sequentialIt == frames.end() || *sequentialIt >= nextKeyframe) {
                while (frameIt != frames.end() && frameIt != sequentialIt)
                    framesInSequentialGOPs.push_back(*frameIt++);
            }
        } else {
            // This GOP can't be sequential because the frames in it don't start at a keyframe.
            // We're at frames in the next GOP.
        }
    }

    // If we quit but there are still frames in the last GOP, add them to the non-sequential list.
    while (frameIt != frames.end())
        framesNotInSequentialGOPs.push_back(*frameIt++);

    return std::make_pair<std::vector<int>, std::vector<int>>(std::move(framesInSequentialGOPs), std::move(framesNotInSequentialGOPs));
}

bool MP4Reader::allFrameSequencesBeginWithKeyframe(const std::vector<int> &frames) const {
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

std::unique_ptr<lightdb::bytestring> MP4Reader::dataForSamples(unsigned int firstSampleToRead, unsigned int lastSampleToRead) const {
    unsigned long size = 0;

    // First read to get sizes.
    for (auto i = firstSampleToRead; i <= lastSampleToRead; i++) {
        GF_ISOSample *sample = gf_isom_get_sample_info(file_, trackNumber_, i, NULL, NULL);
        size += sample->dataLength;
        gf_isom_sample_del(&sample);
    }

    // Then read to get the actual data.
    std::unique_ptr<lightdb::bytestring> videoData(new lightdb::bytestring);
    videoData->reserve(size);
    for (auto i = firstSampleToRead; i <= lastSampleToRead; i++) {
        GF_ISOSample *sample = gf_isom_get_sample(file_, trackNumber_, i, NULL);
        videoData->insert(videoData->end(), sample->data, sample->data + sample->dataLength);
        gf_isom_sample_del(&sample);
    }

    return videoData;
}