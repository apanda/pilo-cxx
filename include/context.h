#include <boost/heap/fibonacci_heap.hpp>
#include <tuple>

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

namespace PILO {
    typedef std::function<void(int)> Task;
    typedef double Time;
    typedef double BPS;
    class Context {
        public:
            Context(Time endTime);
            bool next();
            Time get_time() const;
            inline Time now() const { return _time; }
            void set_time(Time time);
            void schedule(Time delta, std::function<void(Time)> task);
            void scheduleAbsolute(Time time, std::function<void(Time)> task);
        private:
            struct TaskCompare {
                inline bool operator()(const std::tuple<Time, Task>& t1, 
                        const std::tuple<Time, Task>& t2) const {
                    Time time1, time2;
                    std::tie(time1, std::ignore) = t1;
                    std::tie(time2, std::ignore) = t2;
                    return time1 > time2;
                }

            };
            typedef boost::heap::fibonacci_heap<std::tuple<Time, Task>, boost::heap::compare<TaskCompare>> 
                Queue;
            Queue _queue;
            Time _time;
            Time _end;
    };
}
#endif
