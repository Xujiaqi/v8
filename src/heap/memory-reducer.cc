// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-reducer.h"

#include "src/flags.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/utils.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

const int MemoryReducer::kLongDelayMs = 20000;
const int MemoryReducer::kShortDelayMs = 500;
const int MemoryReducer::kWatchdogDelayMs = 100000;
const int MemoryReducer::kMaxNumberOfGCs = 3;

MemoryReducer::TimerTask::TimerTask(MemoryReducer* memory_reducer)
    : CancelableTask(memory_reducer->heap()->isolate()),
      memory_reducer_(memory_reducer) {}


void MemoryReducer::TimerTask::RunInternal() {
  Heap* heap = memory_reducer_->heap();
  Event event;
  double time_ms = heap->MonotonicallyIncreasingTimeInMs();
  heap->tracer()->SampleAllocation(time_ms, heap->NewSpaceAllocationCounter(),
                                   heap->OldGenerationAllocationCounter());
  event.type = kTimer;
  event.time_ms = time_ms;
  event.low_allocation_rate = heap->HasLowAllocationRate();
  event.can_start_incremental_gc =
      heap->incremental_marking()->IsStopped() &&
      heap->incremental_marking()->CanBeActivated();
  memory_reducer_->NotifyTimer(event);
}


void MemoryReducer::NotifyTimer(const Event& event) {
  DCHECK_EQ(kTimer, event.type);
  DCHECK_EQ(kWait, state_.action);
  state_ = Step(state_, event);
  if (state_.action == kRun) {
    DCHECK(heap()->incremental_marking()->IsStopped());
    DCHECK(FLAG_incremental_marking);
    if (FLAG_trace_gc_verbose) {
      PrintIsolate(heap()->isolate(), "Memory reducer: started GC #%d\n",
                   state_.started_gcs);
    }
    if (heap()->ShouldOptimizeForMemoryUsage()) {
      // Do full GC if memory usage has higher priority than latency. This is
      // important for background tabs that do not send idle notifications.
      heap()->CollectAllGarbage(Heap::kReduceMemoryFootprintMask,
                                "memory reducer");
    } else {
      heap()->StartIdleIncrementalMarking();
    }
  } else if (state_.action == kWait) {
    if (!heap()->incremental_marking()->IsStopped() &&
        heap()->ShouldOptimizeForMemoryUsage()) {
      // Make progress with pending incremental marking if memory usage has
      // higher priority than latency. This is important for background tabs
      // that do not send idle notifications.
      const int kIncrementalMarkingDelayMs = 500;
      double deadline = heap()->MonotonicallyIncreasingTimeInMs() +
                        kIncrementalMarkingDelayMs;
      heap()->incremental_marking()->AdvanceIncrementalMarking(
          0, deadline, i::IncrementalMarking::StepActions(
                           i::IncrementalMarking::NO_GC_VIA_STACK_GUARD,
                           i::IncrementalMarking::FORCE_MARKING,
                           i::IncrementalMarking::FORCE_COMPLETION));
      heap()->FinalizeIncrementalMarkingIfComplete(
          "Memory reducer: finalize incremental marking");
    }
    // Re-schedule the timer.
    ScheduleTimer(state_.next_gc_start_ms - event.time_ms);
    if (FLAG_trace_gc_verbose) {
      PrintIsolate(heap()->isolate(), "Memory reducer: waiting for %.f ms\n",
                   state_.next_gc_start_ms - event.time_ms);
    }
  }
}


void MemoryReducer::NotifyMarkCompact(const Event& event) {
  DCHECK_EQ(kMarkCompact, event.type);
  Action old_action = state_.action;
  state_ = Step(state_, event);
  if (old_action != kWait && state_.action == kWait) {
    // If we are transitioning to the WAIT state, start the timer.
    ScheduleTimer(state_.next_gc_start_ms - event.time_ms);
  }
  if (old_action == kRun) {
    if (FLAG_trace_gc_verbose) {
      PrintIsolate(heap()->isolate(), "Memory reducer: finished GC #%d (%s)\n",
                   state_.started_gcs,
                   state_.action == kWait ? "will do more" : "done");
    }
  }
}


void MemoryReducer::NotifyContextDisposed(const Event& event) {
  DCHECK_EQ(kContextDisposed, event.type);
  Action old_action = state_.action;
  state_ = Step(state_, event);
  if (old_action != kWait && state_.action == kWait) {
    // If we are transitioning to the WAIT state, start the timer.
    ScheduleTimer(state_.next_gc_start_ms - event.time_ms);
  }
}


