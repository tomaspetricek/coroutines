#include <iostream>
#include <coroutine>
#include <unordered_map>
#include <functional>


namespace io {
    struct task {
        struct promise {
            std::exception_ptr error{};
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            task get_return_object() { return {}; }
            void unhandled_exception() { error = std::current_exception(); }

//        template<class Value>
//        auto await_transform(Value value) { return value_awaiter<Value>{value}; }
        };
        using promise_type = promise;
        std::coroutine_handle<promise> handle_;
    };

    struct context {
        std::unordered_map<int, std::function<void(std::string)>> outstanding;

        void submit(int file_descriptor, auto func) {
            outstanding[file_descriptor] =  func;
        }

        void complete(int file_descriptor, std::string value) {
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

io::task read(io::context& context)
{
    std::cout << "first: " << co_await io::async_reader{context, 1} << "\n";
    std::cout << "second: " << co_await io::async_reader{context, 1} << "\n";
}

// title: Implementing a C++ Coroutine Task from Scratch - Dietmar Kühl - ACCU 2023
// src: https://youtu.be/Npiw4cYElng
int main()
{
    io::context context;
    auto task = read(context);
    (void) task;

    context.complete(1, "first line");
    std::cout << "back in main\n";
    context.complete(1, "second line");
    return 0;
}
