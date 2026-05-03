#include <stdint.h>
#include "task.h"
#include "scheduler.h"

uint32_t systemcall_table[255];
uint8_t pos = 0;


int
terminal_putchar(char c){
    uint16_t* t_buffer = (uint16_t*)0xb8000;
    t_buffer[pos] = (uint16_t)c | (uint16_t) 15 << 8;
    pos++;

    return 1;
}

int
sys_print(char *data, int data_length)
{
    int ret = 0;
    for ( int i = 0; i < data_length; i++ )
        ret += terminal_putchar((int) ((const unsigned char*) data)[i]);
    return ret;
}

void
sys_create_task(char *name, void (*entry)(void *), void *arg)
{
    task_create(name, entry, arg);
}

void
sys_exit(void)
{
    task_exit();
}

void
sys_yield(void)
{
    task_yield();
}

void
syscall_init(void)
{
	systemcall_table[1] = (uint32_t)sys_print;
	systemcall_table[2] = (uint32_t)sys_create_task;
	systemcall_table[3] = (uint32_t)sys_exit;
	systemcall_table[4] = (uint32_t)sys_yield;
}
