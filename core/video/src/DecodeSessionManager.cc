#include "DecodeSessionManager.h"

namespace lightdb {

std::list<decoded::DecodedFrameInformation> DecodeSession::read() {
    if (decoder_->frame_queue().isComplete())
        return {};

    std::list<decoded::DecodedFrameInformation> frames;
    do {
        auto decodedInfo = decode(std::chrono::milliseconds(1u));
        if (decodedInfo.has_value()) {
            frames.emplace_back(decodedInfo.value());
        }
    } while (!decoder_->frame_queue().isEmpty()
//                && !decoder_.frame_queue().isEndOfDecode() // Not sure if this is correct because we want to add frames as long as it's not empty.
                && frames.size() <=  configuration_.output_surfaces / 4);

    if (decoder_->frameNumberQueue().read_available()) {
        auto next = decoder_->frameNumberQueue().front();
        assert(next);
    }

    return frames;
}

std::optional<decoded::DecodedFrameInformation> DecodeSession::decode(std::chrono::milliseconds duration) {
    std::shared_ptr<CUVIDPARSERDISPINFO> packet;
    for (auto begin = std::chrono::system_clock::now();
         std::chrono::system_clock::now() - begin < duration;
         std::this_thread::sleep_for(duration / 4)) {
        if ((packet = decoder_->frame_queue().try_dequeue<CUVIDPARSERDISPINFO>()) != nullptr) {
            auto frameNumber = -1;
            assert(decoder_->frameNumberQueue().pop(frameNumber));
            unsigned int picIndex = packet->picture_index;
            auto frameInfo = decoder_->frameInfoForPicIndex(picIndex);
            auto returnVal = decoded::DecodedFrameInformation{static_cast<unsigned int>(frameNumber),
                                                    frameInfo.first,
                                                    frameInfo.second,
                                                    decoder_->videoFormatForPicIndex(picIndex)};

            // When packet goes out of scope, the picture index will be released.
            decoder_->removeFrameInfoForPicIndex(picIndex);
            return returnVal;
        }
    }
    return std::nullopt;
}

void DecodeSessionManager::setUpDecoders(VideoLock &lock, DecodeConfiguration &decodeConfiguration) {
    decodeSessions_.reserve(numberOfSessions_);
    for (auto i = 0u; i < numberOfSessions_; ++i) {
        decodeSessions_.emplace_back(std::make_unique<DecodeSession>(i, lock, decodeConfiguration));
        availableDecoders_.insert(i);
    }
}

void DecodeSessionManager::markDecoderAsAvailable(unsigned int index) {
    // This function assumes that the mutex is already being held.
    if (!tileAndEncodedDataQueue_.empty()) {
        auto &info = tileAndEncodedDataQueue_.front();
        decodeSessions_[index]->decodePacket(info.first, info.second);
        tileAndEncodedDataQueue_.pop();
    } else {
        availableDecoders_.insert(index);
    }
}

std::unordered_map<unsigned int, std::list<decoded::DecodedFrameInformation>> DecodeSessionManager::read()
{
    // Read from each decoder.
    // If the decoder is done decoding, add it to the list of available decoders.
    std::scoped_lock lock(lock_);

    std::unordered_map<unsigned int, std::list<decoded::DecodedFrameInformation>> decodedFrameInfo;

    for (auto i = 0u; i < numberOfSessions_; ++i) {
        auto &decodeSession = *decodeSessions_[i];
        if (!decodeSession.isComplete()) {
            auto decodedFrames = decodeSession.read();
            if (decodedFrames.size())
                decodedFrameInfo.emplace(decodeSession.tileNumber(), decodedFrames);
        }

        // Check if it's complete after reading because that may have emptied it.
        if (decodeSession.isComplete())
            markDecoderAsAvailable(i);
    }

    return decodedFrameInfo;
}

} // namespace lightdb