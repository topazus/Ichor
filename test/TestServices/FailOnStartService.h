#pragma once

#include <ichor/dependency_management/Service.h>
#include "UselessService.h"

namespace Ichor {
    struct IFailOnStartService {
        [[nodiscard]] virtual uint64_t getStartCount() const noexcept = 0;
    protected:
        ~IFailOnStartService() = default;
    };

    struct FailOnStartService final : public IFailOnStartService, public Service<FailOnStartService> {
        FailOnStartService() = default;
        ~FailOnStartService() final = default;

        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
            startCount++;
            co_return tl::unexpected(StartError::FAILED);
        }

        AsyncGenerator<void> stop() final {
            co_return;
        }

        [[nodiscard]] uint64_t getStartCount() const noexcept final {
            return startCount;
        }

        uint64_t startCount{};
    };

    struct FailOnStartWithDependenciesService final : public IFailOnStartService, public Service<FailOnStartWithDependenciesService> {
        FailOnStartWithDependenciesService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
            reg.registerDependency<IUselessService>(this, true);
        }
        ~FailOnStartWithDependenciesService() final = default;

        AsyncGenerator<tl::expected<void, Ichor::StartError>> start() final {
            startCount++;
            co_return tl::unexpected(StartError::FAILED);
        }

        AsyncGenerator<void> stop() final {
            co_return;
        }

        void addDependencyInstance(IUselessService *, IService *) {
            svcCount++;
        }

        void removeDependencyInstance(IUselessService *, IService *) {
            svcCount--;
        }

        [[nodiscard]] uint64_t getStartCount() const noexcept final {
            return startCount;
        }

        uint64_t svcCount{};
        uint64_t startCount{};
    };
}