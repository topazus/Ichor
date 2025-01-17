#pragma once

#include <ichor/services/logging/Logger.h>
#include <ichor/dependency_management/AdvancedService.h>
#include <ichor/event_queues/IEventQueue.h>
#include "OptionalService.h"

using namespace Ichor;

class TestService final : public AdvancedService<TestService> {
public:
    TestService(DependencyRegister &reg, Properties props) : AdvancedService(std::move(props)) {
        reg.registerDependency<ILogger>(this, true);
        reg.registerDependency<IOptionalService>(this, false);
    }
    ~TestService() final = default;

private:
    Task<tl::expected<void, Ichor::StartError>> start() final {
        ICHOR_LOG_INFO(_logger, "TestService started with dependency");
        _started = true;
        if(_injectionCount == 2) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
        co_return {};
    }

    Task<void> stop() final {
        ICHOR_LOG_INFO(_logger, "TestService stopped with dependency");
        co_return;
    }

    void addDependencyInstance(ILogger &logger, IService &isvc) {
        _logger = &logger;

        ICHOR_LOG_INFO(_logger, "Inserted logger svcid {} for svcid {}", isvc.getServiceId(), getServiceId());
    }

    void removeDependencyInstance(ILogger&, IService&) {
        _logger = nullptr;
    }

    void addDependencyInstance(IOptionalService&, IService &isvc) {
        ICHOR_LOG_INFO(_logger, "Inserted IOptionalService svcid {}", isvc.getServiceId());

        _injectionCount++;
        if(_started && _injectionCount == 2) {
            GetThreadLocalEventQueue().pushEvent<QuitEvent>(getServiceId());
        }
    }

    void removeDependencyInstance(IOptionalService&, IService&) {
    }

    friend DependencyRegister;

    ILogger *_logger{nullptr};
    bool _started{false};
    int _injectionCount{0};
};
