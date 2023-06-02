#include <core/core.hpp>
#include <async/async.hpp>

namespace ion {
async::async() { }

///
async::async(size_t count, FnProcess fn) : async() {
    ///
    std::unique_lock<std::mutex> lock(process::mtx_list);
    
    /// create empty results [dynamic] vector [we need to couple the count into process, or perhaps bring it into async]
    d.proc      = process { count, fn };
    runtime &ps = d.proc.state;
    ps.handle   = d.proc.mem->grab();
    
    /// measure d.proc.rt address here
    ps.threads = new std::vector<std::thread>();
    ps.threads->reserve(count);
    for (size_t i = 0; i < count; i++)
        ps.threads->emplace_back(std::thread(process::run, &ps, int(i)));
    ///
    process::procs.push(&ps);
}

/// path to execute, we dont host a bunch of environment vars. ar.
/// make path a string instance i think; fs::path doesnt offs
async::async(exec command) : async(1, [&](runtime &proc, int i) -> mx {
    path p   = command.path();
    path dir = p.parent();
    
    if (dir == "" || dir == ".") {
        path   cwd = dir::cwd(); /// this always puts a trailing slash on
        path local = cwd / p;
        assert(local != "");
        exec   cmd = fmt {"{0}{1}", { str(local.cs()), command }};
        command    = cmd;
        assert(local.exists());
    } else {
        /// would rather not use anything in PATH
        if (p.exists()) {
            console.log("std::system (shell) will default to PATH for command:\n > {0}", { p });
        }
    }
    console.log("shell > {0}", { command });
    int exit_code = int(std::system(command.cs()));
    return exit_code;
}) { }

async::async(lambda<mx(runtime &, int)> fn) : async(1, fn) {}

array<mx> async::sync() {
    /// wait for join to complete, set results internal and return
    for (;;) {
        d.mtx.lock();
        if (d.proc.joining()) {
            d.mtx.unlock();
            break;
        }
        d.mtx.unlock();
        yield();
    }
    return d.proc.state.results;
}

/// await all async processes to complete
int async::await() {
    for (;;) {
        process::mtx_global.lock();
        if (!process::procs.len()) {
            process::mtx_global.unlock();
            break;
        }
        process::mtx_global.unlock();
        sleep(u64(1000));
    }
    process::th_manager.join();
    return process::exit_code;
}

/// return future for this async
async::operator future() {
    lambda<void(mx)>   s, f;
    completer    c = { s, f };
    assert( d.proc);
    assert(!d.proc.state.on_done);
    d.proc.state.on_done = [s, f](mx v) {
        s(v);
    };
    d.proc.state.on_fail = [s, f](mx v) {
        f(v);
    };
    return future(c);
}
}
