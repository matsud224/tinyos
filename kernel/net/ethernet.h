#pragma once
#include "protohdr.h"

void ethernet_initialize(void);
void ethernet_send(ether_flame *flm);
void ethernet_send(hdrstack *flm);
