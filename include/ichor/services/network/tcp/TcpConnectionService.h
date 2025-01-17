#pragma once

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32) && !defined(__APPLE__)) || defined(__CYGWIN__)

#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/logging/Logger.h>
#include <ichor/services/timer/ITimerFactory.h>

namespace Ichor {
    class TcpConnectionService final : public IConnectionService, public AdvancedService<TcpConnectionService> {
    public:
        TcpConnectionService(DependencyRegister &reg, Properties props);
        ~TcpConnectionService() final = default;

        tl::expected<uint64_t, SendErrorReason> sendAsync(std::vector<uint8_t>&& msg) final;
        void setPriority(uint64_t priority) final;
        uint64_t getPriority() final;

    private:
        Task<tl::expected<void, Ichor::StartError>> start() final;
        Task<void> stop() final;

        void addDependencyInstance(ILogger &logger, IService &isvc);
        void removeDependencyInstance(ILogger &logger, IService &isvc);

        void addDependencyInstance(ITimerFactory &logger, IService &isvc);
        void removeDependencyInstance(ITimerFactory &logger, IService &isvc);

        friend DependencyRegister;

        int _socket;
        int _attempts;
        uint64_t _priority;
        uint64_t _msgIdCounter;
        bool _quit;
        ILogger *_logger{nullptr};
        ITimerFactory *_timerFactory{nullptr};
    };
}

#endif