void MemoryReducer::NotifyBackgroundIdleNotification(const Event& event) {
  DCHECK_EQ(kBackgroundIdleNotification, event.type);
  Action old_action = state_.action;
  int old_started_gcs = state_.started_gcs;
  state_ = Step(state_, event);
  if (old_action == kWait && state_.action == kWait &&
      old_started_gcs + 1 == state_.started_gcs) {
    DCHECK(heap()->incremental_marking()->IsStopped());
    // TODO(ulan): Replace it with incremental marking GC once
    // chromium:490559 is fixed.
    if (event.time_ms > state_.last_gc_time_ms + kLongDelayMs) {
      heap()->CollectAllGarbage(Heap::kReduceMemoryFootprintMask,
                                "memory reducer background GC");
    } else {
      DCHECK(FLAG_incremental_marking);
      heap()->StartIdleIncrementalMarking();
      if (FLAG_trace_gc_verbose) {
        PrintIsolate(heap()->isolate(),
                     "Memory reducer: started GC #%d"
                     " (background idle)\n",
                     state_.started_gcs);
      }
    }
  }
}


bool MemoryReducer::WatchdogGC(const State& state, const Event& event) {
  return state.last_gc_time_ms != 0 &&
         event.time_ms > state.last_gc_time_ms + kWatchdogDelayMs;
}


// For specification of this function see the comment for MemoryReducer class.
MemoryReducer::State MemoryReducer::Step(const State& state,
                                         const Event& event) {
  if (!FLAG_incremental_marking || !FLAG_memory_reducer) {
    return State(kDone, 0, 0, state.last_gc_time_ms);
  }
  switch (state.action) {
    case kDone:
      if (event.type == kTimer || event.type == kBackgroundIdleNotification) {
        return state;
      } else {
        DCHECK(event.type == kContextDisposed || event.type == kMarkCompact);
        return State(
            kWait, 0, event.time_ms + kLongDelayMs,
            event.type == kMarkCompact ? event.time_ms : state.last_gc_time_ms);
      }
    case kWait:
      switch (event.type) {
        case kContextDisposed:
          return state;
        case kTimer:
          if (state.started_gcs >= kMaxNumberOfGCs) {
            return State(kDone, kMaxNumberOfGCs, 0.0, state.last_gc_time_ms);
          } else if (event.can_start_incremental_gc &&
                     (event.low_allocation_rate || WatchdogGC(state, event))) {
            if (state.next_gc_start_ms <= event.time_ms) {
              return State(kRun, state.started_gcs + 1, 0.0,
                           state.last_gc_time_ms);
            } else {
              return state;
            }
          } else {
            return State(kWait, state.started_gcs, event.time_ms + kLongDelayMs,
                         state.last_gc_time_ms);
          }
        case kBackgroundIdleNotification:
          if (event.can_start_incremental_gc &&
              state.started_gcs < kMaxNumberOfGCs) {
            return State(kWait, state.started_gcs + 1,
                         event.time_ms + kLongDelayMs, state.last_gc_time_ms);
          } else {
            return state;
          }
        case kMarkCompact:
          return State(kWait, state.started_gcs, event.time_ms + kLongDelayMs,
                       event.time_ms);
      }
    case kRun:
      if (event.type != kMarkCompact) {
        return state;
      } else {
        if (state.started_gcs < kMaxNumberOfGCs &&
            (event.next_gc_likely_to_collect_more || state.started_gcs == 1)) {
          return State(kWait, state.started_gcs, event.time_ms + kShortDelayMs,
                       event.time_ms);
        } else {
          return State(kDone, kMaxNumberOfGCs, 0.0, event.time_ms);
        }
      }
  }
  UNREACHABLE();
  return State(kDone, 0, 0, 0.0);  // Make the compiler happy.
}


void MemoryReducer::ScheduleTimer(double delay_ms) {
  DCHECK(delay_ms > 0);
  // Leave some room for precision error in task scheduler.
  const double kSlackMs = 100;
  v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(heap()->isolate());
  auto timer_task = new MemoryReducer::TimerTask(this);
  V8::GetCurrentPlatform()->CallDelayedOnForegroundThread(
      isolate, timer_task, (delay_ms + kSlackMs) / 1000.0);
}


void MemoryReducer::TearDown() { state_ = State(kDone, 0, 0, 0.0); }

}  // namespace internal
}  // namespace v8
