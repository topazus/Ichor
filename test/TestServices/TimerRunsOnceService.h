#pragma once

#include <ichor/DependencyManager.h>
#include <ichor/services/timer/TimerService.h>
#include <ichor/dependency_management/Service.h>
#include <ichor/dependency_management/ILifecycleManager.h>

using namespace Ichor;

class TimerRunsOnceService final : public Service<TimerRunsOnceService> {
public:
    TimerRunsOnceService() = default;
    ~TimerRunsOnceService() final = default;

    AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
        fmt::print("start\n");
        _timerManager = getManager().createServiceManager<Timer, ITimer>();
        _timerManager->setChronoInterval(std::chrono::milliseconds(5));
        _timerManager->setCallback(this, [this](DependencyManager &dm) {
            count++;
            _timerManager->stopTimer();
        });
        _timerManager->startTimer(true);
        co_return {};
    }

    AsyncGenerator<void> stop() final {
        _timerManager = nullptr;
        co_return;
    }

    uint64_t count{};
private:
    Timer* _timerManager{};

};
