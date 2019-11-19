//
// Created by Maureen Daum on 2019-06-21.
//

#ifndef LIGHTDB_METADATA_H
#define LIGHTDB_METADATA_H

namespace lightdb {
    class MetadataSpecification {
    public:
        MetadataSpecification(const std::string &tableName, const std::string &columnName, const std::string &expectedValue,
                unsigned int firstFrame = 0, unsigned int lastFrame = UINT32_MAX)
            : tableName_(tableName), columnName_(columnName), expectedValue_(expectedValue),
            expectedIntValue_(-1),
            firstFrame_(firstFrame), lastFrame_(lastFrame)
        { }

        MetadataSpecification(const std::string &tableName, const std::string &columnName, int expectedValue,
                              unsigned int firstFrame = 0, unsigned int lastFrame = UINT32_MAX)
                : tableName_(tableName), columnName_(columnName), expectedValue_(""), expectedIntValue_(expectedValue),
                  firstFrame_(firstFrame), lastFrame_(lastFrame)
        { }

        MetadataSpecification(const MetadataSpecification &other, unsigned int numberOfFrames)
            : MetadataSpecification(other)
        {
            lastFrame_ = std::min(lastFrame_, numberOfFrames);
        }

        const std::string &tableName() const { return tableName_; }
        const std::string &columnName() const { return columnName_; }
        const std::string &expectedValue() const { return expectedValue_; }
        const int &expectedIntValue() const { return expectedIntValue_; }

        unsigned int firstFrame() const { return firstFrame_; }
        unsigned int lastFrame() const { return lastFrame_; }

    private:
        std::string tableName_;
        std::string columnName_;
        std::string expectedValue_;
        int expectedIntValue_;
        unsigned int firstFrame_;
        unsigned int lastFrame_;
    };

    enum MetadataSubsetType : int {
        MetadataSubsetTypeFrame = 1 << 0,
        MetadataSubsetTypePixel = 1 << 1,
        MetadataSubsetTypePixelsInFrame = MetadataSubsetTypeFrame | MetadataSubsetTypePixel,
    };

    class FrameMetadataSpecification : public MetadataSpecification {
        using MetadataSpecification::MetadataSpecification;
    };

    class PixelMetadataSpecification : public MetadataSpecification {
        using MetadataSpecification::MetadataSpecification;
    };

    class PixelsInFrameMetadataSpecification : public MetadataSpecification {
        using MetadataSpecification::MetadataSpecification;
    };

} // namespace lightdb

#endif //LIGHTDB_METADATA_H
