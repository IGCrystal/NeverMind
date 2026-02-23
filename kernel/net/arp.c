#include "nm/net.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void net_stats_note_arp_hit(void);
void net_stats_note_arp_miss(void);

#define ARP_CACHE_SIZE 32

struct arp_entry {
    bool used;
    uint32_t ip;
    uint8_t mac[6];
    uint64_t age;
};

static struct arp_entry cache[ARP_CACHE_SIZE];
static uint64_t age_tick;

static void copy_mac(uint8_t dst[6], const uint8_t src[6])
{
    for (int i = 0; i < 6; i++) {
        dst[i] = src[i];
    }
}

void arp_cache_add(uint32_t ip, const uint8_t mac[6])
{
    int slot = -1;
    uint64_t oldest = (uint64_t)-1;
    int oldest_idx = 0;
    age_tick++;

    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (cache[i].used && cache[i].ip == ip) {
            copy_mac(cache[i].mac, mac);
            cache[i].age = age_tick;
            return;
        }
        if (!cache[i].used && slot < 0) {
            slot = i;
        }
        if (cache[i].used && cache[i].age < oldest) {
            oldest = cache[i].age;
            oldest_idx = i;
        }
    }

    if (slot < 0) {
        slot = oldest_idx;
    }

    cache[slot].used = true;
    cache[slot].ip = ip;
    copy_mac(cache[slot].mac, mac);
    cache[slot].age = age_tick;
}

bool arp_cache_lookup(uint32_t ip, uint8_t mac_out[6])
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (cache[i].used && cache[i].ip == ip) {
            copy_mac(mac_out, cache[i].mac);
            net_stats_note_arp_hit();
            return true;
        }
    }
    net_stats_note_arp_miss();
    return false;
}

void arp_input(const uint8_t *packet, uint16_t len)
{
    if (packet == 0 || len < 28) {
        return;
    }

    uint8_t sha[6];
    uint32_t spa = ((uint32_t)packet[14] << 24) | ((uint32_t)packet[15] << 16) |
                   ((uint32_t)packet[16] << 8) | (uint32_t)packet[17];
    for (int i = 0; i < 6; i++) {
        sha[i] = packet[8 + i];
    }
    arp_cache_add(spa, sha);
}
