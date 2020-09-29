#ifndef TASM_WORKLOADWRAPPERS_H
#define TASM_WORKLOADWRAPPERS_H

#include "SemanticDataManager.h"
#include "SemanticSelection.h"
#include "Tasm.h"
#include "TiledVideoManager.h"
#include "TileLocationProvider.h"
#include "WorkloadCostEstimator.h"
#include "Video.h"
#include <boost/python/list.hpp>
#include <boost/python/wrapper.hpp>

namespace tasm::python {
class Query {
public:
    Query(const std::string &video,
            const std::string &label,
            unsigned int firstFrameInclusive,
            unsigned int lastFrameExclusive)
        : Query(video, "", label, firstFrameInclusive, lastFrameExclusive) {}

    Query(const std::string &video,
            const std::string label)
        : Query(video, "", label, 0, 0) {}

    Query(const std::string &video,
            const std::string &label,
            unsigned int frame)
        : Query(video, "", label, frame, frame + 1) {}

    Query(const std::string &video,
            const std::string &metadataIdentifier,
            const std::string &label)
        : Query(video, metadataIdentifier, label, 0, 0) {}

    Query(const std::string &video,
            const std::string &metadataIdentifier,
            const std::string &label,
            unsigned int firstFrameInclusive,
            unsigned int lastFrameExclusive)
        : video_(video),
        metadataIdentifier_(metadataIdentifier),
        label_(label),
        firstFrameInclusive_(firstFrameInclusive),
        lastFrameExclusive_(lastFrameExclusive)
    {}

    std::shared_ptr<SemanticDataManager> toSemanticDataManager(TASM &tasm) const {
        auto temporalSelection = std::shared_ptr<TemporalSelection>();
        if (lastFrameExclusive_)
            temporalSelection = std::make_shared<RangeTemporalSelection>(firstFrameInclusive_, lastFrameExclusive_);
        return std::make_shared<SemanticDataManager>(
                tasm.semanticIndex(),
                video_,
                std::make_shared<SingleMetadataSelection>(std::string(label_)),
                temporalSelection);
    }

private:
    std::string video_;
    std::string metadataIdentifier_;
    std::string label_;
    unsigned int firstFrameInclusive_;
    unsigned int lastFrameExclusive_;
};

template <typename T>
std::vector<T> extract(boost::python::list l) {
    std::vector<T> result;
    for (auto i = 0; i < boost::python::len(l); ++i) {
        result.push_back(boost::python::extract<T>(l[i]));
    }
    return result;
}

template <typename T>
boost::python::list listify(std::vector<T> vec) {
    auto list = boost::python::list();
    for (const auto &i : vec)
        list.append(i);
    return list;
}

class PythonWorkload {
public:
    PythonWorkload(boost::python::list queries, boost::python::list counts)
        : queries_(extract<Query>(queries)),
        counts_(extract<unsigned int>(counts))
    {}

    std::shared_ptr<Workload> toWorkload(TASM &tasm) const {
        std::vector<std::shared_ptr<SemanticDataManager>> selections(queries_.size());
        std::transform(queries_.begin(), queries_.end(), selections.begin(), [&](const Query &query) {
            return query.toSemanticDataManager(tasm);
        });
        return std::make_shared<Workload>(selections, counts_);
    }

    std::vector<unsigned int> &counts() { return counts_; }
    boost::python::list list_counts() {
        return listify<unsigned int>(counts_);
    };

private:
    std::vector<Query> queries_;
    std::vector<unsigned int> counts_;
};



class PythonTileLayout : public TileLayout {
public:
    PythonTileLayout(const TileLayout &other)
        : TileLayout(other)
    {}

    boost::python::list widthsAsList() const {
        return listify<unsigned int>(widthsOfColumns_);
    }

    boost::python::list heightsAsList() const {
        return listify<unsigned int>(heightsOfRows_);
    }
};

class PythonTiledVideo {
public:
    PythonTiledVideo(const std::string &metadataIdentifier,
            const std::string &resourcesPath,
            unsigned int gopLength)
        : gopLength_(gopLength) {
        auto tiledEntry = std::make_shared<TiledEntry>("", resourcesPath, metadataIdentifier);
        auto tiledVideoManager = std::make_shared<TiledVideoManager>(tiledEntry);
        tileLocationProvider_ = std::make_shared<SingleTileLocationProvider>(tiledVideoManager);
    }

    std::shared_ptr<TileLayoutProvider> tileLayoutProvider() const { return tileLocationProvider_; }
    unsigned int gopLength() const { return gopLength_; }
    std::shared_ptr<PythonTileLayout> tileLayoutForFrame(unsigned int frame) {
        return std::make_shared<PythonTileLayout>(*tileLocationProvider_->tileLayoutForFrame(frame));
    }

private:
    std::shared_ptr<SingleTileLocationProvider> tileLocationProvider_;
    unsigned int gopLength_;
};

class PythonWorkloadCostEstimator {
public:
    static CostElements estimateCostForWorkload(PythonWorkload &workload, PythonTiledVideo &video, TASM &tasm) {
        auto workloadCostEstimator = WorkloadCostEstimator(video.tileLayoutProvider(), workload.toWorkload(tasm), video.gopLength());
        return workloadCostEstimator.estimateCostForWorkload();
    }
};
} // namespace tasm::python

#endif //TASM_WORKLOADWRAPPERS_H
