#include "lwip_udp.h"

#include <cstdlib>
#include <cstring>

#include "esp_heap_caps.h"
#include "lwip/igmp.h"
#include "lwip/ip.h"
#include "lwip/tcpip.h"

void LwipUdp::on_recv(void *arg, udp_pcb *, pbuf *p, const ip_addr_t *addr, u16_t port) {
    auto *self = static_cast<LwipUdp *>(arg);
    if (self == nullptr || self->q_ == nullptr || p == nullptr) {
        if (p) {
            pbuf_free(p);
        }
        return;
    }

    auto *pkt = static_cast<Pkt *>(heap_caps_malloc(sizeof(Pkt), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (pkt == nullptr) {
        self->drop_count_ = self->drop_count_ + 1u;
        pbuf_free(p);
        return;
    }

    pkt->port = port;
    pkt->ip = 0;
    if (addr != nullptr && IP_IS_V4(addr)) {
        pkt->ip = lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(addr)));
    }
    const u16_t n = pbuf_copy_partial(p, pkt->data, sizeof(pkt->data), 0);
    pkt->len = n;
    pbuf_free(p);

    self->rx_count_ = self->rx_count_ + 1u;
    if (xQueueSend(self->q_, &pkt, 0) != pdTRUE) {
        free(pkt);
        self->drop_count_ = self->drop_count_ + 1u;
    }
}

void LwipUdp::drain() {
    if (q_ == nullptr) {
        return;
    }
    Pkt *pkt = nullptr;
    while (xQueueReceive(q_, &pkt, 0) == pdTRUE) {
        free(pkt);
    }
}

bool LwipUdp::open(uint16_t localPort) {
    close();
    rx_count_ = 0;
    drop_count_ = 0;

    q_ = xQueueCreate(kQueueLen, sizeof(Pkt *));
    if (q_ == nullptr) {
        return false;
    }

    LOCK_TCPIP_CORE();
    pcb_ = udp_new();
    if (pcb_ == nullptr) {
        UNLOCK_TCPIP_CORE();
        vQueueDelete(q_);
        q_ = nullptr;
        return false;
    }
    ip_set_option(pcb_, SOF_BROADCAST);
    const err_t err = udp_bind(pcb_, IP_ADDR_ANY, localPort);
    if (err != ERR_OK) {
        udp_remove(pcb_);
        pcb_ = nullptr;
        UNLOCK_TCPIP_CORE();
        vQueueDelete(q_);
        q_ = nullptr;
        return false;
    }
    udp_recv(pcb_, &LwipUdp::on_recv, this);
    UNLOCK_TCPIP_CORE();
    return true;
}

void LwipUdp::close() {
    if (pcb_ != nullptr) {
        LOCK_TCPIP_CORE();
        udp_remove(pcb_);
        pcb_ = nullptr;
        UNLOCK_TCPIP_CORE();
    }
    drain();
    if (q_ != nullptr) {
        vQueueDelete(q_);
        q_ = nullptr;
    }
}

bool LwipUdp::sendTo(const uint8_t *data, std::size_t n, BIF::NET::Endpoint dest) {
    if (pcb_ == nullptr || data == nullptr || n == 0u || n > 0xFFFFu) {
        return false;
    }

    pbuf *p = pbuf_alloc(PBUF_TRANSPORT, static_cast<u16_t>(n), PBUF_RAM);
    if (p == nullptr) {
        return false;
    }
    if (pbuf_take(p, data, static_cast<u16_t>(n)) != ERR_OK) {
        pbuf_free(p);
        return false;
    }

    ip_addr_t ip{};
    ip_addr_set_ip4_u32(&ip, lwip_htonl(dest.ip));

    LOCK_TCPIP_CORE();
    const err_t err = udp_sendto(pcb_, p, &ip, dest.port);
    UNLOCK_TCPIP_CORE();
    pbuf_free(p);
    return err == ERR_OK;
}

std::size_t LwipUdp::recvFrom(uint8_t *buf, std::size_t maxN, BIF::NET::Endpoint &src) {
    if (q_ == nullptr || buf == nullptr || maxN == 0u) {
        return 0;
    }

    Pkt *pkt = nullptr;
    if (xQueueReceive(q_, &pkt, 0) != pdTRUE || pkt == nullptr) {
        return 0;
    }

    src.ip = pkt->ip;
    src.port = pkt->port;
    const std::size_t n = pkt->len < maxN ? pkt->len : maxN;
    std::memcpy(buf, pkt->data, n);
    free(pkt);
    return n;
}

bool LwipUdp::joinGroup(BIF::NET::Ipv4 group) {
    ip4_addr_t g{};
    ip4_addr_set_u32(&g, lwip_htonl(group));
    LOCK_TCPIP_CORE();
    const err_t err = igmp_joingroup(IP4_ADDR_ANY4, &g);
    UNLOCK_TCPIP_CORE();
    return err == ERR_OK;
}

bool LwipUdp::leaveGroup(BIF::NET::Ipv4 group) {
    ip4_addr_t g{};
    ip4_addr_set_u32(&g, lwip_htonl(group));
    LOCK_TCPIP_CORE();
    const err_t err = igmp_leavegroup(IP4_ADDR_ANY4, &g);
    UNLOCK_TCPIP_CORE();
    return err == ERR_OK;
}

std::size_t LwipUdp::available() const {
    if (q_ == nullptr) {
        return 0;
    }
    return static_cast<std::size_t>(uxQueueMessagesWaiting(q_));
}
