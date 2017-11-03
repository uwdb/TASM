#ifndef VISUALCLOUD_DECODEREADER_H
#define VISUALCLOUD_DECODEREADER_H

#include <boost/lockfree/spsc_queue.hpp>
#include "dynlink_cuda.h"
#include "PThreadMutex.h"

struct DecodeReaderPacket: public CUVIDSOURCEDATAPACKET {
public:
    DecodeReaderPacket() { }

    DecodeReaderPacket(const DecodeReaderPacket &packet)
        : CUVIDSOURCEDATAPACKET{packet}, buffer_(packet.buffer_)
    { }

    DecodeReaderPacket(const CUVIDSOURCEDATAPACKET &packet)
        : CUVIDSOURCEDATAPACKET{packet.flags, packet.payload_size, nullptr, packet.timestamp},
          buffer_(std::make_shared<std::vector<unsigned char>>())
    {
        buffer_ = std::make_shared<std::vector<unsigned char>>();
        buffer_->reserve(payload_size);
        buffer_->insert(buffer_->begin(), packet.payload, packet.payload + payload_size);
        payload = buffer_->data();
    }

    DecodeReaderPacket& operator=(const DecodeReaderPacket &packet) {
        CUVIDSOURCEDATAPACKET::operator=(packet);
        buffer_ = packet.buffer_;
        payload = buffer_->data();

        return *this;
    }

private:
    std::shared_ptr<std::vector<unsigned char>> buffer_;
};

class DecodeReader {
public:
    virtual std::optional<DecodeReaderPacket> ReadPacket() = 0;
    virtual CUVIDEOFORMAT format() const = 0;
    virtual bool isComplete() const = 0;
};

class FileDecodeReader: public DecodeReader {
public:
    FileDecodeReader(const std::string &filename)
        : FileDecodeReader(filename.c_str()) { }

    FileDecodeReader(const char *filename)
            : filename_(filename),
              packets(), // Must be initialized before source
              source(CreateVideoSource(filename)),
              format_(GetVideoSourceFormat(source)) {
        if(format().codec != cudaVideoCodec_H264 && format().codec != cudaVideoCodec_HEVC)
            throw std::runtime_error("Reader only supports H264/HEVC input video"); //TODO
        else if(format().chroma_format != cudaVideoChromaFormat_420)
            throw std::runtime_error("Reader only supports 4:2:0 chroma"); // TODO
    }

    CUVIDEOFORMAT format() const override { return format_; }
    const std::string &filename() const { return filename_; }

    std::optional<DecodeReaderPacket> ReadPacket() override {
        DecodeReaderPacket packet;

        while (cuvidGetVideoSourceState(source) == cudaVideoState_Started &&
                !packets.read_available())
            std::this_thread::yield();

        return packets.pop(packet)
               ? packet
               : std::optional<DecodeReaderPacket>();
    }

    bool isComplete() const override {
        return !packets.read_available() && cuvidGetVideoSourceState(source) != cudaVideoState_Started;
    }

private:
    static int CUDAAPI HandleVideoData(void *userData, CUVIDSOURCEDATAPACKET *packet) {
        FileDecodeReader *reader = static_cast<FileDecodeReader*>(userData);

        CUVIDSOURCEDATAPACKET *copy = new CUVIDSOURCEDATAPACKET{
                .flags = packet->flags,
                .payload_size = packet->payload_size,
                .payload =
                new unsigned char[packet->payload_size],
                .timestamp = packet->timestamp
        };
        memcpy(const_cast<unsigned char*>(copy->payload), const_cast<unsigned char*>(packet->payload), packet->payload_size);
        DecodeReaderPacket drp(*packet);

        while(!reader->packets.push(drp))
        //while(!reader->packets.push(*copy))
        //while(!reader->packets.push(*packet))
            std::this_thread::yield();

        return 1;
    }

    CUvideosource CreateVideoSource(const char *filename) {
        CUresult status;
        CUvideosource source;
        CUVIDSOURCEPARAMS videoSourceParameters = {
                .ulClockRate = 0,
                .uReserved1 = {},
                .pUserData = this,
                .pfnVideoDataHandler = HandleVideoData,
                .pfnAudioDataHandler = nullptr,
                0
        };

        if((status = cuvidCreateVideoSource(&source, filename, &videoSourceParameters)) != CUDA_SUCCESS)
            throw std::runtime_error(std::to_string(status) + "DecodeReader.cuvidCreateVideoSource"); //TODO
        else if((status = cuvidSetVideoSourceState(source, cudaVideoState_Started)) != CUDA_SUCCESS)
            throw std::runtime_error(std::to_string(status) + "DecodeReader.cuvidSetVideoSourceState"); //TODO

        return source;
    }

    CUVIDEOFORMAT GetVideoSourceFormat(CUvideosource source) {
        CUresult status;
        CUVIDEOFORMAT format;

        if((status = cuvidGetSourceVideoFormat(source, &format, 0)) != CUDA_SUCCESS)
            throw std::runtime_error(std::to_string(status) + "DecodeReader.GetVideoSourceFormat"); //TODO
        return format;
    }

    std::string filename_;
    boost::lockfree::spsc_queue<DecodeReaderPacket, boost::lockfree::capacity<4096>> packets;
    CUvideosource source;
    CUVIDEOFORMAT format_;
};

#endif //VISUALCLOUD_DECODEREADER_H
