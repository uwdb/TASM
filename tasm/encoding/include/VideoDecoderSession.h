#ifndef TASM_VIDEODECODERSESSION_H
#define TASM_VIDEODECODERSESSION_H

#include "EncodedData.h"
#include "DecodeReader.h"
#include "Frame.h"
#include "Operator.h"
#include "VideoDecoder.h"

#include <cstring>
#include <thread>

namespace tasm {
using DataQueue = tasm::spsc_queue<std::pair<std::shared_ptr<std::vector<unsigned char>>, unsigned int>>;
using EncodedReader = std::shared_ptr<Operator<CPUEncodedFrameDataPtr>>;

class VideoDecoderSession {
public:
    VideoDecoderSession(VideoDecoder &decoder, EncodedReader reader)
            : decoder_(decoder),
              nextDataQueue_(1000),
              isDoneReading_(false),
              isComplete_(false) {
        reader_ = std::make_unique<std::thread>(&VideoDecoderSession::ReadNext, std::ref(decoder_), reader,
                                                std::ref(nextDataQueue_), &isDoneReading_);
        worker_ = std::make_unique<std::thread>(&VideoDecoderSession::DecodeAll, std::ref(decoder_), std::ref(nextDataQueue_),
                                                &isDoneReading_, &isComplete_);
    }

    VideoDecoderSession(const VideoDecoderSession &) = delete;
    VideoDecoderSession(const VideoDecoderSession &&) = delete;

    ~VideoDecoderSession() {
        worker_->join();
        reader_->join();

        assert(nextDataQueue_.empty());
        assert(isDoneReading_);
    }

    bool isComplete() { return isComplete_; }

    template<typename Rep, typename Period, size_t interval=4>
    std::shared_ptr<DecodedFrame> decode(std::chrono::duration<Rep, Period> duration) {
        std::shared_ptr<CUVIDPARSERDISPINFO> packet;

        for(auto begin = std::chrono::system_clock::now();
            std::chrono::system_clock::now() - begin < duration;
            std::this_thread::sleep_for(duration / interval)) {
            bool gotFrame = false;
            if (decoder_.decodedPictureQueue().read_available()) {
                packet = decoder_.decodedPictureQueue().front();
                decoder_.decodedPictureQueue().pop();
                gotFrame = true;
            }

            if (gotFrame) {
                auto frameNumber = -1;
                auto tileNumber = -1;
                if (decoder_.frameNumberQueue()->pop(frameNumber)) {
                    decoder_.tileNumberQueue()->pop(tileNumber);
                    return std::make_shared<DecodedFrame>(decoder_, packet, frameNumber, tileNumber);
                } else
                    return std::make_shared<DecodedFrame>(decoder_, packet);
            }
        }

        return nullptr;
    }

private:
    VideoDecoder &decoder_;
    std::unique_ptr<std::thread> reader_;
    std::unique_ptr<std::thread> worker_;
    DataQueue nextDataQueue_;
    std::atomic_bool isDoneReading_;
    std::atomic_bool isComplete_;

    static CUvideoparser CreateParser(VideoDecoder &decoder) {
        CUresult status;
        CUvideoparser parser = nullptr;
        CUVIDPARSERPARAMS parameters = {
                .CodecType = decoder.createInfo().CodecType,
                .ulMaxNumDecodeSurfaces = static_cast<unsigned int>(decoder.createInfo().ulNumDecodeSurfaces),
                .ulClockRate = 0,
                .ulErrorThreshold = 0,
                .ulMaxDisplayDelay = 1,
                .uReserved1 = {},
                .pUserData = &decoder,
                .pfnSequenceCallback = HandleVideoSequence,
                .pfnDecodePicture = HandlePictureDecode,
                .pfnDisplayPicture = HandlePictureDisplay,
                {}, nullptr
        };

        if ((status = cuvidCreateVideoParser(&parser, &parameters)) != CUDA_SUCCESS)
            throw std::runtime_error("Call to cuvidCreateVideoParser failed: " + std::to_string(status));

        return parser;
    }

    static int CUDAAPI HandleVideoSequence(void *userData, CUVIDEOFORMAT *format) {
        auto *decoder = static_cast<VideoDecoder *>(userData);

        assert(format->display_area.bottom - format->display_area.top >= 0);
        assert(format->display_area.right - format->display_area.left >= 0);

        if (decoder == nullptr)
            std::cerr << "Unexpected null decoder during video decode (HandleVideoSequence)" << std::endl;
        else if (decoder->reconfigureDecoderIfNecessary(format)) {
            // Pass to skip the check below. Do the check if the decoder was not reconfigured because those are cases
            // where it should have been reconfigured.
            return 1;
        } else if ((format->codec != decoder->createInfo().CodecType) ||
                   (format->chroma_format != decoder->createInfo().ChromaFormat))
            throw std::runtime_error("Video format changed but not currently supported");

        return 1;
    }

