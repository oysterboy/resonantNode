#pragma once

#include <stddef.h>

/*
RingQueue (step 11e item 4)

A fixed-capacity FIFO over inline storage, for the single-threaded loop: no
allocation, no exceptions, no iterators to invalidate. It replaces the
hand-rolled read-index / count pairs that DetectionRuntime (verdicts, pending
inspections, Analyzer pipeline events), ResonantBehavior (own-emit history)
and NodeNet (radio outbox) each carried.

push() refuses when full and the caller counts the overflow; pushOverwrite()
drops the oldest instead (a history, not a queue). emplace() hands out the
next free slot for in-place filling. at(i) is the i-th element from the
front. clear() forgets the elements without touching their storage.
*/
template <class T, size_t N>
class RingQueue {
public:
    static constexpr size_t kCapacity = N;

    size_t count() const { return _count; }
    bool empty() const { return _count == 0; }
    bool full() const { return _count == N; }

    void clear() {
        _read = 0;
        _count = 0;
    }

    // Appends a copy; false, and nothing changes, when full.
    bool push(const T& value) {
        T* slot = emplace();
        if (slot == nullptr) {
            return false;
        }
        *slot = value;
        return true;
    }

    // Appends a copy; when full, the oldest element makes room.
    void pushOverwrite(const T& value) {
        if (full()) {
            popFront();
        }
        *emplace() = value;
    }

    // The next free slot, to fill in place; nullptr when full.
    T* emplace() {
        if (full()) {
            return nullptr;
        }
        T* slot = &_slots[(_read + _count) % N];
        ++_count;
        return slot;
    }

    // Copies the oldest element out and drops it; false when empty.
    bool pop(T& out) {
        if (empty()) {
            return false;
        }
        out = _slots[_read];
        popFront();
        return true;
    }

    // The oldest element; only valid when !empty().
    T& front() { return _slots[_read]; }
    const T& front() const { return _slots[_read]; }

    // Drops the oldest element; nothing happens when empty.
    void popFront() {
        if (empty()) {
            return;
        }
        _read = (_read + 1) % N;
        --_count;
    }

    // The i-th element from the front, i < count().
    T& at(size_t i) { return _slots[(_read + i) % N]; }
    const T& at(size_t i) const { return _slots[(_read + i) % N]; }

private:
    T _slots[N] = {};
    size_t _read = 0;
    size_t _count = 0;
};
