#pragma once

#include <ichor/dependency_management/Service.h>
#include <ichor/events/Event.h>

using namespace Ichor;

struct IEventHandlerService {
    virtual std::unordered_map<uint64_t, uint64_t>& getHandledEvents() = 0;

protected:
    ~IEventHandlerService() = default;
};

template <Derived<Event> EventT>
struct EventHandlerService final : public IEventHandlerService, public Service<EventHandlerService<EventT>> {
    EventHandlerService() = default;

    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        _handler = this->getManager().template registerEventHandler<EventT>(this);

        co_return {};
    }

    AsyncGenerator<void> stop() final {
        _handler.reset();

        co_return;
    }

    AsyncGenerator<IchorBehaviour> handleEvent(EventT const &evt) {
        auto counter = handledEvents.find(evt.type);

        if(counter == end(handledEvents)) {
            handledEvents.template emplace<>(evt.type, 1);
        } else {
            counter->second++;
        }

        co_return {};
    }

    std::unordered_map<uint64_t, uint64_t>& getHandledEvents() final {
        return handledEvents;
    }

    EventHandlerRegistration _handler{};
    std::unordered_map<uint64_t, uint64_t> handledEvents;
};
