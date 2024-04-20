// Copyright 2008 Dolphin Emulator Project / 2017 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <random>
#include <tuple>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core_timing.h"

namespace Core {

// Sort by time, unless the times are the same, in which case sort by the order added to the queue
bool Timing::Event::operator>(const Timing::Event& right) const {
    return std::tie(time, fifo_order) > std::tie(right.time, right.fifo_order);
}

bool Timing::Event::operator<(const Timing::Event& right) const {
    return std::tie(time, fifo_order) < std::tie(right.time, right.fifo_order);
}

Timing::Timing(std::size_t num_cores, u32 cpu_clock_percentage, s64 override_base_ticks) {
    // Generate non-zero base tick count to simulate time the system ran before launching the game.
    // This accounts for games that rely on the system tick to seed randomness.
    const auto base_ticks = override_base_ticks >= 0 ? override_base_ticks : GenerateBaseTicks();

    timers.resize(num_cores);
    for (std::size_t i = 0; i < num_cores; ++i) {
        timers[i] = std::make_shared<Timer>(base_ticks);
    }
    UpdateClockSpeed(cpu_clock_percentage);
    current_timer = timers[0].get();
}

s64 Timing::GenerateBaseTicks() {
    if (Settings::values.init_ticks_type.GetValue() == Settings::InitTicks::Fixed) {
        return Settings::values.init_ticks_override.GetValue();
    }
    // Bounded to 32 bits to make sure we don't generate too high of a counter and risk overflowing.
    std::mt19937 random_gen(std::random_device{}());
    return random_gen();
}

void Timing::UpdateClockSpeed(u32 cpu_clock_percentage) {
    for (auto& timer : timers) {
        timer->cpu_clock_scale = 100.0 / cpu_clock_percentage;
    }
}

TimingEventType* Timing::RegisterEvent(const std::string& name, TimedCallback callback) {
    // check for existing type with same name.
    // we want event type names to remain unique so that we can use them for serialization.
    auto info = event_types.emplace(name, TimingEventType{});
    TimingEventType* event_type = &info.first->second;
    event_type->name = &info.first->first;
    if (callback != nullptr) {
        event_type->callback = callback;
    }
    return event_type;
}

void Timing::ScheduleEvent(s64 cycles_into_future, const TimingEventType* event_type,
                           std::uintptr_t user_data, std::size_t core_id, bool thread_safe_mode) {
    if (event_queue_locked) {
        return;
    }

    ASSERT(event_type != nullptr);
    Timing::Timer* timer = nullptr;
    if (core_id == std::numeric_limits<std::size_t>::max()) {
        timer = current_timer;
    } else {
        ASSERT(core_id < timers.size());
        timer = timers.at(core_id).get();
    }

    if (thread_safe_mode) {
        // Events scheduled in thread safe mode come after blocking operations with
        // unpredictable timings in the host machine, so there is no need to be cycle accurate.
        // To prevent the event from scheduling before the next advance(), we set a minimum time
        // of MAX_SLICE_LENGTH * 2 cycles into the future.
        cycles_into_future = std::max(static_cast<s64>(MAX_SLICE_LENGTH * 2), cycles_into_future);

        timer->ts_queue.Push(Event{static_cast<s64>(timer->GetTicks() + cycles_into_future), 0,
                                   user_data, event_type});
    } else {
        s64 timeout = timer->GetTicks() + cycles_into_future;
        if (current_timer == timer) {
            // If this event needs to be scheduled before the next advance(), force one early
            if (!timer->is_timer_sane)
                timer->ForceExceptionCheck(cycles_into_future);

            timer->event_queue.emplace_back(
                Event{timeout, timer->event_fifo_id++, user_data, event_type});
            std::push_heap(timer->event_queue.begin(), timer->event_queue.end(), std::greater<>());
        } else {
            timer->ts_queue.Push(Event{static_cast<s64>(timer->GetTicks() + cycles_into_future), 0,
                                       user_data, event_type});
        }
    }
}

void Timing::UnscheduleEvent(const TimingEventType* event_type, std::uintptr_t user_data) {
    if (event_queue_locked) {
        return;
    }
    for (auto timer : timers) {
        auto itr = std::remove_if(
            timer->event_queue.begin(), timer->event_queue.end(),
            [&](const Event& e) { return e.type == event_type && e.user_data == user_data; });

        // Removing random items breaks the invariant so we have to re-establish it.
        if (itr != timer->event_queue.end()) {
            timer->event_queue.erase(itr, timer->event_queue.end());
            std::make_heap(timer->event_queue.begin(), timer->event_queue.end(), std::greater<>());
        }
    }
    // TODO:remove events from ts_queue
}

void Timing::RemoveEvent(const TimingEventType* event_type) {
    if (event_queue_locked) {
        return;
    }
    for (auto timer : timers) {
        auto itr = std::remove_if(timer->event_queue.begin(), timer->event_queue.end(),
                                  [&](const Event& e) { return e.type == event_type; });

        // Removing random items breaks the invariant so we have to re-establish it.
        if (itr != timer->event_queue.end()) {
            timer->event_queue.erase(itr, timer->event_queue.end());
            std::make_heap(timer->event_queue.begin(), timer->event_queue.end(), std::greater<>());
        }
    }
    // TODO:remove events from ts_queue
}

void Timing::SetCurrentTimer(std::size_t core_id) {
    current_timer = timers[core_id].get();
}

s64 Timing::GetTicks() const {
    return current_timer->GetTicks();
}

s64 Timing::GetGlobalTicks() const {
    const auto& timer =
        std::max_element(timers.cbegin(), timers.cend(), [](const auto& a, const auto& b) {
            return a->GetTicks() < b->GetTicks();
        });
    return (*timer)->GetTicks();
}

std::chrono::microseconds Timing::GetGlobalTimeUs() const {
    return std::chrono::microseconds{GetGlobalTicks() * 1000000 / BASE_CLOCK_RATE_ARM11};
}

std::shared_ptr<Timing::Timer> Timing::GetTimer(std::size_t cpu_id) {
    return timers[cpu_id];
}

Timing::Timer::Timer(s64 base_ticks) : executed_ticks(base_ticks) {}

Timing::Timer::~Timer() {
    MoveEvents();
}

u64 Timing::Timer::GetTicks() const {
    u64 ticks = static_cast<u64>(executed_ticks);
    if (!is_timer_sane) {
        ticks += slice_length - downcount;
    }
    return ticks;
}

void Timing::Timer::AddTicks(u64 ticks) {
    downcount -= static_cast<u64>(ticks * cpu_clock_scale);
}

u64 Timing::Timer::GetIdleTicks() const {
    return static_cast<u64>(idled_cycles);
}

void Timing::Timer::ForceExceptionCheck(s64 cycles) {
    cycles = std::max<s64>(0, cycles);
    if (downcount > cycles) {
        slice_length -= downcount - cycles;
        downcount = cycles;
    }
}

void Timing::Timer::MoveEvents() {
    for (Event ev; ts_queue.Pop(ev);) {
        ev.fifo_order = event_fifo_id++;
        event_queue.emplace_back(std::move(ev));
        std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

s64 Timing::Timer::GetMaxSliceLength() const {
    const auto& next_event = event_queue.begin();
    if (next_event != event_queue.end()) {
        ASSERT(next_event->time - executed_ticks > 0);
        return next_event->time - executed_ticks;
    }
    return MAX_SLICE_LENGTH;
}

void Timing::Timer::Advance() {
    MoveEvents();

    s64 cycles_executed = slice_length - downcount;
    idled_cycles = 0;
    executed_ticks += cycles_executed;
    slice_length = 0;
    downcount = 0;

    is_timer_sane = true;

    while (!event_queue.empty() && event_queue.front().time <= executed_ticks) {
        Event evt = std::move(event_queue.front());
        std::pop_heap(event_queue.begin(), event_queue.end(), std::greater<>());
        event_queue.pop_back();
        if (evt.type->callback != nullptr) {
            evt.type->callback(evt.user_data, static_cast<int>(executed_ticks - evt.time));
        } else {
            LOG_ERROR(Core, "Event '{}' has no callback", *evt.type->name);
        }
    }

    is_timer_sane = false;
}

void Timing::Timer::SetNextSlice(s64 max_slice_length) {
    slice_length = max_slice_length;

    // Still events left (scheduled in the future)
    if (!event_queue.empty()) {
        slice_length = static_cast<int>(
            std::min<s64>(event_queue.front().time - executed_ticks, max_slice_length));
    }

    downcount = slice_length >> downcount_hack;
}

void Timing::Timer::Idle() {
    idled_cycles += downcount;
    downcount = 0;
}

s64 Timing::Timer::GetDowncount() const {
    return downcount;
}

} // namespace Core
