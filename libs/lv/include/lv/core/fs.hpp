#pragma once

/**
 * @file fs.hpp
 * @brief RAII wrappers for LVGL filesystem API
 *
 * Provides File and Directory classes with automatic close-on-destruct.
 * Works with any registered LVGL filesystem driver (POSIX, STDIO, FATFS, etc.).
 *
 * @code
 * // Read a file
 * lv::fs::File f("A:/config.txt");
 * if (f) {
 *     char buf[256];
 *     uint32_t n;
 *     f.read(buf, sizeof(buf), &n);
 * }
 *
 * // List a directory
 * lv::fs::Directory dir("A:/assets");
 * if (dir) {
 *     char name[64];
 *     while (dir.read(name, sizeof(name)) == lv::fs::res::ok && name[0]) {
 *         // name starts with '/' for subdirectories
 *     }
 * }
 * @endcode
 */

#include <lvgl.h>
#include <cstring>

namespace lv::fs {

// ==================== Constants ====================

namespace mode {
    constexpr auto read       = LV_FS_MODE_RD;
    constexpr auto write      = LV_FS_MODE_WR;
    constexpr auto read_write = static_cast<lv_fs_mode_t>(LV_FS_MODE_RD | LV_FS_MODE_WR);
} // namespace mode

namespace seek {
    constexpr auto set = LV_FS_SEEK_SET;
    constexpr auto cur = LV_FS_SEEK_CUR;
    constexpr auto end = LV_FS_SEEK_END;
} // namespace seek

namespace res {
    constexpr auto ok          = LV_FS_RES_OK;
    constexpr auto hw_err      = LV_FS_RES_HW_ERR;
    constexpr auto fs_err      = LV_FS_RES_FS_ERR;
    constexpr auto not_found   = LV_FS_RES_NOT_EX;
    constexpr auto full        = LV_FS_RES_FULL;
    constexpr auto locked      = LV_FS_RES_LOCKED;
    constexpr auto denied      = LV_FS_RES_DENIED;
    constexpr auto busy        = LV_FS_RES_BUSY;
    constexpr auto timeout     = LV_FS_RES_TOUT;
    constexpr auto not_impl    = LV_FS_RES_NOT_IMP;
    constexpr auto out_of_mem  = LV_FS_RES_OUT_OF_MEM;
    constexpr auto inv_param   = LV_FS_RES_INV_PARAM;
    constexpr auto unknown     = LV_FS_RES_UNKNOWN;
} // namespace res

// ==================== File ====================

/**
 * @brief RAII file handle
 *
 * Wraps lv_fs_file_t with automatic close on destruction.
 * Move-only, embeds the C struct directly.
 */
class File {
    lv_fs_file_t m_file{};
    bool m_open = false;

public:
    /// Default constructor (closed state)
    File() noexcept = default;

    /// Open a file immediately
    explicit File(const char* path, lv_fs_mode_t m = LV_FS_MODE_RD) noexcept {
        m_open = (lv_fs_open(&m_file, path, m) == LV_FS_RES_OK);
    }

    ~File() {
        if (m_open) lv_fs_close(&m_file);
    }

    // Move-only
    File(File&& other) noexcept : m_file(other.m_file), m_open(other.m_open) {
        other.m_open = false;
        std::memset(&other.m_file, 0, sizeof(other.m_file));
    }

    File& operator=(File&& other) noexcept {
        if (this != &other) {
            if (m_open) lv_fs_close(&m_file);
            m_file = other.m_file;
            m_open = other.m_open;
            other.m_open = false;
            std::memset(&other.m_file, 0, sizeof(other.m_file));
        }
        return *this;
    }

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    // ==================== Open / Close ====================

    /// Open a file (closes any currently open file first)
    lv_fs_res_t open(const char* path, lv_fs_mode_t m = LV_FS_MODE_RD) noexcept {
        if (m_open) lv_fs_close(&m_file);
        lv_fs_res_t r = lv_fs_open(&m_file, path, m);
        m_open = (r == LV_FS_RES_OK);
        return r;
    }

    /// Close the file
    void close() noexcept {
        if (m_open) {
            lv_fs_close(&m_file);
            m_open = false;
        }
    }

    /// Check if file is open
    [[nodiscard]] bool is_open() const noexcept { return m_open; }
    explicit operator bool() const noexcept { return m_open; }

    // ==================== I/O ====================

