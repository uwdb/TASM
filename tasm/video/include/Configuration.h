#ifndef TASM_CONFIGURATION_H
#define TASM_CONFIGURATION_H

#include "EncodeAPI.h"
#include <string>

enum class Codec {
    Unknown,
    H264,
    HEVC,
};

struct Configuration {
    unsigned int displayWidth;
    unsigned int displayHeight;
    unsigned int codedWidth;
    unsigned int codedHeight;
    unsigned int maxWidth;
    unsigned int maxHeight;
    unsigned int frameRate;
    Codec codec;
    unsigned int bitrate;

    Configuration(
            unsigned int displayWidth,
            unsigned int displayHeight,
            unsigned int codedWidth,
            unsigned int codedHeight,
            unsigned int maxWidth,
            unsigned int maxHeight,
            unsigned int frameRate,
            Codec codec,
            unsigned int bitrate):
        displayWidth(displayWidth),
        displayHeight(displayHeight),
        codedWidth(codedWidth),
        codedHeight(codedHeight),
        maxWidth(maxWidth),
        maxHeight(maxHeight),
        frameRate(frameRate),
        codec(codec),
        bitrate(bitrate) {}

    Configuration() = default;
};

struct EncodeConfiguration: public Configuration
{
    EncodeCodec               codec;
    NV_ENC_BUFFER_FORMAT      inputFormat;
    GUID                      preset;
    unsigned int              gopLength;
    unsigned int              numB;
    NV_ENC_PIC_STRUCT         pictureStruct;

    // Video buffering verifier (VBV) parameters
    struct {
        unsigned int          maxBitrate;
        int                   size;
    } videoBufferingVerifier;

    // Quantization parameters
    struct {
        unsigned int     quantizationParameter;
        std::string      deltaMapFilename;
        NV_ENC_PARAMS_RC_MODE rateControlMode;

        // I frame initial quantization: qp * factor + offset
        float            i_quant_factor;
        float            i_quant_offset;

        // B frame initial quantization: qp * factor + offset
        float            b_quant_factor;
        float            b_quant_offset;
    } quantization;

    // How often should an IDR refresh frame be injected?
    // Ignored when GOP != NVENC_INFINITE_GOPLENGTH
    struct {
        bool enabled;
        unsigned int period;   // Number of frames to perform the refresh over
        unsigned int duration; // Period between successive refreshes
    } intraRefresh;

    struct {
        bool enableMEOnly;
        bool enableAsyncMode;
        bool enableTemporalAQ;
        bool enableReferenceFrameInvalidation;
    } flags;

    EncodeConfiguration(const struct EncodeConfiguration&) = default;

    EncodeConfiguration(const struct EncodeConfiguration& model, const unsigned int height, const unsigned int width)
            : EncodeConfiguration(model)
    {
        this->displayHeight = height;
        this->displayWidth = width;
        this->maxHeight = model.maxHeight != 0 && model.maxHeight < height ? height : model.maxHeight;
        this->maxWidth = model.maxWidth != 0 && model.maxWidth < width ? width : model.maxWidth;
    }

    EncodeConfiguration(const Configuration &configuration,
                        const EncodeCodec codec,
                        const unsigned int gop_length,
                        const NV_ENC_PARAMS_RC_MODE rateControlMode=NV_ENC_PARAMS_RC_CONSTQP,
                        const unsigned int b_frames=0,
                        const NV_ENC_BUFFER_FORMAT input_format=NV_ENC_BUFFER_FORMAT_NV12,
                        const NV_ENC_PIC_STRUCT picture_struct=NV_ENC_PIC_STRUCT_FRAME,
                        const float i_qfactor=DEFAULT_I_QFACTOR,
                        const float b_qfactor=DEFAULT_B_QFACTOR,
                        const float i_qoffset=DEFAULT_I_QOFFSET,
                        const float b_qoffset=DEFAULT_B_QOFFSET):
            EncodeConfiguration(configuration,
                                codec, NV_ENC_PRESET_DEFAULT_GUID,
                    //configuration.framerate,
                                gop_length, configuration.bitrate,
                                rateControlMode, b_frames, input_format, picture_struct,
                                i_qfactor, b_qfactor, i_qoffset, b_qoffset)
    { }

    EncodeConfiguration(const Configuration &configuration,
                        const EncodeCodec codec,
                        const std::string &preset,
                        const unsigned int gop_length,
                        const NV_ENC_PARAMS_RC_MODE rateControlMode=NV_ENC_PARAMS_RC_CONSTQP,
                        const unsigned int b_frames=0,
                        const NV_ENC_BUFFER_FORMAT input_format=NV_ENC_BUFFER_FORMAT_NV12,
                        const NV_ENC_PIC_STRUCT picture_struct=NV_ENC_PIC_STRUCT_FRAME,
                        const float i_qfactor=DEFAULT_I_QFACTOR,
                        const float b_qfactor=DEFAULT_B_QFACTOR,
                        const float i_qoffset=DEFAULT_I_QOFFSET,
                        const float b_qoffset=DEFAULT_B_QOFFSET):
            EncodeConfiguration(configuration,
                                codec, EncodeAPI::GetPresetGUID(preset.c_str(), codec),
                    //configuration.framerate,
                                gop_length, configuration.bitrate,
                                rateControlMode, b_frames, input_format, picture_struct,
                                i_qfactor, b_qfactor, i_qoffset, b_qoffset)
    { }

    EncodeConfiguration(const Configuration &configuration,
                        const EncodeCodec codec, GUID preset, //const Configuration::FrameRate &fps,
                        const unsigned int gop_length, const size_t bitrate,
                        const NV_ENC_PARAMS_RC_MODE rateControlMode=NV_ENC_PARAMS_RC_CONSTQP,
                        const unsigned int b_frames=0,
                        const NV_ENC_BUFFER_FORMAT input_format=NV_ENC_BUFFER_FORMAT_NV12,
                        const NV_ENC_PIC_STRUCT picture_struct=NV_ENC_PIC_STRUCT_FRAME,
                        const float i_qfactor=DEFAULT_I_QFACTOR,
                        const float b_qfactor=DEFAULT_B_QFACTOR,
                        const float i_qoffset=DEFAULT_I_QOFFSET,
                        const float b_qoffset=DEFAULT_B_QOFFSET) :
            Configuration{configuration},
            codec(codec),
            inputFormat(input_format),
            preset(preset),
            gopLength(gop_length),
            numB(b_frames),
            pictureStruct(picture_struct),
            videoBufferingVerifier{},
            quantization{28, "", rateControlMode, i_qfactor, b_qfactor, i_qoffset, b_qoffset},
            intraRefresh{false, 0, 0},
            flags{false, false, false, false}
    { }
};

#endif //TASM_CONFIGURATION_H
