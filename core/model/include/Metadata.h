//
// Created by Maureen Daum on 2019-06-21.
//

#ifndef LIGHTDB_METADATA_H
#define LIGHTDB_METADATA_H

#include <unordered_set>

namespace lightdb {
    class MetadataElement {
    public:
        virtual std::string whereClauseConstraints(bool includeFrameLimits) const = 0;
        virtual void updateLastFrameConstraints(unsigned int lastFrame) = 0;
        virtual unsigned int firstFrame() const = 0;
        virtual unsigned int lastFrame() const = 0;
        virtual const std::vector<std::string> &objects() const { static std::vector<std::string> empty; return empty; }
    };

    class AllMetadataElement : public MetadataElement {
    public:
        AllMetadataElement(int mod = -1, const std::string &columnName = "label", unsigned int firstFrame = 0, unsigned int lastFrame = UINT32_MAX)
            : columnName_(columnName),
            mod_(mod),
            firstFrame_(firstFrame),
            lastFrame_(lastFrame) {}

        std::string whereClauseConstraints(bool includeFrameLimits) const override {
            std::string baseClause = columnName_ + " NOT LIKE 'YOLO%%' AND " + columnName_ + " NOT LIKE 'MOG%%' AND " + columnName_ + " != 'KNN'";

            if (mod_ > 0)
                baseClause += " AND frame % " + std::to_string(mod_) + " = 0";

            if (!includeFrameLimits)
                return baseClause;

            return baseClause + " AND frame >= " + std::to_string(firstFrame_) + " AND frame < " + std::to_string(lastFrame_);
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
        const std::string columnName_;
        int mod_;
        unsigned int firstFrame_;
        unsigned int lastFrame_;
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

        const std::vector<std::string> &objects() const override {
            if (objects_.size())
                return objects_;

            objects_ = std::vector<std::string>{expectedValue_.length() ? expectedValue_ : std::to_string(expectedIntValue_) };
            return objects_;
        };

    private:
        std::string columnName_;
        std::string expectedValue_;
        int expectedIntValue_;
        unsigned int firstFrame_;
        unsigned int lastFrame_;
        mutable std::vector<std::string> objects_;
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

        const std::vector<std::string> &objects() const override {
            // Not implemented.
            assert(false);
            static std::vector<std::string> empty;
            return empty;
        }

    private:
        std::vector<std::shared_ptr<MetadataElement>> elements_;
    };

    class OrMetadataElement : public MetadataElement {
    public:
        OrMetadataElement(const std::vector<std::shared_ptr<MetadataElement>> &elements)
            : elements_(elements) {}

        std::string whereClauseConstraints(bool includeFrameLimits) const override {
            if (includeFrameLimits && !constraintWithLimits_.empty())
                return constraintWithLimits_;
            else if (!includeFrameLimits && !constraintWithoutLimits_.empty())
                return constraintWithoutLimits_;

            std::string &constraint = includeFrameLimits ? constraintWithLimits_ : constraintWithoutLimits_;

            for (auto i = 0u; i < elements_.size(); ++i) {
                constraint += "(" + elements_[i]->whereClauseConstraints(includeFrameLimits) + ")";
                if (i < elements_.size() - 1)
                    constraint += " OR ";
            }
            return constraint;
        }

        void updateLastFrameConstraints(unsigned int lastFrame) override {
            for (auto element : elements_)
                element->updateLastFrameConstraints(lastFrame);
        }

        unsigned int firstFrame() const override {
            std::vector<unsigned int> firstFrames(elements_.size());
            std::transform(elements_.begin(), elements_.end(), firstFrames.begin(), [](auto m) {
                return m->firstFrame();
            });
            return *std::min_element(firstFrames.begin(), firstFrames.end());
        }

        unsigned int lastFrame() const override {
            std::vector<unsigned int> lastFrames(elements_.size());
            std::transform(elements_.begin(), elements_.end(), lastFrames.begin(), [](auto m) {
                return m->lastFrame();
            });
            return *std::max_element(lastFrames.begin(), lastFrames.end());
        }

        const std::vector<std::string> &objects() const override {
            if (objects_)
                return *objects_;

            objects_.reset(new std::vector<std::string>());
            for (auto it = elements_.begin(); it != elements_.end(); ++it) {
                objects_->insert(objects_->end(), (**it).objects().begin(), (**it).objects().end());
            }
            return *objects_;
        }

    private:
        std::vector<std::shared_ptr<MetadataElement>> elements_;
        mutable std::string constraintWithLimits_;
        mutable std::string constraintWithoutLimits_;
        mutable std::unique_ptr<std::vector<std::string>> objects_;
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
        const std::vector<std::string> &objects() const { return metadataElement_->objects(); }

        std::string whereClauseConstraints(bool includeFrameLimits = false) const {
            return metadataElement_->whereClauseConstraints(includeFrameLimits);
        }
        virtual ~MetadataSpecification() = default;

    private:
        std::string tableName_;
        std::shared_ptr<MetadataElement> metadataElement_;
    };

    struct ROI {
        ROI()
            : x1(0), y1(0), x2(0), y2(0) {}
        ROI(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
            : x1(x1), y1(y1), x2(x2), y2(y2) {}
        unsigned int x1;
        unsigned int y1;
        unsigned int x2;
        unsigned int y2;
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

    class FrameSpecification {
    public:
        FrameSpecification(const std::vector<int> &framesToRead)
            : framesToRead_(framesToRead),
            framesToReadAsSet_(framesToRead_.begin(), framesToRead_.end())
        { }

        const std::vector<int> &framesToRead() const { return framesToRead_; }
        const std::unordered_set<int> &framesToReadAsASet() const { return framesToReadAsSet_; }
    private:
        std::vector<int> framesToRead_;
        std::unordered_set<int> framesToReadAsSet_;
    };

} // namespace lightdb

#endif //LIGHTDB_METADATA_H
