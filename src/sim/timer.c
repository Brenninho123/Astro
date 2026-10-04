/* Simulated timer: derived from the Windows tick counter. */
#include <windows.h>

#include "sim.h"
#include "timer.h"

static ULONGLONG start_ms;

void timer_init(void)
{
    start_ms = GetTickCount64();
}

uint32_t timer_ticks(void)
{
    return (uint32_t)((GetTickCount64() - start_ms) / (1000 / TIMER_HZ));
}

void timer_sleep_ms(uint32_t ms)
{
    sim_present();
    Sleep(ms);
}
