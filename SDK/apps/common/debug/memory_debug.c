#include "system/timer.h"
#include "system/init.h"
#include "malloc.h"

extern const u32 CONFIG_HEAP_MEMORY_TRACE;


void memory_debug_timer(void *p)
{
    memory_trace_dump();
}


int memory_debug_init()
{
    if (CONFIG_HEAP_MEMORY_TRACE == 0) {
        return 0;
    }
    sys_timer_add(NULL, memory_debug_timer, 1000);
    return 0;
}
late_initcall(memory_debug_init);

