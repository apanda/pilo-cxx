#include "context.h"
namespace PILO {
    Context::Context():
        _time(0.0) {
    }

    Time Context::get_time() {
        return _time;
    }

    void Context::set_time(Time time) {
        _time = time;
    }

    bool Context::next() {
        if (_queue.empty()) {
            return false;
        }
        Time time;
        std::function<void(Time)> task;
        std::tie(time, task) = _queue.top();
        _queue.pop();
        _time = time;
        task(_time);
        return !_queue.empty();
    }
    
    void Context::schedule(Time delta, std::function<void(Time)> task) {
        _queue.emplace(std::make_tuple(_time + delta, task));
    }

    void Context::scheduleAbsolute(Time time, std::function<void(Time)> task) {
        assert(time >= _time);
        if (time >= _time) {
            _queue.emplace(std::make_tuple(time, task));
        }
    }
}
