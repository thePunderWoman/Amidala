// wcb_rx_queue.h
// Fixed-size single-producer/single-consumer ring buffer for handing mesh
// commands from WCB_Client's onCommand callback (runs on the ESP-NOW/WiFi
// receive task, per the library's own docs) to the main loop task
// (AmidalaController::animate()). The library explicitly warns callbacks
// must do minimal work and defer everything else -- this is that deferral
// point. Safe for exactly one producer and one consumer; not safe for
// multiple of either.
#pragma once

#include <atomic>
#include <stdint.h>
#include <string.h>

#define WCB_RX_QUEUE_DEPTH 8
// Matches WCB_Client's own long-unicast fragmentation ceiling (199 chars) + NUL.
#define WCB_RX_CMD_LEN 200

struct WCBRxMessage {
    uint8_t senderID;
    char    command[WCB_RX_CMD_LEN];
};

class WCBRxQueue {
public:
    // Called from the ESP-NOW callback. Never blocks; silently drops the
    // message if the queue is full rather than stalling the WiFi task.
    void push(uint8_t senderID, const char *command) {
        uint8_t head     = fHead.load(std::memory_order_relaxed);
        uint8_t nextHead = (head + 1) % WCB_RX_QUEUE_DEPTH;
        if (nextHead == fTail.load(std::memory_order_acquire)) return; // full, drop
        fSlots[head].senderID = senderID;
        strncpy(fSlots[head].command, command, WCB_RX_CMD_LEN - 1);
        fSlots[head].command[WCB_RX_CMD_LEN - 1] = '\0';
        fHead.store(nextHead, std::memory_order_release);
    }

    // Called from animate(). Returns true and fills *out if a message was
    // available, false if the queue was empty.
    bool pop(WCBRxMessage *out) {
        uint8_t tail = fTail.load(std::memory_order_relaxed);
        if (tail == fHead.load(std::memory_order_acquire)) return false; // empty
        *out = fSlots[tail];
        fTail.store((tail + 1) % WCB_RX_QUEUE_DEPTH, std::memory_order_release);
        return true;
    }

private:
    WCBRxMessage         fSlots[WCB_RX_QUEUE_DEPTH];
    std::atomic<uint8_t> fHead{0};
    std::atomic<uint8_t> fTail{0};
};