    /// Read bytes from file
    lv_fs_res_t read(void* buf, uint32_t size, uint32_t* bytes_read = nullptr) noexcept {
        uint32_t br = 0;
        lv_fs_res_t r = lv_fs_read(&m_file, buf, size, &br);
        if (bytes_read) *bytes_read = br;
        return r;
    }

    /// Write bytes to file
    lv_fs_res_t write(const void* buf, uint32_t size, uint32_t* bytes_written = nullptr) noexcept {
        uint32_t bw = 0;
        lv_fs_res_t r = lv_fs_write(&m_file, buf, size, &bw);
        if (bytes_written) *bytes_written = bw;
        return r;
    }

    // ==================== Position ====================

    /// Seek to position
    lv_fs_res_t seek(uint32_t pos, lv_fs_whence_t w = LV_FS_SEEK_SET) noexcept {
        return lv_fs_seek(&m_file, pos, w);
    }

    /// Get current position (via out-param)
    lv_fs_res_t tell(uint32_t& pos) noexcept {
        return lv_fs_tell(&m_file, &pos);
    }

    /// Get current position (returns 0 on error)
    [[nodiscard]] uint32_t tell() noexcept {
        uint32_t pos = 0;
        lv_fs_tell(&m_file, &pos);
        return pos;
    }

    /// Get file size (returns 0 on error)
    [[nodiscard]] uint32_t size() noexcept {
        uint32_t s = 0;
        lv_fs_get_size(&m_file, &s);
        return s;
    }

    // ==================== Raw Access ====================

    [[nodiscard]] lv_fs_file_t* get() noexcept { return &m_file; }
    [[nodiscard]] const lv_fs_file_t* get() const noexcept { return &m_file; }
};

// ==================== Directory ====================

/**
 * @brief RAII directory handle
 *
 * Wraps lv_fs_dir_t with automatic close on destruction.
 * Move-only, embeds the C struct directly.
 */
class Directory {
    lv_fs_dir_t m_dir{};
    bool m_open = false;

public:
    /// Default constructor (closed state)
    Directory() noexcept = default;

    /// Open a directory immediately
    explicit Directory(const char* path) noexcept {
        m_open = (lv_fs_dir_open(&m_dir, path) == LV_FS_RES_OK);
    }

    ~Directory() {
        if (m_open) lv_fs_dir_close(&m_dir);
    }

    // Move-only
    Directory(Directory&& other) noexcept : m_dir(other.m_dir), m_open(other.m_open) {
        other.m_open = false;
        std::memset(&other.m_dir, 0, sizeof(other.m_dir));
    }

    Directory& operator=(Directory&& other) noexcept {
        if (this != &other) {
            if (m_open) lv_fs_dir_close(&m_dir);
            m_dir = other.m_dir;
            m_open = other.m_open;
            other.m_open = false;
            std::memset(&other.m_dir, 0, sizeof(other.m_dir));
        }
        return *this;
    }

    Directory(const Directory&) = delete;
    Directory& operator=(const Directory&) = delete;

    // ==================== Open / Close ====================

    /// Open a directory (closes any currently open one first)
    lv_fs_res_t open(const char* path) noexcept {
        if (m_open) lv_fs_dir_close(&m_dir);
        lv_fs_res_t r = lv_fs_dir_open(&m_dir, path);
        m_open = (r == LV_FS_RES_OK);
        return r;
    }

    /// Close the directory
    void close() noexcept {
        if (m_open) {
            lv_fs_dir_close(&m_dir);
            m_open = false;
        }
    }

    /// Check if directory is open
    [[nodiscard]] bool is_open() const noexcept { return m_open; }
    explicit operator bool() const noexcept { return m_open; }

    // ==================== Reading ====================

    /// Read next directory entry into buffer. Returns empty name at end of listing.
    lv_fs_res_t read(char* fn, uint32_t fn_len) noexcept {
        return lv_fs_dir_read(&m_dir, fn, fn_len);
    }

    // ==================== Raw Access ====================

    [[nodiscard]] lv_fs_dir_t* get() noexcept { return &m_dir; }
    [[nodiscard]] const lv_fs_dir_t* get() const noexcept { return &m_dir; }
};

// ==================== Free Functions ====================

/// Check if a filesystem drive is ready
inline bool is_ready(char letter) noexcept {
    return lv_fs_is_ready(letter);
}

/// Get all available drive letters into buf
inline void get_letters(char* buf) noexcept {
    lv_fs_get_letters(buf);
}

} // namespace lv::fs
