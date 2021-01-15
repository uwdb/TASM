#ifndef LIGHTDB_STATSCOLLECTOR_H
#define LIGHTDB_STATSCOLLECTOR_H

#include <sstream>
#include <list>
#include <optional>

namespace lightdb {

class StatsCollector {
public:
    static StatsCollector &instance() {
        if (instance_.has_value())
            return instance_.value();
        instance_.emplace(StatsCollector());
        return instance_.value();
    }

    void setUpNewQuery(const std::string &video, unsigned int qid, const std::string &variant, const std::string &component) {
        video_ = video;
        qid_ = qid;
        variant_ = variant;
        component_ = component;
    }

    void setQueryComponent(const std::string &component) {
        component_ = component;
    }

    void addRuntime(const std::string &key, unsigned long duration) {
        stats_.emplace_back<Row>({video_, qid_, variant_, component_, key, 1, std::to_string(duration)});
    }

    void addStat(const std::string &key, unsigned long value) {
        stats_.emplace_back<Row>({video_, qid_, variant_, component_, key, 0, std::to_string(value)});
    }

    void addStat(const std::string &key, const std::string &value) {
        stats_.emplace_back<Row>({video_, qid_, variant_, component_, key, 0, value});
    }

    std::string toCSV() {
        std::stringstream sstream;
        for (auto &row : stats_) {
            sstream << row.video << sep_
                    << row.qid << sep_
                    << row.variant << sep_
                    << row.component << sep_
                    << row.key << sep_
                    << row.isRuntime << sep_
                    << row.value
                    << "\n";
        }
        stats_.clear();
        return sstream.str();
    }

private:
    struct Row {
        std::string video;
        unsigned int qid;
        std::string variant;
        std::string component;
        std::string key;
        unsigned int isRuntime;
        std::string value;
    };

    static std::optional<StatsCollector> instance_;
    std::string video_;
    unsigned int qid_;
    std::string variant_;
    std::string component_;
    std::list<Row> stats_;
    static constexpr char const *sep_ = ",";
};

} // namespace lightdb

#endif //LIGHTDB_STATSCOLLECTOR_H
