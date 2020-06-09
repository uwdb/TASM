#ifndef TASM_VIDEODECODERSESSION_H
#define TASM_VIDEODECODERSESSION_H

#include "DecodeReader.h"
#include "VideoDecoder.h"

#include <cstring>
#include <thread>

using DataQueue = spsc_queue<std::pair<std::shared_ptr<std::vector<unsigned char>>, unsigned int>>;

template<typename Input = DecodeReader::iterator>
class VideoDecoderSession {
public:
    VideoDecoderSession(std::shared_ptr<VideoDecoder> decoder, Input reader, const Input end)
            : decoder_(decoder),
              nextDataQueue_(1000),
              isDoneReading_(false) {
        reader_ = std::make_unique<std::thread>(&VideoDecoderSession::ReadNext, decoder, reader, end,
                                                std::ref(nextDataQueue_), &isDoneReading_);
        worker_ = std::make_unique<std::thread>(&VideoDecoderSession::DecodeAll, decoder, reader, end, std::ref(nextDataQueue_), &isDoneReading_);
    }

    VideoDecoderSession(const VideoDecoderSession&) = delete;
    VideoDecoderSession(const VideoDecoderSession&&) = delete;

    ~VideoDecoderSession() {
        worker_->join();
        reader_->join();

        assert(nextDataQueue_.empty());
        assert(isDoneReading_);
    }

private:
    std::shared_ptr<VideoDecoder> decoder_;
    std::unique_ptr<std::thread> reader_;
    std::unique_ptr<std::thread> worker_;
    DataQueue nextDataQueue_;
    std::atomic_bool isDoneReading_;

    static CUvideoparser CreateParser(std::shared_ptr<VideoDecoder> decoder) {
        CUresult status;
        CUvideoparser parser = nullptr;
        CUVIDPARSERPARAMS parameters = {
                .CodecType = decoder->createInfo().CodecType,
                .ulMaxNumDecodeSurfaces = static_cast<unsigned int>(decoder->createInfo().ulNumDecodeSurfaces),
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
        auto* decoder = static_cast<VideoDecoder*>(userData);

        assert(format->display_area.bottom - format->display_area.top >= 0);
        assert(format->display_area.right - format->display_area.left >= 0);

        if(decoder == nullptr)
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
        auto* decoder = static_cast<VideoDecoder*>(userData);

        if(decoder == nullptr)
            std::cerr << "Unexpected null decoder during video decode (HandlePictureDecode)" << std::endl;
        else {
            if((status = cuvidDecodePicture(decoder->handle(), parameters)) != CUDA_SUCCESS)
                std::cerr << "cuvidDecodePicture failed (" << status << ")" << std::endl;
        }

        return 1;
    }

    static int CUDAAPI HandlePictureDisplay(void *userData, CUVIDPARSERDISPINFO *frame) {
        auto* decoder = static_cast<VideoDecoder*>(userData);

        if(decoder == nullptr)
            std::cerr << "Unexpected null decoder during video decode (HandlePictureDisplay)" << std::endl;
        else {
            // TODO: This should happen on a separate thread than cuvidDecodePicture() for performance.
            decoder->mapFrame(frame, decoder->currentFormat());
        }

        return 1;
    }

    static void ReadNext(std::shared_ptr<VideoDecoder> decoder, Input reader, Input end, DataQueue &nextDataQueue,
                         std::atomic_bool *isDoneReading) {
        do {
            std::shared_ptr<std::vector<unsigned char>> combinedData(new std::vector<unsigned char>());
            unsigned long flags = 0;
            if (reader != end) {
                for (auto i = 0u; i < 20; ++i) {
                    if (reader != end) {
                        int firstFrameIndex = -1;
                        int numberOfFrames = -1;
                        int tileNumber = -1;
                        bool gotFirstFrameIndex = (*reader).getFirstFrameIndexIfSet(firstFrameIndex);
                        bool gotNumberOfFrames = (*reader).getNumberOfFramesIfSet(numberOfFrames);
                        bool gotTileNumber = (*reader).getTileNumberIfSet(tileNumber);

                        if (gotFirstFrameIndex && gotNumberOfFrames) {
                            if (decoder->frameNumberQueue()->write_available() <
                                static_cast<unsigned int>(numberOfFrames)) {
                                if (!nextDataQueue.read_available()) {
                                    // There should be at least some data to decode.
                                    assert(i);
                                    break;
                                } else {
                                    while (decoder->frameNumberQueue()->write_available() <
                                           (long unsigned int) numberOfFrames)
                                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                }
                            }

                            for (int i = firstFrameIndex; i < firstFrameIndex + numberOfFrames; i++) {
                                assert(decoder->frameNumberQueue()->push(i));
                                if (gotTileNumber)
                                    assert(decoder->tileNumberQueue()->push(tileNumber));
                            }
                        }

                        auto packet = static_cast<DecodeReaderPacket>(reader++);
                        combinedData->insert(combinedData->end(), packet.payload, packet.payload + packet.payload_size);
                        flags |= packet.flags;
                    }
                }
                while (!nextDataQueue.push(std::make_pair(combinedData, flags))) {
                    // We're getting too far ahead of the decoder. Sleep for a bit.
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        } while (reader != end);
        *isDoneReading = true;
    }

    static void DecodeAll(std::shared_ptr<VideoDecoder> decoder, Input reader, const Input end, DataQueue &nextDataQueue, std::atomic_bool *isDoneReading) {
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

        }  while (!(*isDoneReading) || nextDataQueue.read_available());

        cuvidDestroyVideoParser(parser);
        std::cout << "Decode complete; thread terminating." << std::endl;
    }
};

#endif //TASM_VIDEODECODERSESSION_H
