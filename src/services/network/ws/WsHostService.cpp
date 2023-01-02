#ifdef ICHOR_USE_BOOST_BEAST

#include <ichor/DependencyManager.h>
#include <ichor/services/network/IConnectionService.h>
#include <ichor/services/network/ws/WsHostService.h>
#include <ichor/services/network/ws/WsConnectionService.h>
#include <ichor/services/network/ws/WsEvents.h>
#include <ichor/services/network/NetworkEvents.h>
#include <ichor/services/network/http/HttpScopeGuards.h>

Ichor::WsHostService::WsHostService(DependencyRegister &reg, Properties props, DependencyManager *mng) : Service(std::move(props), mng) {
    reg.registerDependency<ILogger>(this, true);
    reg.registerDependency<IAsioContextService>(this, true);
}

Ichor::AsyncGenerator<tl::expected<void, Ichor::StartError>> Ichor::WsHostService::start() {
    if(getProperties().contains("Priority")) {
        _priority = Ichor::any_cast<uint64_t>(getProperties()["Priority"]);
    }

    if(!getProperties().contains("Port") || !getProperties().contains("Address")) {
        ICHOR_LOG_ERROR(_logger, "Missing port or address when starting WsHostService");
        co_return tl::unexpected(StartError::FAILED);
    }

    if(getProperties().contains("NoDelay")) {
        _tcpNoDelay = Ichor::any_cast<bool>(getProperties()["NoDelay"]);
    }

    _eventRegistration = getManager().registerEventHandler<NewWsConnectionEvent>(this, getServiceId());

    auto address = net::ip::make_address(Ichor::any_cast<std::string&>(getProperties()["Address"]));
    auto port = Ichor::any_cast<uint16_t>(getProperties()["Port"]);
    _strand = std::make_unique<net::strand<net::io_context::executor_type>>(_asioContextService->getContext()->get_executor());

    net::spawn(*_strand, [this, address = std::move(address), port](net::yield_context yield) {
        ScopeGuardAtomicCount const guard{_finishedListenAndRead};
        try {
            listen(tcp::endpoint{address, port}, std::move(yield));
        } catch (std::runtime_error &e) {
            ICHOR_LOG_ERROR(_logger, "caught std runtime_error {}", e.what());
            getManager().pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        } catch (...) {
            ICHOR_LOG_ERROR(_logger, "caught unknown error");
            getManager().pushEvent<StopServiceEvent>(getServiceId(), getServiceId());
        }
    }ASIO_SPAWN_COMPLETION_TOKEN);

    co_return {};
}

Ichor::AsyncGenerator<void> Ichor::WsHostService::stop() {
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! trying to stop WsHostService {}", getServiceId());
    _quit = true;

    for(auto conn : _connections) {
        getManager().pushEvent<StopServiceEvent>(getServiceId(), conn->getServiceId());
    }

    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! acceptor {}", getServiceId());
    net::spawn(*_strand, [this](net::yield_context yield) {
        ScopeGuardAtomicCount const guard{_finishedListenAndRead};
//        fmt::print("----------------------------------------------- {}:{} ws acceptor close\n", getServiceId(), getServiceName());
        _wsAcceptor->close();
//        fmt::print("----------------------------------------------- {}:{} ws acceptor done\n", getServiceId(), getServiceName());
    }ASIO_SPAWN_COMPLETION_TOKEN);

    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! pre-await {} {}", getServiceId(), _startStopEvent.is_set());
    co_await _startStopEvent;

    while(_finishedListenAndRead.load(std::memory_order_acquire) != 0) {
        std::this_thread::sleep_for(1ms);
    }

    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! stopped WsHostService {}", getServiceId());
    _strand = nullptr;
    _wsAcceptor = nullptr;

    co_return;
}

void Ichor::WsHostService::addDependencyInstance(ILogger *logger, IService *) {
    _logger = logger;
}

void Ichor::WsHostService::removeDependencyInstance(ILogger *logger, IService *) {
    _logger = nullptr;
}

void Ichor::WsHostService::addDependencyInstance(IAsioContextService *AsioContextService, IService *) {
    _asioContextService = AsioContextService;
}

void Ichor::WsHostService::removeDependencyInstance(IAsioContextService *logger, IService *) {
    _asioContextService = nullptr;
}

Ichor::AsyncGenerator<Ichor::IchorBehaviour> Ichor::WsHostService::handleEvent(Ichor::NewWsConnectionEvent const &evt) {
    if(_quit.load(std::memory_order_acquire)) {
        co_return {};
    }

    auto connection = getManager().createServiceManager<WsConnectionService, IConnectionService>(Properties{
        {"WsHostServiceId", Ichor::make_any<uint64_t>(getServiceId())},
        {"Socket", Ichor::make_any<decltype(evt._socket)>(evt._socket)},
        {"Filter", Ichor::make_any<Filter>(Filter{ClientConnectionFilter{}})}
    });
    _connections.push_back(connection);

    co_return {};
}

void Ichor::WsHostService::setPriority(uint64_t priority) {
    _priority = priority;
}

uint64_t Ichor::WsHostService::getPriority() {
    return _priority;
}

void Ichor::WsHostService::fail(beast::error_code ec, const char *what) {
    ICHOR_LOG_ERROR(_logger, "Boost.BEAST fail: {}, {}", what, ec.message());
    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! push {}", getServiceId());
    getManager().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), 1000, [this](DependencyManager &dm) {
        INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! _startStopEvent set {}", getServiceId());
        _startStopEvent.set();
    });
    getManager().pushPrioritisedEvent<StopServiceEvent>(getServiceId(), _priority, getServiceId());
}

void Ichor::WsHostService::listen(tcp::endpoint endpoint, net::yield_context yield)
{
    beast::error_code ec;

    _wsAcceptor = std::make_unique<tcp::acceptor>(*_asioContextService->getContext());
    _wsAcceptor->open(endpoint.protocol(), ec);
    if(ec) {
        return fail(ec, "open");
    }

    _wsAcceptor->set_option(net::socket_base::reuse_address(true), ec);
    if(ec) {
        return fail(ec, "set_option");
    }

    _wsAcceptor->bind(endpoint, ec);
    if(ec) {
        return fail(ec, "bind");
    }

    _wsAcceptor->listen(net::socket_base::max_listen_connections, ec);
    if(ec) {
        return fail(ec, "listen");
    }

    while(!_quit && !_asioContextService->fibersShouldStop())
    {
        tcp::socket socket(*_asioContextService->getContext());

        // tcp accept new connections
        _wsAcceptor->async_accept(socket, yield[ec]);
        if(ec) {
            return fail(ec, "accept");
        }
        if(_quit.load(std::memory_order_acquire)) {
            break;
        }

        socket.set_option(tcp::no_delay(_tcpNoDelay));

        getManager().pushPrioritisedEvent<NewWsConnectionEvent>(getServiceId(), _priority, std::make_shared<websocket::stream<beast::tcp_stream>>(std::move(socket)));
    }

    INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! push2 {}", getServiceId());
    getManager().pushPrioritisedEvent<RunFunctionEvent>(getServiceId(), 1000, [this](DependencyManager &dm) {
        INTERNAL_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! _startStopEvent set2 {}", getServiceId());
        _startStopEvent.set();
    });
}

#endif
