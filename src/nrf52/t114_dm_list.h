// Heltec T114: store for direct messages to the own call that were not read yet. The 6 page ring
// of sendDisplayText() is overwritten by every message of the channel; a direct message stays here
// until the user has paged to it with the button. Unread messages are shown first, a marker
// ("DM2") shows in the status line. RAM only. Whole file is compiled for BOARD_HELTEC_T114 only.
#pragma once

#if defined(BOARD_HELTEC_T114)

#include "aprs_structures.h"

void t114DmStore(struct aprsMessage &aprsmsg);   // called for every message shown; keeps direct messages only
int  t114DmUnread();
void t114DmMarker(char *cstatus);                // cstatus: buffer of at least 10 characters, status line start

bool t114DmPageActive();
bool t114DmPageOpen();       // oldest unread message first; marks it read; false if there is none
bool t114DmPageNext();       // next unread message; false if there is none left (the caller leaves the page)
void t114DmPageClose();      // leaves the page state only, the caller redraws what comes next
void t114DmPageRedraw();     // full redraw (e.g. after the display was woken)
void t114DmPageUpdate();     // once per second while the page is active (age in the header)

#endif
