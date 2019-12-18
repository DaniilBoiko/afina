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
        }

        memcpy(buf, ctx.Low, size);
        ctx.Stack = std::tuple<char *, uint32_t>(buf, size);
    }

    void Engine::Restore(context &ctx) {
        char curr_pos;

        if (ctx.Low > &curr_pos and ctx.Hight > &curr_pos) {
            Restore(ctx);
        }

        memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
        longjmp(ctx.Environment, 1);
    }

    void Engine::yield() {
        if (alive == nullptr) {
            return;
        }

        context *start = alive;
        while (start == cur_routine or start->calling or start->State == DEAD) {
            if (start->next) {
                start = start->next;
            } else {
                start = start->next;
                break;
            }
        }

        if (start) {
            sched(start);
        } else {
            return;
        }
    }

    void Engine::sched(void *routine_) {
        if (!routine_) {
            if (cur_routine) {
                if (cur_routine->called_by) {
                    sched(cur_routine->called_by);
                } else {
                    yield();
                }
            }
            return;
        }

        context *ctx = (context *) routine_;

        if (cur_routine) {
            cur_routine->calling = ctx;
            ctx->called_by = cur_routine;
            cur_routine->State = SUSPENDED;
            Store(*cur_routine);

            if (setjmp(cur_routine->Environment)) {
                return;
            }
        }

        cur_routine = ctx;
        cur_routine->State = RUNNING;

        if (ctx != idle_ctx) {
            Restore(*ctx);
        }
    }

} // namespace Coroutine
} // namespace Afina
