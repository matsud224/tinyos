#pragma once

struct etheraddr {
  u8 addr[6];
};

void ether_rx(void *_devno);
