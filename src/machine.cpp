#include "machine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Snapshot guarded by a lightweight spinlock; critical sections are just a
// struct copy so they are extremely short.
static portMUX_TYPE   s_mux = portMUX_INITIALIZER_UNLOCKED;
static MachineSnapshot s_snap;

static QueueHandle_t  s_cmdQ = nullptr;   // web -> machine
static QueueHandle_t  s_logQ = nullptr;   // machine -> web

void machine_init()
{
    s_cmdQ = xQueueCreate(24, sizeof(MachineCommand));
    s_logQ = xQueueCreate(64, sizeof(MachineLogLine));
}

// ---------------------------------------------------------------------------
//  Commands
// ---------------------------------------------------------------------------
bool machine_post_command(const MachineCommand &cmd)
{
    if (!s_cmdQ) return false;
    return xQueueSend(s_cmdQ, &cmd, 0) == pdTRUE;
}

bool machine_poll_command(MachineCommand &out)
{
    if (!s_cmdQ) return false;
    return xQueueReceive(s_cmdQ, &out, 0) == pdTRUE;
}

// ---------------------------------------------------------------------------
//  Snapshot
// ---------------------------------------------------------------------------
void machine_publish(const MachineSnapshot &snap)
{
    portENTER_CRITICAL(&s_mux);
    s_snap = snap;
    portEXIT_CRITICAL(&s_mux);
}

void machine_get(MachineSnapshot &out)
{
    portENTER_CRITICAL(&s_mux);
    out = s_snap;
    portEXIT_CRITICAL(&s_mux);
}

// ---------------------------------------------------------------------------
//  Log
// ---------------------------------------------------------------------------
void machine_log_push(bool outgoing, const char *s)
{
    if (!s_logQ) return;
    MachineLogLine l;
    l.outgoing = outgoing;
    strncpy(l.s, s, sizeof(l.s) - 1);
    l.s[sizeof(l.s) - 1] = '\0';
    // Drop the oldest line if the web consumer is falling behind.
    if (xQueueSend(s_logQ, &l, 0) != pdTRUE) {
        MachineLogLine drop;
        xQueueReceive(s_logQ, &drop, 0);
        xQueueSend(s_logQ, &l, 0);
    }
}

bool machine_log_pop(MachineLogLine &out)
{
    if (!s_logQ) return false;
    return xQueueReceive(s_logQ, &out, 0) == pdTRUE;
}
