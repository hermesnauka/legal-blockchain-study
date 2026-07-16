#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "core/LedgerEvent.h"

namespace legalchain::core {

/// Minimal in-process publish/subscribe bus — the C++ analogue of Spring's
/// ApplicationEventPublisher/@EventListener. Listeners are copied out under
/// the lock before being invoked so a listener can safely subscribe/publish
/// without deadlocking itself.
class EventBus {
public:
    using Listener = std::function<void(const LedgerEvent&)>;

    void subscribe(Listener listener) {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.push_back(std::move(listener));
    }

    void publish(const LedgerEvent& event) {
        std::vector<Listener> copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            copy = listeners_;
        }
        for (auto& listener : copy) {
            listener(event);
        }
    }

private:
    std::mutex mutex_;
    std::vector<Listener> listeners_;
};

} // namespace legalchain::core
