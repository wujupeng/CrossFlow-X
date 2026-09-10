#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

struct TopologyChangeEvent {
    TopologyVersion oldVersion;
    TopologyVersion newVersion;
    NodeId changedBy;
};

class ITopologyChangeEventSource {
public:
    using TopologyChangeCallback = std::function<void(const TopologyChangeEvent&)>;

    virtual ~ITopologyChangeEventSource() = default;
    virtual void setCallback(TopologyChangeCallback cb) noexcept = 0;
    virtual void notifyTopologyChanged(const TopologyVersion& oldVersion,
                                       const TopologyVersion& newVersion,
                                       const NodeId& changedBy) noexcept = 0;
};

class TopologyChangeEventSource : public ITopologyChangeEventSource {
public:
    TopologyChangeEventSource() = default;
    ~TopologyChangeEventSource() = default;

    TopologyChangeEventSource(const TopologyChangeEventSource&) = delete;
    TopologyChangeEventSource& operator=(const TopologyChangeEventSource&) = delete;

    void setCallback(TopologyChangeCallback cb) noexcept override { callback_ = std::move(cb); }

    void notifyTopologyChanged(const TopologyVersion& oldVersion,
                               const TopologyVersion& newVersion,
                               const NodeId& changedBy) noexcept override {
        if (callback_) {
            callback_(TopologyChangeEvent{oldVersion, newVersion, changedBy});
        }
    }

private:
    TopologyChangeCallback callback_;
};

}  // namespace cfx