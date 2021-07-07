/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ThrottledFunc.h
--*/

#pragma once
#include "pch.h"

// Class Description:
// - Represents a function that takes arguments and whose invocation is
//   delayed by a specified duration and rate-limited such that if the code
//   tries to run the function while a call to the function is already
//   pending, then the previous call with the previous arguments will be
//   cancelled and the call will be made with the new arguments instead.
// - The function will be run on the the specified dispatcher.
template<typename... Args>
class ThrottledFunc : public std::enable_shared_from_this<ThrottledFunc<Args...>>
{
public:
    using Func = std::function<void(Args...)>;

    ThrottledFunc(Func func, winrt::Windows::Foundation::TimeSpan delay, winrt::Windows::UI::Core::CoreDispatcher dispatcher) :
        _func{ func },
        _delay{ delay },
        _dispatcher{ dispatcher }
    {
    }

    // Method Description:
    // - Runs the function later with the specified arguments, except if `Run`
    //   is called again before with new arguments, in which case the new
    //   arguments will be used instead.
    // - For more information, read the class' documentation.
    // - This method is always thread-safe. It can be called multiple times on
    //   different threads.
    // Arguments:
    // - arg: the argument to pass to the function
    // Return Value:
    // - <none>
    template<typename... MakeArgs>
    void Run(MakeArgs&&... args)
    {
        {
            std::lock_guard guard{ _lock };

            bool hadValue = _pendingRunArgs.has_value();
            _pendingRunArgs.emplace(std::forward<MakeArgs>(args)...);

            if (hadValue)
            {
                // already pending
                return;
            }
        }

        _dispatcher.RunAsync(CoreDispatcherPriority::Low, [weakThis = this->weak_from_this()]() {
            if (auto self{ weakThis.lock() })
            {
                DispatcherTimer timer;
                timer.Interval(self->_delay);
                timer.Tick([=](auto&&...) {
                    if (auto self{ weakThis.lock() })
                    {
                        timer.Stop();

                        std::optional<std::tuple<Args...>> args;
                        {
                            std::lock_guard guard{ self->_lock };
                            self->_pendingRunArgs.swap(args);
                        }
                        std::apply(self->_func, args.value());
                    }
                });
                timer.Start();
            }
        });
    }

    // Method Description:
    // - Modifies the pending arguments for the next function invocation, if
    //   there is one pending currently.
    // - Let's say that you just called the `Run` method with some arguments.
    //   After the delay specified in the constructor, the function specified
    //   in the constructor will be called with these arguments.
    // - By using this method, you can modify the arguments before the function
    //   is called.
    // - You pass a function to this method which will take references to
    //   the arguments (one argument corresponds to one reference to an
    //   argument) and will modify them.
    // - When there is no pending invocation of the function, this method will
    //   not do anything.
    // - This method is always thread-safe. It can be called multiple times on
    //   different threads.
    // Arguments:
    // - f: the function to call with references to the arguments
    // Return Value:
    // - <none>
    template<typename F>
    void ModifyPending(F f)
    {
        std::lock_guard guard{ _lock };

        if (_pendingRunArgs.has_value())
        {
            std::apply(f, _pendingRunArgs.value());
        }
    }

private:
    Func _func;
    winrt::Windows::Foundation::TimeSpan _delay;
    winrt::Windows::UI::Core::CoreDispatcher _dispatcher;

    std::optional<std::tuple<Args...>> _pendingRunArgs;
    std::mutex _lock;
};

// Class Description:
// - Represents a function whose invocation is delayed by a specified duration
//   and rate-limited such that if the code tries to run the function while a
//   call to the function is already pending, the request will be ignored.
// - The function will be run on the the specified dispatcher.
template<>
class ThrottledFunc<> : public std::enable_shared_from_this<ThrottledFunc<>>
{
public:
    using Func = std::function<void()>;

    ThrottledFunc(Func func, winrt::Windows::Foundation::TimeSpan delay, winrt::Windows::UI::Core::CoreDispatcher dispatcher);

    void Run();

private:
    Func _func;
    winrt::Windows::Foundation::TimeSpan _delay;
    winrt::Windows::UI::Core::CoreDispatcher _dispatcher;

    std::atomic_flag _isRunPending;
};
