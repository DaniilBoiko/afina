#include <afina/concurrency/Executor.h>

#include <condition_variable>

namespace Afina {
namespace Concurrency {
    Executor::Executor(Afina::Concurrency::Executor &&) {};
    Executor::Executor(std::string name, int size) {};
    Executor& Executor::operator=(Afina::Concurrency::Executor &&) {};
    Executor& Executor::operator=(const Afina::Concurrency::Executor &) {}
    Executor::~Executor() {};

    void perform(Executor *executor) {
        auto border = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
        while (executor -> state == Executor::State::kRun) {
            if (std::chrono::system_clock::now() > border) {
                break;
            }
            std::unique_lock<std::mutex> lk(executor -> mutex);
            executor -> empty_condition.wait(lk, [executor]{return !(executor -> tasks.empty());});
            for (auto task : executor -> tasks) {

            }
        }

        std::unique_lock<std::mutex> lk(executor -> mutex);
        std::thread::id current_thread_id = std::this_thread::get_id();

        for (auto thread = executor -> threads.begin(); thread < executor -> threads.end(); thread++) {
            if (thread -> get_id() == current_thread_id) {
                executor -> threads.erase(thread);
                break;
            }
        }
        if (executor -> threads.empty()) {
            executor -> empty_condition.notify_one();
        }
    }

    void Executor::Stop(bool await) {
        state = State::kStopping;

        if (await) {
            std::unique_lock<std::mutex> lk(mutex);
            empty_condition.wait(lk, [this]{return (this->threads.empty());});
        }

        state = State::kStopped;
    };

}
} // namespace Afina
