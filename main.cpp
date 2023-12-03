#include <iostream>
#include <coroutine>
#include <unordered_map>
#include <functional>
#include <memory>

namespace io {
    struct is_awaiter_test {
        struct promise {
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            is_awaiter_test get_return_object() { return {}; }
            void unhandled_exception() { }
        };
        using promise_type = promise;
    };

    template<class Awaiter>
    concept is_awaiter = requires() {
        [](Awaiter awaiter) -> is_awaiter_test {
            co_await awaiter;
        };
    };

    template<class Value>
    struct value_awaiter {
        Value value_;
        constexpr bool await_ready() { return true; }
        void await_suspend(auto) { }
        Value await_resume() { return value_; }
    };

    struct task {
        struct promise {
            std::coroutine_handle<> continuation{std::noop_coroutine()};
            std::exception_ptr error{};

            struct final_awaiter : std::suspend_always {
                promise* promise_;

                std::coroutine_handle<> await_suspend(auto) noexcept
                {
                    return promise_->continuation;
                }
            };

            std::suspend_always initial_suspend() { return {}; }
            final_awaiter final_suspend() noexcept { return {{}, this}; }
            task get_return_object()
            {
                return task{unique_promise{this}};
            }
            void unhandled_exception() { error = std::current_exception(); }

            template<class Awaiter>
            requires is_awaiter<Awaiter>
            auto await_transform(Awaiter awaiter) { return awaiter; }

            template<class Value>
            requires (!is_awaiter<Value>)
            auto await_transform(Value value) { return value_awaiter<Value>{value}; }
        };

        using deleter = decltype([](promise* promise_ptr) {
            std::coroutine_handle<promise>::from_promise(*promise_ptr).destroy();
        });
        using unique_promise = std::unique_ptr<promise, deleter>;
        using promise_type = promise;

        unique_promise promise_;

        void start()
        {
            auto handle = std::coroutine_handle<promise_type>::from_promise(*promise_);
            handle.resume();
        }

        struct nested_awaiter {
            promise* promise_;

            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> continuation)
            {
                promise_->continuation = continuation;
                std::coroutine_handle<promise_type>::from_promise(*promise_).resume();
            }
            void await_resume() { }
        };
        nested_awaiter operator co_await() const { return {promise_.get()}; }
    };

    struct context {
        std::unordered_map<int, std::function<void(std::string)>> outstanding;

        void submit(int file_descriptor, auto func)
        {
            outstanding[file_descriptor] = func;
        }

        void complete(int file_descriptor, std::string value)
        {
            auto it = outstanding.find(file_descriptor);

            if (it!=outstanding.end()) {
                auto func = it->second;
                outstanding.erase(it);
                func(value);
            }
        }
    };

    struct async_reader {
        io::context& context;
        int file_descriptor;
        std::string value{};

        constexpr bool await_ready() const { return false; }

        void await_suspend(std::coroutine_handle<> handle)
        {
            context.submit(file_descriptor, [this, handle](const std::string& line) {
                value = line;
                handle.resume();
            });
        }

        std::string await_resume() const
        {
            return value;
        }
    };
}

int to_be_made_async()
{
    return 17;
}

io::task read_more(io::context& context)
{
    std::cout << "second: " << co_await io::async_reader{context, 1} << '\n';
    std::cout << "third: " << co_await io::async_reader{context, 1} << '\n';
}

io::task read(io::context& context)
{
    std::cout << "first: " << co_await io::async_reader{context, 1} << '\n';
    std::cout << "value: " << co_await to_be_made_async() << '\n';
    co_await read_more(context);
    std::cout << "end read\n";
}

// title: Implementing a C++ Coroutine Task from Scratch - Dietmar Kühl - ACCU 2023
// src: https://youtu.be/Npiw4cYElng
// code: https://github.com/dietmarkuehl/co_await-all-the-things
int main()
{
    io::context context;
    auto task = read(context);
    task.start();

    context.complete(1, "first line");
    std::cout << "back in main\n";
    context.complete(1, "second line");
    context.complete(1, "third line");
    return 0;
}
