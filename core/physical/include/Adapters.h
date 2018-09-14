#ifndef LIGHTDB_ADAPTERS_H
#define LIGHTDB_ADAPTERS_H

#include "LightField.h"
#include "PhysicalOperators.h"
#include "MaterializedLightField.h"
#include <queue>
#include <mutex>

namespace lightdb::physical {

class PhysicalToLogicalLightFieldAdapter: public LightField {
public:
    explicit PhysicalToLogicalLightFieldAdapter(const PhysicalLightFieldReference &physical)
            : LightField({},
                         physical->logical()->volume(),
                         physical->logical()->colorSpace()),
              physical_(physical)
    { }

    PhysicalToLogicalLightFieldAdapter(PhysicalToLogicalLightFieldAdapter &) = default;
    PhysicalToLogicalLightFieldAdapter(const PhysicalToLogicalLightFieldAdapter &) = default;
    PhysicalToLogicalLightFieldAdapter(PhysicalToLogicalLightFieldAdapter &&) = default;

    ~PhysicalToLogicalLightFieldAdapter() override = default;

    void accept(LightFieldVisitor& visitor) override { visitor.visit(*this); }

    PhysicalLightFieldReference source() { return physical_; }

private:
    PhysicalLightFieldReference physical_;
};

//using StreamingLightField = PhysicalToLogicalLightFieldAdapter;

/*
 * Converts a GPU-based physical light field into one that implements GPUOperator.
 * This is necessary while there are separate hierarchies for CPU and GPU operators,
 * which is sad and should be fixed.
 */
class GPUOperatorAdapter: public GPUOperator {
public:
    explicit GPUOperatorAdapter(const PhysicalLightFieldReference &source)
            : GPUOperatorAdapter(source, FindGPUOperatorAncestor(source))
    { }

    GPUOperatorAdapter(GPUOperatorAdapter &) = default;
    GPUOperatorAdapter(const GPUOperatorAdapter &) = default;
    GPUOperatorAdapter(GPUOperatorAdapter &&) = default;

    ~GPUOperatorAdapter() override = default;

    std::optional<physical::MaterializedLightFieldReference> read() override {
        return source_->read();
    }

private:
    GPUOperatorAdapter(const PhysicalLightFieldReference &source, GPUOperator &op)
            : GPUOperator(source->logical(), {},
                          op.gpu(),
                          [&op]() { return op.configuration(); }),
              source_(source)
    { }

    static GPUOperator& FindGPUOperatorAncestor(PhysicalLightFieldReference physical) {
        // Find ancestor GPUOperator instance given other possible intermediating GPU-based ancestors
        if(physical.is<GPUOperator>())
            return physical.downcast<GPUOperator>();
        else if(physical->device() == DeviceType::GPU &&
                physical->parents().size() == 1)
            return FindGPUOperatorAncestor(physical->parents()[0]);
        else
            throw InvalidArgumentError("Could not find GPUOperator ancestor", "physical");
    }

    PhysicalLightFieldReference source_;
};

class TeedPhysicalLightFieldAdapter {
public:
static std::shared_ptr<TeedPhysicalLightFieldAdapter> make(const PhysicalLightFieldReference &source,
                                                           const size_t size) {
    return std::make_shared<TeedPhysicalLightFieldAdapter>(source, size);
}

TeedPhysicalLightFieldAdapter(const PhysicalLightFieldReference &source,
                              const size_t size)
        : mutex_(std::make_shared<std::mutex>()),
          source_(source),
          queues_{size} {
    for(auto index = 0u; index < size; index++)
        tees_.emplace_back(
                LightFieldReference::make<PhysicalToLogicalLightFieldAdapter>(
                        PhysicalLightFieldReference::make<TeedPhysicalLightField>(*this, index)));
    }

    TeedPhysicalLightFieldAdapter(const TeedPhysicalLightFieldAdapter&) = delete;
    TeedPhysicalLightFieldAdapter(TeedPhysicalLightFieldAdapter&& other) = default;

    size_t size() const {
        return tees_.size();
    }

    LightFieldReference operator[] (const size_t index) {
        return tees_.at(index);
    }

private:
    class TeedPhysicalLightField: public PhysicalLightField {
    public:
        TeedPhysicalLightField(TeedPhysicalLightFieldAdapter &adapter,
                               const size_t index)
                : PhysicalLightField(adapter.source_->logical(),
                                     {adapter.source_},
                                     adapter.source_->device()),
                  adapter_(adapter),
                  index_(index)
        { }

        TeedPhysicalLightField(const TeedPhysicalLightField&) = delete;
        TeedPhysicalLightField(TeedPhysicalLightField&&) = default;

        std::optional<physical::MaterializedLightFieldReference> read() override {
            return adapter_.read(index_);
        }

    private:
        TeedPhysicalLightFieldAdapter &adapter_;
        const size_t index_;
    };

    std::optional<physical::MaterializedLightFieldReference> read(const size_t index) {
        std::optional<physical::MaterializedLightFieldReference> value;

        std::lock_guard lock(*mutex_);

        auto &queue = queues_[index];

        if(queue.empty())
            // Teed stream's queue is empty, so read from base stream and broadcast to all queues
            if((value = source_->read()).has_value())
                std::for_each(queues_.begin(), queues_.end(), [&value](auto &q) { q.push(value.value()); });

        // Now return a value (if any) from this tee's queue
        if(!queue.empty()) {
            value = queue.front();
            queue.pop();
            return value;
        } else
            return {};
    }

    std::shared_ptr<std::mutex> mutex_;
    PhysicalLightFieldReference source_;
    std::vector<std::queue<MaterializedLightFieldReference>> queues_;
    std::vector<LightFieldReference> tees_;
};

}; //namespace lightdb::physical

#endif //LIGHTDB_ADAPTERS_H
