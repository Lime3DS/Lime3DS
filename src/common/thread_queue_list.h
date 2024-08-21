// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/split_member.hpp>
#include "common/common_types.h"

namespace Common {

template <class T, unsigned int N>
struct ThreadQueueList {
    // TODO(yuriks): If performance proves to be a problem, the std::deques can be replaced with
    //               (dynamically resizable) circular buffers to remove their overhead when
    //               inserting and popping.

    using Priority = unsigned int;

    // Number of priority levels. (Valid levels are [0..NUM_QUEUES).)
    static constexpr Priority NUM_QUEUES = N;

    ThreadQueueList() {
        first = nullptr;
    }

    // Only for debugging, returns priority level.
    [[nodiscard]] Priority contains(const T& uid) const {
        for (Priority i = 0; i < NUM_QUEUES; ++i) {
            const Queue& cur = queues[i];
            if (std::find(cur.data.cbegin(), cur.data.cend(), uid) != cur.data.cend()) {
                return i;
            }
        }

        return -1;
    }

    [[nodiscard]] T get_first() const {
        const Queue* cur = first;
        while (cur != nullptr) {
            if (!cur->data.empty()) {
                return cur->data.front();
            }
            cur = cur->next_nonempty;
        }

        return T();
    }

    T pop_first() {
        Queue* cur = first;
        while (cur != nullptr) {
            if (!cur->data.empty()) {
                auto tmp = std::move(cur->data.front());
                cur->data.pop_front();
                return tmp;
            }
            cur = cur->next_nonempty;
        }

        return T();
    }

    T pop_first_better(Priority priority) {
        Queue* cur = first;
        Queue* stop = &queues[priority];
        while (cur < stop) {
            if (!cur->data.empty()) {
                auto tmp = std::move(cur->data.front());
                cur->data.pop_front();
                return tmp;
            }
            cur = cur->next_nonempty;
        }

        return T();
    }

    void push_front(Priority priority, const T& thread_id) {
        Queue* cur = &queues[priority];
        cur->data.push_front(thread_id);
    }

    void push_back(Priority priority, const T& thread_id) {
        Queue* cur = &queues[priority];
        cur->data.push_back(thread_id);
    }

    void move(const T& thread_id, Priority old_priority, Priority new_priority) {
        remove(old_priority, thread_id);
        prepare(new_priority);
        push_back(new_priority, thread_id);
    }

    void remove(Priority priority, const T& thread_id) {
        Queue* const cur = &queues[priority];
        const auto iter = std::remove(cur->data.begin(), cur->data.end(), thread_id);
        cur->data.erase(iter, cur->data.end());
    }

    void rotate(Priority priority) {
        Queue* cur = &queues[priority];

        if (cur->data.size() > 1) {
            cur->data.push_back(std::move(cur->data.front()));
            cur->data.pop_front();
        }
    }

    void clear() {
        queues.fill(Queue());
        first = nullptr;
    }

    [[nodiscard]] bool empty(Priority priority) const {
        const Queue* cur = &queues[priority];
        return cur->data.empty();
    }

    void prepare(Priority priority) {
        Queue* cur = &queues[priority];
        if (cur->next_nonempty == UnlinkedTag())
            link(priority);
    }

private:
    struct Queue {
        // Points to the next active priority, skipping over ones that have never been used.
        Queue* next_nonempty = UnlinkedTag();
        // Double-ended queue of threads in this priority level
        std::deque<T> data;
    };

    /// Special tag used to mark priority levels that have never been used.
    static Queue* UnlinkedTag() {
        return reinterpret_cast<Queue*>(1);
    }

    void link(Priority priority) {
        Queue* cur = &queues[priority];

        for (int i = priority - 1; i >= 0; --i) {
            if (queues[i].next_nonempty != UnlinkedTag()) {
                cur->next_nonempty = queues[i].next_nonempty;
                queues[i].next_nonempty = cur;
                return;
            }
        }

        cur->next_nonempty = first;
        first = cur;
    }

    // The first queue that's ever been used.
    Queue* first;
    // The priority level queues of thread ids.
    std::array<Queue, NUM_QUEUES> queues;

    s64 ToIndex(const Queue* q) const {
        if (q == nullptr) {
            return -2;
        } else if (q == UnlinkedTag()) {
            return -1;
        } else {
            return q - queues.data();
        }
    }

    Queue* ToPointer(s64 idx) {
        if (idx == -1) {
            return UnlinkedTag();
        } else if (idx < 0) {
            return nullptr;
        } else {
            return &queues[idx];
        }
    }

    friend class boost::serialization::access;
    template <class Archive>
    void save(Archive& ar, const unsigned int file_version) const {
        const s64 idx = ToIndex(first);
        ar << idx;
        for (std::size_t i = 0; i < NUM_QUEUES; i++) {
            const s64 idx1 = ToIndex(queues[i].next_nonempty);
            ar << idx1;
            ar << queues[i].data;
        }
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int file_version) {
        s64 idx;
        ar >> idx;
        first = ToPointer(idx);
        for (std::size_t i = 0; i < NUM_QUEUES; i++) {
            ar >> idx;
            queues[i].next_nonempty = ToPointer(idx);
            ar >> queues[i].data;
        }
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()
};

} // namespace Common
