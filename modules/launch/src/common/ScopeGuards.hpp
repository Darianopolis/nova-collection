#pragma once
#ifndef SCOPE_GUARDS_H
#define SCOPE_GUARDS_H

template<typename Fn>
class OnScopeExit
{
    Fn fn;
public:
    OnScopeExit(Fn fn)
        : fn(std::move(fn))
    {}

    ~OnScopeExit()
    {
        fn();
    }
};

template<typename Fn>
class OnScopeSuccess
{
    Fn fn;
    int exceptions;
public:
    OnScopeSuccess(Fn fn)
        : fn(std::move(fn))
        , exceptions(std::uncaught_exceptions())
    {}

    ~OnScopeSuccess()
    {
        if (std::uncaught_exceptions() <= exceptions)
            fn();
    }
};

template<typename Fn>
class OnScopeFailure
{
    Fn fn;
    int exceptions;
public:
    OnScopeFailure(Fn fn)
        : fn(std::move(fn))
        , exceptions(std::uncaught_exceptions())
    {}

    ~OnScopeFailure()
    {
        if (std::uncaught_exceptions() > exceptions)
            fn();
    }
};

#define COMMON_CONCAT2(a, b) a##b
#define COMMON_CONCAT(a, b) COMMON_CONCAT2(a, b)
#define ON_SCOPE_EXIT(...)    OnScopeExit    COMMON_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]
#define ON_SCOPE_SUCCESS(...) OnScopeSuccess COMMON_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]
#define ON_SCOPE_FAILURE(...) OnScopeFailure COMMON_CONCAT(_scope_guard_$_, __COUNTER__) = [__VA_ARGS__]

#endif // !SCOPE_GUARDS_H