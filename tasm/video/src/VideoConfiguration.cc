#include "VideoConfiguration.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
extern "C" {
#include <libavformat/avformat.h>
}

namespace tasm::video {

Codec CodecFromAVCodec(AVCodecID codecId) {
    switch (codecId) {
        case AV_CODEC_ID_H264:
            return Codec::H264;
        case AV_CODEC_ID_HEVC:
            return Codec::HEVC;
        default:
            assert(false);
    }
}

std::unique_ptr<Configuration> GetConfiguration(const std::experimental::filesystem::path &path) {
    int result;
    char error[AV_ERROR_MAX_STRING_SIZE];
    auto context = avformat_alloc_context();

    std::unique_ptr<Configuration> configuration;
    try {
        if ((result = avformat_open_input(&context, path.c_str(), nullptr, nullptr)) < 0) {
            throw std::runtime_error(av_make_error_string(error, AV_ERROR_MAX_STRING_SIZE, result));
        } else if ((result = avformat_find_stream_info(context, nullptr)) < 0) {
            throw std::runtime_error(av_make_error_string(error, AV_ERROR_MAX_STRING_SIZE, result));
        } else if (context->bit_rate < 0) {
            throw std::runtime_error("No bitrate detected");
        }

        assert(context->nb_streams == 1);
        auto stream = context->streams[0];

        auto displayWidth = stream->codecpar->width;
        auto displayHeight = stream->codecpar->height;
        auto codedWidth = stream->codec->coded_width;
        auto codedHeight = stream->codec->coded_height;
        auto codec = CodecFromAVCodec(stream->codecpar->codec_id);
        if (stream->r_frame_rate.den != 1) {
            std::cerr << "Framerate must be a whole number" << std::endl;
            assert(false);
        }
        auto framerate = stream->r_frame_rate.num;
        configuration.reset(new Configuration(
                displayWidth, displayHeight,
                codedWidth, codedHeight,
                codedWidth, codedHeight,
                framerate, codec, static_cast<unsigned int>(context->bit_rate)));

        avformat_close_input(&context);
        avformat_free_context(context);
    } catch (const std::exception&) {
        avformat_close_input(&context);
        avformat_free_context(context);
        throw;
    }

    return configuration;
}

} // tasm::video