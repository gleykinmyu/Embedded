#pragma once

#include "board_pins.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class RelayBoard {
public:
    struct Snapshot {
        bool busy;
        bool chase;
        bool on[RELAY_COUNT];
    };

    void begin();
    bool set(size_t ch, bool on);
    bool is_on(size_t ch);
    bool all_on();
    bool all_off();
    bool chase_start();
    bool chase_stop();
    Snapshot snapshot();
    void print();

private:
    class Lock {
    public:
        explicit Lock(SemaphoreHandle_t mu, TickType_t ticks = pdMS_TO_TICKS(200));
        ~Lock();
        explicit operator bool() const { return ok_; }
        Lock(const Lock &) = delete;
        Lock &operator=(const Lock &) = delete;

    private:
        SemaphoreHandle_t mu_;
        bool ok_;
    };

    bool tca_write(uint8_t reg, uint8_t value);
    void coil_on(size_t ch);
    void coil_off(size_t ch);
    void coils_off();
    bool coil_is_on(size_t ch) const;
    bool di_active(size_t ch) const;
    bool channel_is_on(size_t ch) const;
    bool pulse_to(bool want_on);
    void chase_kick();
    void chase_loop();
    static void chase_task(void *arg);

    i2c_master_dev_handle_t tca_{};
    SemaphoreHandle_t mu_{};
    TaskHandle_t chase_task_{};
    uint8_t shadow_{};
    volatile bool pulse_busy_{};
    size_t chase_ch_{};
    bool chase_running_{};
};
