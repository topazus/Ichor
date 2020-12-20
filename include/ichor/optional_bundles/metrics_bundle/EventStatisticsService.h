#pragma once

#include <unordered_map>
#include <ichor/Service.h>
#include <ichor/optional_bundles/logging_bundle/Logger.h>
#include <ichor/optional_bundles/timer_bundle/TimerService.h>
#include <chrono>

namespace Ichor {

    struct StatisticEntry {
        StatisticEntry() = default;
        StatisticEntry(int64_t _timestamp, int64_t _processingTimeRequired) : timestamp(_timestamp), processingTimeRequired(_processingTimeRequired) {}
        int64_t timestamp{};
        int64_t processingTimeRequired{};
    };

    struct AveragedStatisticEntry {
        AveragedStatisticEntry() = default;
        AveragedStatisticEntry(int64_t _timestamp, int64_t _minProcessingTimeRequired, int64_t _maxProcessingTimeRequired, int64_t _avgProcessingTimeRequired, uint64_t _occurances) :
            timestamp(_timestamp), minProcessingTimeRequired(_minProcessingTimeRequired), maxProcessingTimeRequired(_maxProcessingTimeRequired), avgProcessingTimeRequired(_avgProcessingTimeRequired),
            occurances(_occurances) {}
        int64_t timestamp{};
        int64_t minProcessingTimeRequired{};
        int64_t maxProcessingTimeRequired{};
        int64_t avgProcessingTimeRequired{};
        uint64_t occurances{};
    };

    class IEventStatisticsService : public virtual IService {
    public:
        ~IEventStatisticsService() override = default;

        virtual const std::unordered_map<std::string_view, std::vector<StatisticEntry>>& getRecentStatistics() = 0;
        virtual const std::unordered_map<std::string_view, std::vector<AveragedStatisticEntry>>& getAverageStatistics() = 0;
    };

    class EventStatisticsService final : public IEventStatisticsService, public Service {
    public:
        ~EventStatisticsService() final = default;

        bool preInterceptEvent(Event const * const evt);
        bool postInterceptEvent(Event const * const evt, bool processed);

        Generator<bool> handleEvent(TimerEvent const * const evt);

        bool start() final;
        bool stop() final;

        const std::unordered_map<std::string_view, std::vector<StatisticEntry>>& getRecentStatistics() override;
        const std::unordered_map<std::string_view, std::vector<AveragedStatisticEntry>>& getAverageStatistics() override;
    private:
        std::unordered_map<std::string_view, std::vector<StatisticEntry>> _recentEventStatistics{};
        std::unordered_map<std::string_view, std::vector<AveragedStatisticEntry>> _averagedStatistics{};
        std::chrono::time_point<std::chrono::steady_clock> _startProcessingTimestamp{};
        bool _showStatisticsOnStop{false};
        uint64_t _averagingIntervalMs{5000};
        std::unique_ptr<EventHandlerRegistration> _timerEventRegistration{nullptr};
        std::unique_ptr<EventInterceptorRegistration> _interceptorRegistration{nullptr};
    };
}