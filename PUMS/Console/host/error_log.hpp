/**
 * @file error_log.hpp
 * @brief Кольцевой журнал ошибок/событий для lab / node.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace smcp {
namespace host {

class ErrorLog {
public:
    static constexpr std::size_t kCap = 256u;

    void push(uint8_t node_id, const char* kind, const char* detail) noexcept
    {
        std::lock_guard<std::mutex> lock(_mu);
        Entry e{};
        e.node_id = node_id;
        std::snprintf(e.kind, sizeof(e.kind), "%s", kind != nullptr ? kind : "?");
        std::snprintf(e.text, sizeof(e.text), "%s", detail != nullptr ? detail : "");
        if (_items.size() >= kCap) {
            _items.erase(_items.begin());
        }
        _items.push_back(e);
        std::fprintf(stderr, "[ERR] node=0x%02X %s: %s\n",
            static_cast<unsigned>(node_id), e.kind, e.text);
    }

    void dump() const
    {
        std::lock_guard<std::mutex> lock(_mu);
        if (_items.empty()) {
            std::printf("(no errors)\n");
            return;
        }
        for (const Entry& e : _items) {
            std::printf("  0x%02X  %-12s  %s\n",
                static_cast<unsigned>(e.node_id), e.kind, e.text);
        }
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(_mu);
        _items.clear();
    }

    [[nodiscard]] std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(_mu);
        return _items.size();
    }

    /** kind совпал; @a sub — подстрока text (nullptr = любой text). */
    [[nodiscard]] bool has(uint8_t node_id, const char* kind,
                           const char* sub = nullptr) const
    {
        std::lock_guard<std::mutex> lock(_mu);
        for (const Entry& e : _items) {
            if (e.node_id != node_id) {
                continue;
            }
            if (std::strcmp(e.kind, kind != nullptr ? kind : "") != 0) {
                continue;
            }
            if (sub == nullptr || std::strstr(e.text, sub) != nullptr) {
                return true;
            }
        }
        return false;
    }

private:
    struct Entry {
        uint8_t node_id = 0;
        char kind[16]{};
        char text[160]{};
    };

    mutable std::mutex _mu;
    std::vector<Entry> _items;
};

} // namespace host
} // namespace smcp
