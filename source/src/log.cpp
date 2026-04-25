// ---------------------------------------------------------------------------
// Mordor logging backend implementation (quill)
//
// This is the ONLY translation unit that includes quill backend headers.
// See include/mordor/log.hpp for the engine-facing API contract.
// ---------------------------------------------------------------------------

#include "mordor/log.hpp"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

quill::Logger* g_logger = nullptr;

} // namespace

namespace mordor::log {

void init(const char* log_file)
{
    assert(g_logger == nullptr && "mordor::log::init called more than once");

    quill::BackendOptions backend_opts;
    quill::Backend::start(backend_opts);

    std::vector<std::shared_ptr<quill::Sink>> sinks;

    auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
    sinks.push_back(std::move(console_sink));

    if (log_file != nullptr)
    {
        quill::FileSinkConfig file_cfg;
        file_cfg.set_open_mode('w');
        auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(log_file, file_cfg);
        sinks.push_back(std::move(file_sink));
    }

    g_logger = quill::Frontend::create_or_get_logger("mordor", std::move(sinks));
    g_logger->set_log_level(quill::LogLevel::Debug);
}

void flush()
{
    if (g_logger != nullptr)
    {
        // flush_log() blocks until the backend has written all pending records.
        g_logger->flush_log();
    }
}

void shutdown()
{
    flush();
    quill::Backend::stop();
    g_logger = nullptr;
}

namespace detail {

quill::Logger* engine_logger()
{
    assert(g_logger != nullptr && "mordor::log::init must be called before logging");
    return g_logger;
}

} // namespace detail

} // namespace mordor::log
