#pragma once

namespace Ichor {
    namespace Detail {
        extern unordered_set<uint64_t> emptyDependencies;
    }

    /// This lifecycle manager is created when the underlying service requests 0 dependencies
    /// It contains optimizations for dealing with dependencies.
    /// \tparam ServiceType
    /// \tparam IFaces
    template<class ServiceType, typename... IFaces>
#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
    requires DerivedTemplated<ServiceType, Service>
#endif
    class LifecycleManager final : public ILifecycleManager {
    public:
        template <typename U = ServiceType> requires RequestsProperties<U>
        explicit LifecycleManager(std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _interfaces(std::move(interfaces)), _service(std::forward<Properties>(properties), mng) {
        }

        template <typename U = ServiceType> requires (!RequestsProperties<U>)
        explicit LifecycleManager(std::vector<Dependency> interfaces, Properties&& properties, DependencyManager *mng) : _interfaces(std::move(interfaces)), _service() {
            _service.setProperties(std::forward<Properties>(properties));
        }

        ~LifecycleManager() final = default;

        template<typename... Interfaces>
        [[nodiscard]]
        static std::unique_ptr<LifecycleManager<ServiceType, Interfaces...>> create(Properties&& properties, DependencyManager *mng, InterfacesList_t<Interfaces...>) {
            std::vector<Dependency> interfaces{};
            interfaces.reserve(sizeof...(Interfaces));
            (interfaces.emplace_back(typeNameHash<Interfaces>(), false, false),...);
            return std::make_unique<LifecycleManager<ServiceType, Interfaces...>>(std::move(interfaces), std::forward<Properties>(properties), mng);
        }


        std::vector<decltype(std::declval<DependencyInfo>().begin())> interestedInDependency(ILifecycleManager *dependentService, bool online) noexcept final {
            return {};
        }

        AsyncGenerator<StartBehaviour> dependencyOnline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) final {
            // this function should never be called
            std::terminate();
            co_return StartBehaviour::DONE;
        }

        AsyncGenerator<StartBehaviour> dependencyOffline(ILifecycleManager* dependentService, std::vector<decltype(std::declval<DependencyInfo>().begin())> iterators) final {
            // this function should never be called
            std::terminate();
            co_return StartBehaviour::DONE;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependencies() noexcept final {
            return Detail::emptyDependencies;
        }

        [[nodiscard]]
        unordered_set<uint64_t> &getDependees() noexcept final {
            return _serviceIdsOfDependees;
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> start() final {
            return _service.internal_start(nullptr);
        }

        [[nodiscard]]
        AsyncGenerator<StartBehaviour> stop() final {
            return _service.internal_stop();
        }

        [[nodiscard]]
        bool setInjected() final {
            return _service.internalSetInjected();
        }

        [[nodiscard]]
        bool setUninjected() final {
            return _service.internalSetUninjected();
        }

        [[nodiscard]] std::string_view implementationName() const noexcept final {
            return _service.getServiceName();
        }

        [[nodiscard]] uint64_t type() const noexcept final {
            return typeNameHash<ServiceType>();
        }

        [[nodiscard]] uint64_t serviceId() const noexcept final {
            return _service.getServiceId();
        }

        [[nodiscard]] ServiceType& getService() noexcept {
            return _service;
        }

        [[nodiscard]] uint64_t getPriority() const noexcept final {
            return _service.getServicePriority();
        }

        [[nodiscard]] ServiceState getServiceState() const noexcept final {
            return _service.getState();
        }

        [[nodiscard]] IService const * getIService() const noexcept final {
            return static_cast<IService const *>(&_service);
        }

        [[nodiscard]] const std::vector<Dependency>& getInterfaces() const noexcept final {
            return _interfaces;
        }

        [[nodiscard]] Properties const & getProperties() const noexcept final {
            return _service._properties;
        }

        [[nodiscard]] DependencyRegister const * getDependencyRegistry() const noexcept final {
            return nullptr;
        }

        /// Someone is interested in us, inject ourself into them
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void insertSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                _serviceIdsOfDependees.insert(serviceIdOfOther);
            }
        }

        /// See insertSelfInto
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        template <int i, typename Iface1, typename... otherIfaces>
        void insertSelfInto2(uint64_t keyOfInterfaceToInject, std::function<void(void*, IService*)> &fn) {
            if(typeNameHash<Iface1>() == keyOfInterfaceToInject) {
                fn(static_cast<Iface1*>(&_service), static_cast<IService*>(&_service));
            } else {
                if constexpr(i > 1) {
                    insertSelfInto2<sizeof...(otherIfaces), otherIfaces...>(keyOfInterfaceToInject, fn);
                }
            }
        }

        /// The underlying service got stopped and someone else is asking us to remove ourselves from them.
        /// \param keyOfInterfaceToInject
        /// \param serviceIdOfOther
        /// \param fn
        void removeSelfInto(uint64_t keyOfInterfaceToInject, uint64_t serviceIdOfOther, std::function<void(void*, IService*)> &fn) final {
            INTERNAL_DEBUG("removeSelfInto2() svc {} removing svc {}", serviceId(), serviceIdOfOther);
            if constexpr (sizeof...(IFaces) > 0) {
                insertSelfInto2<sizeof...(IFaces), IFaces...>(keyOfInterfaceToInject, fn);
                // This is erased by the dependency manager
                _serviceIdsOfDependees.erase(serviceIdOfOther);
            }
        }

    private:
        std::vector<Dependency> _interfaces;
        ServiceType _service;
        unordered_set<uint64_t> _serviceIdsOfDependees; // services that depend on this service
    };
}