    static int CUDAAPI HandlePictureDecode(void *userData, CUVIDPICPARAMS *parameters) {
        CUresult status;
        auto *decoder = static_cast<VideoDecoder *>(userData);

        if (decoder == nullptr)
            std::cerr << "Unexpected null decoder during video decode (HandlePictureDecode)" << std::endl;
        else {
            if ((status = cuvidDecodePicture(decoder->handle(), parameters)) != CUDA_SUCCESS)
                std::cerr << "cuvidDecodePicture failed (" << status << ")" << std::endl;
        }

        return 1;
    }

    static int CUDAAPI HandlePictureDisplay(void *userData, CUVIDPARSERDISPINFO *frame) {
        auto *decoder = static_cast<VideoDecoder *>(userData);

        if (decoder == nullptr)
            std::cerr << "Unexpected null decoder during video decode (HandlePictureDisplay)" << std::endl;
        else {
            // TODO: This should happen on a separate thread than cuvidDecodePicture() for performance.
            decoder->mapFrame(frame, decoder->currentFormat());
        }

        return 1;
    }

    static void ReadNext(VideoDecoder &decoder, EncodedReader reader, DataQueue &nextDataQueue,
                         std::atomic_bool *isDoneReading) {
        do {
            std::shared_ptr<std::vector<unsigned char>> combinedData(new std::vector<unsigned char>());
            unsigned long flags = 0;
            if (!reader->isComplete()) {
                for (auto i = 0u; i < 20; ++i) {
                    auto encodedData = reader->next();
                    if (!encodedData.has_value())
                        break;

                    int firstFrameIndex = -1;
                    int numberOfFrames = -1;
                    int tileNumber = -1;
                    bool gotFirstFrameIndex = encodedData.value()->getFirstFrameIndexIfSet(firstFrameIndex);
                    bool gotNumberOfFrames = encodedData.value()->getNumberOfFramesIfSet(numberOfFrames);
                    bool gotTileNumber = encodedData.value()->getTileNumberIfSet(tileNumber);

                    if (gotFirstFrameIndex && gotNumberOfFrames) {
                        if (decoder.frameNumberQueue()->write_available() <
                            static_cast<unsigned int>(numberOfFrames)) {
                            if (!nextDataQueue.read_available()) {
                                // There should be at least some data to decode.
                                assert(i);
                                break;
                            } else {
                                while (decoder.frameNumberQueue()->write_available() <
                                       (long unsigned int) numberOfFrames)
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                        }

                        for (int i = firstFrameIndex; i < firstFrameIndex + numberOfFrames; i++) {
                            assert(decoder.frameNumberQueue()->push(i));
                            if (gotTileNumber)
                                assert(decoder.tileNumberQueue()->push(tileNumber));
                        }
                    }
                    auto packet = encodedData.value()->packet();
                    combinedData->insert(combinedData->end(), packet.payload, packet.payload + packet.payload_size);
                    flags |= packet.flags;
                }
                while (!nextDataQueue.push(std::make_pair(combinedData, flags))) {
                    // We're getting too far ahead of the decoder. Sleep for a bit.
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        } while (!reader->isComplete());
        *isDoneReading = true;
    }

    static void
    DecodeAll(VideoDecoder &decoder, DataQueue &nextDataQueue, std::atomic_bool *isDoneReading, std::atomic_bool *isComplete) {
        CUresult status;
        auto parser = CreateParser(decoder);

        do {
            while (true) {
                if (nextDataQueue.read_available())
                    break;
                else
                    std::this_thread::yield();
            }

            auto combinedData = nextDataQueue.front().first;
            auto flags = nextDataQueue.front().second;
            nextDataQueue.pop();

            CUVIDSOURCEDATAPACKET packet;
            memset(&packet, 0, sizeof(packet));
            packet.flags = flags;
            packet.payload_size = combinedData->size();
            packet.payload = combinedData->data();
            if ((status = cuvidParseVideoData(parser, &packet)) != CUDA_SUCCESS) {
                cuvidDestroyVideoParser(parser);
                throw std::runtime_error("Call to cuvidParseVideoData failed: " + std::to_string(status));
            }

        } while (!(*isDoneReading) || nextDataQueue.read_available());

        cuvidDestroyVideoParser(parser);
        std::cout << "Decode complete; thread terminating." << std::endl;
        isComplete->store(true);
    }
};

} // namespace tasm

#endif //TASM_VIDEODECODERSESSION_H
