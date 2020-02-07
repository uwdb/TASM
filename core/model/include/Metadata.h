//
// Created by Maureen Daum on 2019-06-21.
//

#ifndef LIGHTDB_METADATA_H
#define LIGHTDB_METADATA_H

namespace lightdb {
    class MetadataElement {
    public:
        virtual std::string whereClauseConstraints(bool includeFrameLimits) const = 0;
        virtual void updateLastFrameConstraints(unsigned int lastFrame) = 0;
        virtual unsigned int firstFrame() const = 0;
        virtual unsigned int lastFrame() const = 0;
    };

    class SingleMetadataElement : public MetadataElement {
    public:
        SingleMetadataElement(const std::string &columnName, const std::string &expectedValue, unsigned int firstFrame = 0, unsigned int lastFrame = UINT32_MAX)
            : columnName_(columnName), expectedValue_(expectedValue), expectedIntValue_(-1), firstFrame_(firstFrame), lastFrame_(lastFrame) {}

        SingleMetadataElement(const std::string &columnName, int expectedValue, unsigned int firstFrame = 0, unsigned int lastFrame = UINT32_MAX)
                : columnName_(columnName), expectedValue_(""), expectedIntValue_(expectedValue), firstFrame_(firstFrame), lastFrame_(lastFrame) {}

        std::string whereClauseConstraints(bool includeFrameLimits) const override {
            std::string valueConstraint = columnName_ + "= ";
            if (expectedValue_.length())
                valueConstraint += "'" + expectedValue_ + "'" ;
            else
                valueConstraint += std::to_string(expectedIntValue_);

            if (!includeFrameLimits)
                return valueConstraint;

            return valueConstraint + " AND frame >= " + std::to_string(firstFrame_) + " AND frame < " + std::to_string(lastFrame_);
        }

        void updateLastFrameConstraints(unsigned int lastFrame) override {
            lastFrame_ = std::min(lastFrame_, lastFrame);
        }

        unsigned int firstFrame() const override {
            return firstFrame_;
        }
        unsigned int lastFrame() const override {
            return lastFrame_;
        }

    private:
        std::string columnName_;
        std::string expectedValue_;
        int expectedIntValue_;
        unsigned int firstFrame_;
        unsigned int lastFrame_;
    };

    class AndMetadataElement : public MetadataElement {
    public:
        AndMetadataElement(const std::vector<std::shared_ptr<MetadataElement>> &elements)
            : elements_(elements) {}

        std::string whereClauseConstraints(bool includeFrameLimits) const override {
            std::string constraint = "";
            for (auto i = 0u; i < elements_.size(); ++i) {
                constraint += elements_[i]->whereClauseConstraints(includeFrameLimits);
                if (i < elements_.size() - 1)
                    constraint += " AND ";
            }
            return constraint;
        }

        void updateLastFrameConstraints(unsigned int lastFrame) override {
            for (auto element : elements_)
                element->updateLastFrameConstraints(lastFrame);
        }

        unsigned int firstFrame() const override {
            std::vector<unsigned int> firstFrames(elements_.size());
            std::transform(elements_.begin(), elements_.end(), firstFrames.begin(), [](std::shared_ptr<MetadataElement> m) { return m->firstFrame(); });
            return *std::min_element(firstFrames.begin(), firstFrames.end());
        }

        unsigned int lastFrame() const override {
            std::vector<unsigned int> lastFrames(elements_.size());
            std::transform(elements_.begin(), elements_.end(), lastFrames.begin(), [](std::shared_ptr<MetadataElement> m) { return m->lastFrame(); });
            return *std::max_element(lastFrames.begin(), lastFrames.end());
        }

    private:
        std::vector<std::shared_ptr<MetadataElement>> elements_;
    };

    class MetadataSpecification {
    public:
        MetadataSpecification(const std::string &tableName, std::shared_ptr<MetadataElement> metadataElement)
            : tableName_(tableName), metadataElement_(metadataElement) {}

        MetadataSpecification(const MetadataSpecification &other, unsigned int numberOfFrames)
            : MetadataSpecification(other)
        {
            metadataElement_->updateLastFrameConstraints(numberOfFrames);
        }

        const std::string &tableName() const { return tableName_; }
        unsigned int firstFrame() const { return metadataElement_->firstFrame(); }
        unsigned int lastFrame() const { return metadataElement_->lastFrame(); }

        std::string whereClauseConstraints(bool includeFrameLimits = false) const {
            return metadataElement_->whereClauseConstraints(includeFrameLimits);
        }
        virtual ~MetadataSpecification() = default;

    private:
        std::string tableName_;
        std::shared_ptr<MetadataElement> metadataElement_;
    };

//    class MetadataAndSpecification : public MetadataSpecification {
//    public:
//        MetadataAndSpecification(const std::vector<MetadataSpecification> &components) :
//            components_(components) {}
//        std::string whereClauseConstraints(bool includeFrameLimits = false) const override;
//
//    private:
//        std::vector<MetadataSpecification> components_;
//    };

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
