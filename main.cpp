#include <iostream>
#include <coroutine>


struct task {
    struct promise {
        std::exception_ptr error{};
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        task get_return_object() { return {}; }
        void unhandled_exception() { error = std::current_exception(); }
    };
    using promise_type = promise;
    std::coroutine_handle<promise> handle_;
};

struct value_awaiter {
    int value;
    constexpr bool await_ready() { return true; }
    void await_suspend(auto) { }
    int await_resume() { return value; }
};

task use_task()
{
    std::cout << "value: " << co_await value_awaiter{17} << "\n";
}

// title: Implementing a C++ Coroutine Task from Scratch - Dietmar Kühl - ACCU 2023
// src: https://youtu.be/Npiw4cYElng
int main()
{
    use_task();
    return 0;
}
