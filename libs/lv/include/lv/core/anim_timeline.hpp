#pragma once

/**
 * @file anim_timeline.hpp
 * @brief Zero-cost RAII wrapper for LVGL animation timelines
 *
 * Wraps lv_anim_timeline_t with move-only RAII semantics.
 * Pointer-sized, zero overhead.
 *
 * @code
 * lv::Anim a1;
 * a1.exec_opa(label).values(0, 255).duration(500);
 *
 * lv::Anim a2;
 * a2.exec_y(btn).values(-50, 100).duration(300).ease_out();
 *
 * lv::AnimTimeline tl;
 * tl.add(0, a1)        // fade-in starts at 0ms
 *   .add(200, a2)      // slide-in starts at 200ms
 *   .repeat_infinite()
 *   .start();
 * @endcode
 */

#include <lvgl.h>
#include "anim.hpp"

namespace lv {

/**
 * @brief Animation timeline wrapper
 *
 * Sequences multiple animations with start-time offsets.
 * RAII: destructor deletes the timeline.
 *
 * Size: sizeof(void*) — 4 or 8 bytes
 */
class AnimTimeline {
    lv_anim_timeline_t* m_timeline = nullptr;

public:
    // ==================== Construction ====================

    /// Create a new empty timeline
    AnimTimeline() noexcept : m_timeline(lv_anim_timeline_create()) {}

    /// Adopt an existing timeline (caller transfers ownership)
    struct adopt_t {};
    static constexpr adopt_t adopt{};
    AnimTimeline(adopt_t, lv_anim_timeline_t* tl) noexcept : m_timeline(tl) {}

    // ==================== RAII ====================

    ~AnimTimeline() {
        if (m_timeline) lv_anim_timeline_delete(m_timeline);
    }

    // Move-only
    AnimTimeline(AnimTimeline&& other) noexcept : m_timeline(other.m_timeline) {
        other.m_timeline = nullptr;
    }

    AnimTimeline& operator=(AnimTimeline&& other) noexcept {
        if (this != &other) {
            if (m_timeline) lv_anim_timeline_delete(m_timeline);
            m_timeline = other.m_timeline;
            other.m_timeline = nullptr;
        }
        return *this;
    }

    AnimTimeline(const AnimTimeline&) = delete;
    AnimTimeline& operator=(const AnimTimeline&) = delete;

    // ==================== Raw Access ====================

    [[nodiscard]] lv_anim_timeline_t* get() noexcept { return m_timeline; }
    [[nodiscard]] const lv_anim_timeline_t* get() const noexcept { return m_timeline; }
    explicit operator bool() const noexcept { return m_timeline != nullptr; }

    // ==================== Configuration ====================

    /// Add an animation at a given start time (ms offset from timeline start)
    AnimTimeline& add(uint32_t start_time, const Anim& anim) noexcept {
        lv_anim_timeline_add(m_timeline, start_time, anim.get());
        return *this;
    }

    /// Add a raw lv_anim_t at a given start time
    AnimTimeline& add(uint32_t start_time, const lv_anim_t* anim) noexcept {
        lv_anim_timeline_add(m_timeline, start_time, anim);
        return *this;
    }

    /// Set initial delay before timeline starts
    AnimTimeline& delay(uint32_t ms) noexcept {
        lv_anim_timeline_set_delay(m_timeline, ms);
        return *this;
    }

    /// Enable or disable reverse playback
    AnimTimeline& reverse(bool en) noexcept {
        lv_anim_timeline_set_reverse(m_timeline, en);
        return *this;
    }

    /// Set repeat count (0 = play once, LV_ANIM_REPEAT_INFINITE for forever)
    AnimTimeline& repeat(uint32_t count) noexcept {
        lv_anim_timeline_set_repeat_count(m_timeline, count);
        return *this;
    }

    /// Repeat forever
    AnimTimeline& repeat_infinite() noexcept {
        lv_anim_timeline_set_repeat_count(m_timeline, LV_ANIM_REPEAT_INFINITE);
        return *this;
    }

    /// Set delay between repeats
    AnimTimeline& repeat_delay(uint32_t ms) noexcept {
        lv_anim_timeline_set_repeat_delay(m_timeline, ms);
        return *this;
    }

    /// Scrub to a progress value (0 to LV_ANIM_TIMELINE_PROGRESS_MAX)
    AnimTimeline& progress(uint16_t value) noexcept {
        lv_anim_timeline_set_progress(m_timeline, value);
        return *this;
    }

    /// Set user data
    AnimTimeline& user_data(void* data) noexcept {
        lv_anim_timeline_set_user_data(m_timeline, data);
        return *this;
    }

    // ==================== Actions ====================

    /// Start playback (returns total duration in ms)
    uint32_t start() noexcept {
        return lv_anim_timeline_start(m_timeline);
    }

    /// Pause playback
    AnimTimeline& pause() noexcept {
        lv_anim_timeline_pause(m_timeline);
        return *this;
    }

    /// Merge another timeline into this one
    AnimTimeline& merge(const AnimTimeline& other, int32_t delay_ms = 0) noexcept {
        lv_anim_timeline_merge(m_timeline, other.m_timeline, delay_ms);
        return *this;
    }

    // ==================== Getters ====================

    /// Get elapsed playback time (ms)
    [[nodiscard]] uint32_t playtime() const noexcept {
        return lv_anim_timeline_get_playtime(m_timeline);
    }

    /// Check if reverse is enabled
    [[nodiscard]] bool is_reverse() const noexcept {
        return lv_anim_timeline_get_reverse(m_timeline);
    }

    /// Get current progress (0 to LV_ANIM_TIMELINE_PROGRESS_MAX)
    [[nodiscard]] uint16_t get_progress() const noexcept {
        return lv_anim_timeline_get_progress(m_timeline);
    }

    /// Get initial delay
    [[nodiscard]] uint32_t get_delay() const noexcept {
        return lv_anim_timeline_get_delay(m_timeline);
    }

    /// Get repeat count
    [[nodiscard]] uint32_t get_repeat_count() const noexcept {
        return lv_anim_timeline_get_repeat_count(m_timeline);
    }

    /// Get repeat delay
    [[nodiscard]] uint32_t get_repeat_delay() const noexcept {
        return lv_anim_timeline_get_repeat_delay(m_timeline);
    }

    /// Get user data
    [[nodiscard]] void* get_user_data() const noexcept {
        return lv_anim_timeline_get_user_data(m_timeline);
    }
};

} // namespace lv
