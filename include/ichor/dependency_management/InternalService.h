#pragma once

#include <ichor/dependency_management/IService.h>

namespace Ichor::Detail {

    class InternalService : public IService {
    public:
        InternalService() noexcept : IService(), _properties(), _serviceId(Detail::_serviceIdCounter.fetch_add(1, std::memory_order_relaxed)), _servicePriority(INTERNAL_EVENT_PRIORITY), _serviceGid(sole::uuid4()), _serviceState(ServiceState::INSTALLED) {
        }

        ~InternalService() noexcept override {
            _serviceId = 0;
            _serviceState = ServiceState::UNINSTALLED;
        }

        InternalService(const InternalService&) = default;
        InternalService(InternalService&&) noexcept = default;
        InternalService& operator=(const InternalService&) = default;
        InternalService& operator=(InternalService&&) noexcept = default;

        /// Process-local unique service id
        /// \return id
        [[nodiscard]] uint64_t getServiceId() const noexcept final {
            return _serviceId;
        }

        /// Global unique service id
        /// \return gid
        [[nodiscard]] sole::uuid getServiceGid() const noexcept final {
            return _serviceGid;
        }

        /// Name of the user-specified service (e.g. CoutFrameworkLogger)
        /// \return
        [[nodiscard]] std::string_view getServiceName() const noexcept final {
            return typeName<IEventQueue>();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _serviceState;
        }

        [[nodiscard]] uint64_t getServicePriority() const noexcept final {
            return _servicePriority;
        }

        void setServicePriority(uint64_t priority) noexcept final {
            _servicePriority = priority;
        }

        [[nodiscard]] bool isStarted() const noexcept final {
            return _serviceState == ServiceState::ACTIVE;
        }

        [[nodiscard]] Properties& getProperties() noexcept final {
            return _properties;
        }

        [[nodiscard]] const Properties& getProperties() const noexcept final {
            return _properties;
        }

    protected:
        Properties _properties;
    private:
        ///
        /// \return true if started
        [[nodiscard]] Task<StartBehaviour> internal_start(DependencyInfo *_dependencies) {
            if(_serviceState != ServiceState::INSTALLED || (_dependencies != nullptr && !_dependencies->allSatisfied())) {
                INTERNAL_DEBUG("internal_start service {}:{} state {} dependencies {} {}", getServiceId(), typeName<IEventQueue>(), getState(), _dependencies->size(), _dependencies->allSatisfied());
                co_return {};
            }

            INTERNAL_DEBUG("internal_start service {}:{} state {} -> {}", getServiceId(), typeName<IEventQueue>(), getState(), ServiceState::INJECTING);
            _serviceState = ServiceState::INJECTING;

            co_return {};
        }
        ///
        /// \return true if stopped or already stopped
        [[nodiscard]] Task<StartBehaviour> internal_stop() {
#ifdef ICHOR_USE_HARDENING
            if(_serviceState != ServiceState::UNINJECTING) [[unlikely]] {
                std::terminate();
            }
#endif

            INTERNAL_DEBUG("internal_stop service {}:{} state {} -> {}", getServiceId(), typeName<IEventQueue>(), getState(), ServiceState::INSTALLED);
            _serviceState = ServiceState::INSTALLED;

            co_return {};
        }

        [[nodiscard]] bool internalSetInjected() {
            if(_serviceState != ServiceState::INJECTING) {
                return false;
            }

            INTERNAL_DEBUG("internalSetInjected service {}:{} state {} -> {}", getServiceId(), typeName<IEventQueue>(), getState(), ServiceState::ACTIVE);
            _serviceState = ServiceState::ACTIVE;
            return true;
        }

        [[nodiscard]] bool internalSetUninjected() {
            if(_serviceState != ServiceState::ACTIVE) {
                return false;
            }

            INTERNAL_DEBUG("internalSetUninjected service {}:{} state {} -> {}", getServiceId(), typeName<IEventQueue>(), getState(), ServiceState::UNINJECTING);
            _serviceState = ServiceState::UNINJECTING;
            return true;
        }

        [[nodiscard]] ServiceState getState() const noexcept {
            return _serviceState;
        }

        uint64_t _serviceId;
        uint64_t _servicePriority;
        sole::uuid _serviceGid;
        ServiceState _serviceState;
    };
}
