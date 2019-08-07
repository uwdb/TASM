//
// Created by Maureen Daum on 2019-06-21.
//

#ifndef LIGHTDB_METADATA_H
#define LIGHTDB_METADATA_H

namespace lightdb {
    class MetadataSpecification {
    public:
        MetadataSpecification(const std::string &tableName, const std::string &columnName, const std::string &expectedValue)
            : tableName_(tableName), columnName_(columnName), expectedValue_(expectedValue)
        { }

        const std::string &tableName() const { return tableName_; }
        const std::string &columnName() const { return columnName_; }
        const std::string &expectedValue() const { return expectedValue_; }

    private:
        std::string tableName_;
        std::string columnName_;
        std::string expectedValue_;
    };

} // namespace lightdb

#endif //LIGHTDB_METADATA_H