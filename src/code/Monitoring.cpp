#include "../headers/Monitoring.h"
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <chrono>
#include <thread>

using namespace prometheus;

void PrometheusServer() {
    Exposer exposer{"127.0.0.1:8080"};
    auto registry = std::make_shared<Registry>();

    auto& counter = BuildCounter()
        .Name("Test")
        .Register(*registry);

    exposer.RegisterCollectable(registry);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(120));
    }
}