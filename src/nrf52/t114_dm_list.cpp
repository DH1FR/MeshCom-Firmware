// Heltec T114: unread direct messages. See t114_dm_list.h.

#if defined(BOARD_HELTEC_T114)

#include <Arduino.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "configuration.h"
#include "loop_functions.h"
#include "loop_functions_extern.h"
#include "text_fold.h"
#include "t114_dm_list.h"

extern Adafruit_ST7789 tft1;   // nrf52_main.cpp
extern bool bSetDisplay;       // loop_functions.cpp, not in a header

#define C_WHITE  0xFFFF
#define C_BLACK  0x0000
#define C_RED    0xF800

#define DM_MAX   6
#define DM_TEXT  200

struct DmEntry
{
    bool     used;
    bool     unread;
    char     from[MC_CALL_LEN_Z];
    char     text[DM_TEXT];     // folded to ASCII, the display font has no umlauts
    uint32_t heardMs;
};

static DmEntry g_dm[DM_MAX];
static bool    g_active = false;
static int     g_cur = -1;      // entry shown
static int     g_pos = 0;       // 1 = first message shown since the page was opened

// ------------------------------------------------------------------ storage

void t114DmStore(struct aprsMessage &aprsmsg)
{
    if(strcmp(aprsmsg.msg_destination_call, meshcom_settings.node_call) != 0)
        return;   // not a direct message to us
    if(aprsmsg.msg_payload[0] == '{' || aprsmsg.msg_payload[0] == 0x00)
        return;   // {ping}, {pong}, {MCP}: no text for the user

    // free slot; else the oldest read one; else the oldest unread one
    int slot = -1;
    for(int i = 0; i < DM_MAX; i++)
        if(!g_dm[i].used) { slot = i; break; }
    if(slot < 0)
        for(int i = 0; i < DM_MAX; i++)
            if(!g_dm[i].unread && (slot < 0 || (int32_t)(g_dm[i].heardMs - g_dm[slot].heardMs) < 0)) slot = i;
    if(slot < 0)
    {
        slot = 0;
        for(int i = 1; i < DM_MAX; i++)
            if((int32_t)(g_dm[i].heardMs - g_dm[slot].heardMs) < 0) slot = i;
    }

    DmEntry &e = g_dm[slot];
    e.used = true;
    e.unread = true;
    snprintf(e.from, sizeof(e.from), "%s", aprsmsg.msg_source_call);
    text_fold(aprsmsg.msg_payload, strnlen(aprsmsg.msg_payload, MC_PAYLOAD_LEN), e.text, sizeof(e.text), TF_ASCII);
    e.heardMs = millis();
}

int t114DmUnread()
{
    int n = 0;
    for(int i = 0; i < DM_MAX; i++)
        if(g_dm[i].used && g_dm[i].unread) n++;
    return n;
}

// oldest unread entry, -1 if none
static int oldestUnread()
{
    int best = -1;
    for(int i = 0; i < DM_MAX; i++)
        if(g_dm[i].used && g_dm[i].unread && (best < 0 || (int32_t)(g_dm[i].heardMs - g_dm[best].heardMs) < 0)) best = i;
    return best;
}

// "DM2" in the status line: next to G/T when one of them is shown, else instead of the version
void t114DmMarker(char *cstatus)
{
    int n = t114DmUnread();
    if(n <= 0)
        return;
    if(n > 9) n = 9;

    size_t l = strlen(cstatus);
    while(l < 6)
        cstatus[l++] = ' ';
    cstatus[6] = 0x00;

    if(cstatus[0] == 'G' || cstatus[1] == 'T')
    {
        cstatus[3] = 'D';
        cstatus[4] = (char)('0' + n);
    }
    else
        snprintf(cstatus, 10, "DM%d   ", n);
}

// ------------------------------------------------------------------ drawing

// opaque text, so the old text needs no clearing
static void putText(int16_t x, int16_t y, uint8_t size, uint16_t fg, uint16_t bg, const char *text)
{
    tft1.setTextSize(size);
    tft1.setTextColor(fg, bg);
    tft1.setCursor(x, y);
    tft1.print(text);
}

