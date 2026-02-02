#pragma once

namespace elrs {
namespace scheduling {

// Enable SCHED_FIFO real-time scheduling and lock memory.
// Falls back gracefully with a warning if insufficient privileges.
// priority: RT priority (1-99, default 49)
void enableRealtimeScheduling(int priority = 49);

// Restore default (SCHED_OTHER) scheduling and unlock memory.
void disableRealtimeScheduling();

}  // namespace scheduling
}  // namespace elrs
