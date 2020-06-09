#ifndef LIGHTDB_CONFIGURATION_H
#define LIGHTDB_CONFIGURATION_H

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
    unsigned int frameRate;
    Codec codec;

    Configuration(
            unsigned int displayWidth,
            unsigned int displayHeight,
            unsigned int codedWidth,
            unsigned int codedHeight,
            unsigned int frameRate,
            Codec codec):
        displayWidth(displayWidth),
        displayHeight(displayHeight),
        codedWidth(codedWidth),
        codedHeight(codedHeight),
        frameRate(frameRate),
        codec(codec) {}
};

#endif //LIGHTDB_CONFIGURATION_H
