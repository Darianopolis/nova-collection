#pragma once

#include <format>
#include <syncstream>
#include <iostream>

template<class... Args>
void log(const std::format_string<Args...> fmt, Args&&... args)
{
    std::osyncstream os(std::cout);
    os << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

template<class... Args>
void log_info(const std::format_string<Args...> fmt, Args&&... args)
{
    std::osyncstream os(std::cout);
    os << "[\u001B[94mINFO\u001B[0m] " << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

template<class... Args>
void log_debug(const std::format_string<Args...> fmt, Args&&... args)
{
    std::osyncstream os(std::cout);
    os << "[\u001B[96mDEBUG\u001B[0m] " << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

template<class... Args>
void log_error(const std::format_string<Args...> fmt, Args&&... args)
{
    std::osyncstream os(std::cout);
    os << "[\u001B[91mERROR\u001B[0m] " << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

template<class... Args>
void log_warn(const std::format_string<Args...> fmt, Args&&... args)
{
    std::osyncstream os(std::cout);
    os << "[\u001B[93mWARN\u001B[0m] " << std::vformat(fmt.get(), std::make_format_args(args...)) << '\n';
}

inline
std::string duration_to_string(std::chrono::duration<double, std::nano> dur)
{
    double nanos = dur.count();

    if (nanos > 1e9) {
        double seconds = nanos / 1e9;
        uint32_t decimals = 2 - uint32_t(std::log10(seconds));
        return std::format("{:.{}f}s",seconds, decimals);
    }

    if (nanos > 1e6) {
        double millis = nanos / 1e6;
        uint32_t decimals = 2 - uint32_t(std::log10(millis));
        return std::format("{:.{}f}ms", millis, decimals);
    }

    if (nanos > 1e3) {
        double micros = nanos / 1e3;
        uint32_t decimals = 2 - uint32_t(std::log10(micros));
        return std::format("{:.{}f}us", micros, decimals);
    }

    if (nanos > 0) {
        uint32_t decimals = 2 - uint32_t(std::log10(nanos));
        return std::format("{:.{}f}ns", nanos, decimals);
    }

    return "0";
}