/**
 * @file scheduler.h
 * @brief Contains scheduler declarations and functions
 *
 * Contains functions and definitions to implement a timer-based
 * scheduler and register tasks with the scheduler.
 *
 * @author Justin Scheiner, Colony Project, CMU Robotics Club
 *
 **/

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#define STACKSIZE 128
#define MAXTASKS 4

#include <stdint.h>

void scheduler_init(void);
void yield(void);

int register_task(void (*exec)(void), uint16_t period);
unsigned int time_now(void);

#endif
