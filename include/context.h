#include <boost/heap/fibonacci_heap.hpp>
#include <tuple>

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

namespace PILO {
    typedef std::function<void(int)> Task;
    typedef double Time;
    typedef double BPS;

    // Simulation context. Responsible for maintaining time and scheduling events.
    class Context {
        public:
            Context(Time endTime);

            // Run the next event. Return false if no more events need to be run.
            bool next();

            // Get current time.
            Time get_time() const;

            // Also get current time, I kept screwing up.
            inline Time now() const { return _time; }

            // Set the current time.
            void set_time(Time time);

            // Schedule an event to happen delta time after now.
            void schedule(Time delta, std::function<void(Time)> task);

            // Schedule an event to happen at specified time.
            void scheduleAbsolute(Time time, std::function<void(Time)> task);
        private:

            // Comparator that ignores the task, so we can use fibonacci heap.
            struct TaskCompare {
                inline bool operator()(const std::tuple<Time, Task>& t1,
                        const std::tuple<Time, Task>& t2) const {
                    Time time1, time2;
                    std::tie(time1, std::ignore) = t1;
                    std::tie(time2, std::ignore) = t2;
                    return time1 > time2;
                }

            };

            // Fibonacci heap since it is fast
            typedef boost::heap::fibonacci_heap<std::tuple<Time, Task>, boost::heap::compare<TaskCompare>>
                Queue;

            // Queue of events
            Queue _queue;

            // Current time.
            Time _time;

            // End time.
            Time _end;

            uint64_t _lastMajor;
    };
}
#endif
