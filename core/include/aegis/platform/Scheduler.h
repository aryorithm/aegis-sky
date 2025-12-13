#pragma once
#include <string>

namespace aegis::platform {

    class Scheduler {
    public:
        // Elevate current thread to Real-Time priority (requires sudo)
        // priority: 1 (Lowest RT) to 99 (Highest RT)
        static bool set_realtime_priority(int priority);

        // Set thread name for `htop` debugging
        static void set_thread_name(const std::string& name);

        // Pin thread to a specific CPU Core (e.g., Core 7 reserved for Fusion)
        static bool set_cpu_affinity(int core_id);
    };
}