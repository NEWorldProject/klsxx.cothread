#include <boost/context/continuation.hpp>
#include <coroutine>
#include "kls/coroutine/Async.h"
#include "kls/coroutine/Traits.h"

using namespace boost::context;

using CoAwaitSuspendUser = bool (*)(std::coroutine_handle<> h, void *user);

struct Ctrl {
    void *user;
    CoAwaitSuspendUser suspend;
    continuation swap{};
    [[nodiscard]] constexpr bool await_ready() const noexcept { return false; } //NOLINT
    [[nodiscard]] bool await_suspend(std::coroutine_handle<> h) const noexcept { return suspend(h, user); }
    constexpr void await_resume() const noexcept {}
};

static thread_local Ctrl *ctrl{nullptr}; // THIS IS A HACK

using CallCCFnUser = void (*)(void *data);

kls::coroutine::ValueAsync<> run(CallCCFnUser user, void *data) {
    Ctrl local{};
    ctrl = &local;
    auto ctx = callcc([data, user](continuation &&c) {
        return (ctrl->swap = std::move(c), user(data), std::move(ctrl->swap));
    });
    for (; ctx;) (co_await local, ctx = ctx.resume());
}

void call_cc_trap(CoAwaitSuspendUser suspend, void *user) {
    ctrl->suspend = suspend, ctrl->user = user;
    ctrl->swap = ctrl->swap.resume();
}

class CoFun {
public:
    // this allocates itself on the stack as a frame
    class promise_type {
    public:
        auto get_return_object() {}
        [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; } //NOLINT
        [[nodiscard]] std::suspend_never final_suspend() const noexcept { return {}; } //NOLINT
        void return_void() {}
        void unhandled_exception() {}
    private:
    };

    // The reason why this exists is to follow the coroutine contract and pass the result
    struct await {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; } //NOLINT
        [[nodiscard]] constexpr bool await_suspend(std::coroutine_handle<> h) const noexcept { return false; } //NOLINT
        constexpr void await_resume() const noexcept {}
    };

    // when this coroutine returns, it will always be ready as it never actually suspends
    [[nodiscard]] constexpr bool await_ready() const noexcept { return true; } //NOLINT
    [[nodiscard]] constexpr bool await_suspend(std::coroutine_handle<> h) const noexcept { return false; } //NOLINT
    void await_resume() const noexcept {}

    decltype(auto) await_transform(CoFun &&expr) { return std::forward<CoFun>(expr); } // NOLINT

    template<class U>
    requires requires {
        kls::coroutine::has_co_await_operator_v<U>;
    }
    await await_transform(U &&expr) { return call_cc_suspend(expr.operator co_await()); }

    template<class U>
    requires requires {
        !kls::coroutine::has_co_await_operator_v<U>;
    }
    await await_transform(U &&expr) { return call_cc_suspend(std::forward<U>(expr)); }
private:
    std::exception_ptr m_exception;

    template<class U>
    requires requires(U &&u, std::coroutine_handle<> h) {
        { u.await_suspend() } -> std::same_as<void>;
    }
    void call_cc_suspend(U &expr) {
        auto fn = [](std::coroutine_handle<> h, void *u) { return (static_cast<U *>(u)->await_suspend(h), true); };
        call_cc_trap(fn, &expr);
    }

    template<class U>
    requires requires(U &&u, std::coroutine_handle<> h) {
        { u.await_suspend() } -> std::same_as<bool>;
    }
    void call_cc_suspend(U &expr) {
        auto fn = [](std::coroutine_handle<> h, void *u) { return static_cast<U *>(u)->await_suspend(h); };
        call_cc_trap(fn, &expr);
    }

    template<class U>
    await call_cc_await(U &&expr) {
        if (!expr.await_ready()) call_cc_suspend(expr);
        expr.await_resume();
        return await{};
    }
};

kls::coroutine::ValueAsync<> function() {
    co_return;
}

CoFun poc_called() {
    co_return;
}

CoFun poc() {
    co_await function();
    co_return;
}