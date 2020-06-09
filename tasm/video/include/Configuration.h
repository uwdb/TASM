#ifndef TASM_CONFIGURATION_H
#define TASM_CONFIGURATION_H

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

    Configuration(
            unsigned int displayWidth,
            unsigned int displayHeight,
            unsigned int codedWidth,
            unsigned int codedHeight,
            unsigned int maxWidth,
            unsigned int maxHeight,
            unsigned int frameRate,
            Codec codec):
        displayWidth(displayWidth),
        displayHeight(displayHeight),
        codedWidth(codedWidth),
        codedHeight(codedHeight),
        maxWidth(maxWidth),
        maxHeight(maxHeight),
        frameRate(frameRate),
        codec(codec) {}
};

#endif //TASM_CONFIGURATION_H