static void formatAge(uint32_t heardMs, char *out, size_t len)
{
    uint32_t s = (millis() - heardMs) / 1000;
    if(s < 60)         snprintf(out, len, "%lus", (unsigned long)s);
    else if(s < 3600)  snprintf(out, len, "%lumin", (unsigned long)(s / 60));
    else               snprintf(out, len, "%luh", (unsigned long)(s / 3600));
}

static void drawHeader(DmEntry *e)
{
    char buf[24];
    tft1.fillRect(0, 0, tft1.width(), 30, C_RED);
    snprintf(buf, sizeof(buf), "DM %d/%d", g_pos, g_pos + t114DmUnread());
    putText(4, 6, 2, C_WHITE, C_RED, buf);

    char age[12];
    formatAge(e->heardMs, age, sizeof(age));
    snprintf(buf, sizeof(buf), "%8s", age);
    putText(tft1.width() - 4 - 8 * 12, 6, 2, C_WHITE, C_RED, buf);   // size 2: 12 px per character
}

static void drawBody(DmEntry *e)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%-19s", e->from);
    putText(4, 36, 2, C_BLACK, C_WHITE, buf);

    // text: size 2 (19 characters per line, 4 lines) up to 76 characters, else size 1 (38 per line, 9 lines)
    size_t len = strlen(e->text);
    uint8_t size = (len <= 76) ? 2 : 1;
    int width = (size == 2) ? 19 : 38;
    int maxLines = (size == 2) ? 4 : 9;
    int16_t y0 = (size == 2) ? 58 : 52;
    int16_t dy = (size == 2) ? 20 : 9;

    size_t pos = 0;
    for(int line = 0; line < maxLines; line++)
    {
        char row[48];
        memset(row, ' ', sizeof(row));
        row[width] = 0x00;

        while(pos < len && e->text[pos] == ' ')
            pos++;

        size_t take = 0;
        if(pos < len)
        {
            size_t rest = len - pos;
            take = (rest <= (size_t)width) ? rest : (size_t)width;
            if(rest > (size_t)width)
            {
                // break at the last space of the line if there is one
                for(size_t k = take; k > 0; k--)
                    if(e->text[pos + k] == ' ') { take = k; break; }
            }
            memcpy(row, e->text + pos, take);
        }
        pos += take;

        putText(4, y0 + line * dy, size, C_BLACK, C_WHITE, row);
    }
}

// ------------------------------------------------------------------ page control

bool t114DmPageActive()
{
    return g_active;
}

void t114DmPageRedraw()
{
    if(g_cur < 0 || !g_dm[g_cur].used)
        return;

    bSetDisplay = true;
    tft1.fillScreen(C_WHITE);
    drawHeader(&g_dm[g_cur]);
    drawBody(&g_dm[g_cur]);
    bSetDisplay = false;
}

static void show(int idx)
{
    g_cur = idx;
    g_dm[idx].unread = false;   // shown by a click: counts as read
    g_active = true;
    iDisplayType = 0;           // sendDisplayTime() leaves the header of this page alone
    pageHold = 60;              // seconds, then the normal page logic takes over again
    t114DmPageRedraw();
}

bool t114DmPageOpen()
{
    int idx = oldestUnread();
    if(idx < 0)
        return false;

    g_pos = 1;
    show(idx);
    return true;
}

bool t114DmPageNext()
{
    if(!g_active)
        return false;

    int idx = oldestUnread();
    if(idx < 0)
        return false;

    g_pos++;
    show(idx);
    return true;
}

void t114DmPageClose()
{
    g_active = false;
}

void t114DmPageUpdate()
{
    if(!g_active || bDisplayIsOff || bSetDisplay || g_cur < 0)
        return;

    bSetDisplay = true;
    drawHeader(&g_dm[g_cur]);   // the age runs; the rest of the page does not change
    bSetDisplay = false;
}

#endif
