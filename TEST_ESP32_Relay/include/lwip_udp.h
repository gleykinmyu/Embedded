#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "iudp.hpp"
#include "lwip/udp.h"

/**
 * BIF::NET::IUdp через lwIP raw UDP + очередь указателей (без большого стека tcpip).
 * recvFrom не блокирует: 0 — очередь пуста.
 */
class LwipUdp : public BIF::NET::IUdp {
public:
    LwipUdp() = default;
    LwipUdp(const LwipUdp &) = delete;
    LwipUdp &operator=(const LwipUdp &) = delete;
    ~LwipUdp() override { close(); }

    bool open(uint16_t localPort) override;
    void close() override;
    [[nodiscard]] bool isOpen() const override { return pcb_ != nullptr; }

    bool sendTo(const uint8_t *data, std::size_t n, BIF::NET::Endpoint dest) override;
    std::size_t recvFrom(uint8_t *buf, std::size_t maxN, BIF::NET::Endpoint &src) override;

    bool joinGroup(BIF::NET::Ipv4 group) override;
    bool leaveGroup(BIF::NET::Ipv4 group) override;
    [[nodiscard]] std::size_t available() const override;

    [[nodiscard]] uint32_t rx_count() const { return rx_count_; }
    [[nodiscard]] uint32_t drop_count() const { return drop_count_; }

private:
    static constexpr std::size_t kMaxDatagram = 576;
    static constexpr UBaseType_t kQueueLen = 8;

    struct Pkt {
        BIF::NET::Ipv4 ip;
        uint16_t port;
        uint16_t len;
        uint8_t data[kMaxDatagram];
    };

    static void on_recv(void *arg, udp_pcb *pcb, pbuf *p, const ip_addr_t *addr, u16_t port);
    void drain();

    udp_pcb *pcb_{};
    QueueHandle_t q_{};
    volatile uint32_t rx_count_{};
    volatile uint32_t drop_count_{};
};
