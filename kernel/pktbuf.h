#pragma once

#include <stddef.h>
#include <stdint.h>

/*
複数のバッファにまたがる際はnext_fragでつなげる。
totalはそれらのサイズも含めた合計サイズ。
受信パケットに関しては、単一のバッファで扱う。
送信パケットに関しては全ヘッダを格納できる十分なバッファを用意する。
ペイロードを格納できるだけの空きがあれば、そこにコピー。
足りなければ、next_fragにつなげる。
ヘッダは先頭バッファに格納することを想定しているので、バッファをまたぐadd_headerなどの操作はできない。
*/

struct pktbuf_head {
  struct pktbuf_head *next_frag;
  struct list_head pkt_link;

  uint32_t total;
  uint32_t size;
  uint8_t *head;
  uint8_t *data;
  uint8_t *tail;
  uint8_t *end;

  void (*freefunc)(uint8_t *);
};

struct pktbuf_head *pktbuf_create(uint8_t *buf, uint32_t size, void (*freefunc)(uint8_t *));
struct pktbuf_head *pktbuf_alloc(uint32_t size);
void pktbuf_free(struct pktbuf_head *head);
int pktbuf_reserve(struct pktbuf_head *head, uint32_t size);
uint8_t *pktbuf_add_header(uint32_t size);
void pktbuf_remove_header(uint32_t size);
int pktbuf_write_fragment(struct pktbuf_head *head, uint8_t *buf, uint32_t size);
void pktbuf_add_fragment(struct pktbuf_head *head, struct pktbuf_head *frag);
int pktbuf_is_nonlinear(struct pktbuf_head *head);

