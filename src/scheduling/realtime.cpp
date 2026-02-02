#include "realtime.hpp"

#include <spdlog/spdlog.h>

#ifdef __linux__
#include <sched.h>
#include <sys/mman.h>
#endif

namespace elrs {
namespace scheduling {

void enableRealtimeScheduling(int priority) {
#ifdef __linux__
    // Set SCHED_FIFO with the requested priority
    struct sched_param param{};
    param.sched_priority = priority;

    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        spdlog::warn("Failed to set SCHED_FIFO (priority {}): {} - "
                      "run as root or set CAP_SYS_NICE for best timing",
                      priority, strerror(errno));
    } else {
        spdlog::info("Enabled SCHED_FIFO priority {}", priority);
    }

    // Lock current and future memory pages
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        spdlog::warn("Failed to mlockall: {} - "
                      "page faults may affect timing",
                      strerror(errno));
    } else {
        spdlog::debug("Memory locked (mlockall)");
    }
#else
    (void)priority;
    spdlog::debug("Real-time scheduling not available on this platform");
#endif
}

void disableRealtimeScheduling() {
#ifdef __linux__
    struct sched_param param{};
    param.sched_priority = 0;
    sched_setscheduler(0, SCHED_OTHER, &param);
    munlockall();
    spdlog::debug("Restored default scheduling");
#endif
}

}  // namespace scheduling
}  // namespace elrs
