#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char curr_pos;

    ctx.Low = ctx.Hight = StackBottom;
    if (ctx.Low > &curr_pos) {
        ctx.Low = &curr_pos;
    } else {
        ctx.Hight = &curr_pos;
    }

    size_t size = ctx.Hight - ctx.Low;
    auto *buf = std::get<0>(ctx.Stack);

    if (std::get<1>(ctx.Stack) < size or buf == nullptr) {
        delete[] buf;
        buf = new char[size];
        ctx.Stack = std::tuple<char *, uint32_t>(buf, size);
    }

    memcpy(buf, ctx.Low, size);
}

void Engine::Restore(context &ctx) {
    char curr_pos;

    if (ctx.Low > &curr_pos and ctx.Hight > &curr_pos) {
        Restore(ctx);
    }

    memcpy(ctx.Low, std::get<0>(ctx.Stack), ctx.Hight-ctx.Low);
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    if (alive == nullptr) {
        return;
    }

    auto start = static_cast<context*>(alive);
    if (start != cur_routine) {
        Enter(*start);
    } else {
        if (start->next != nullptr) {
            Enter(*start->next);
        }
        else {
            return;
        }
    }
}

void Engine::sched(void *routine_) {
    if (routine_ == nullptr) {
        yield();
    }

    auto ctx = static_cast<context*>(routine_);

    if (ctx != idle_ctx) {
        Enter(*ctx);
    } else {
        yield();
    }
}

void Engine::Enter(context& ctx) {
    if (cur_routine != nullptr and cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) == 0) {
            Store(*cur_routine);
        }
    }

    cur_routine = &ctx;
    Restore(ctx);
}

} // namespace Coroutine
} // namespace Afina
