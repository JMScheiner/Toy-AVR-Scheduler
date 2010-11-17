/**
 * @file scheduler.c
 * @brief Scheduler
 *
 * Implementation of functions for scheduler
 * Currently relies on avr-gcc's interrupt call convention.
 *
 * @author Justin Scheiner, Colony Project, CMU Robotics Club
 * Based on avrOS and 18348 Lab9 code
 **/

#include <avr/interrupt.h>
#include <util/delay.h>
#include "scheduler.h"

static uint8_t STACK[MAXTASKS][STACKSIZE];
static uint8_t nactive_tasks = 0;
static uint8_t current_task = 0; //Default to main.

//Internal functions 
static void task_terminate(void);

//Define some PCB's
typedef struct PCB_t
{
	uint32_t next;
	void (*exec)(void);
	uint16_t period; //Interval in clock cycles.
	uint8_t* sp;
	char running;
} PCB_t;

//PCB[MAXTASKS] is main.
PCB_t PCB[MAXTASKS + 1];

//Keep track of global time.
static uint32_t current_time = 0;

void scheduler_init() { 
	//Set the counter to 0.
	TCNT3 = 0;
	
	//Clear timer on compare (CTC) mode.
	TCCR3B |= _BV(WGM32);

	//Set a prescalar of 8. 8 / (8MHz) = 1us per tick.
	TCCR3B |= _BV(CS31);

	//Trigger an interrupt every 16th second for now.
	OCR3A = 0xF424;

	//Enable Output Compare A Interrupt. (Begins immediately).
  	ETIMSK |= _BV(OCIE3A);
   
   //Enable interrupts.
	sei();
}

unsigned int time_now() { 
	return current_time;
}

static void task_terminate(void) { 
	PCB[current_task].running = 0;
	yield();
}

void yield() {
	//TODO Actually implement.
	for(;;){}
}

int register_task(void (*exec)(void), uint16_t period)
{
	if(nactive_tasks >= MAXTASKS)
		return -1;
	
	PCB[nactive_tasks].exec = exec;
	PCB[nactive_tasks].period = period;
	
	//Block interrupts because 32 bit add/copy is not atomic.
	cli();
	PCB[nactive_tasks].next = current_time + period;
	sei();

	PCB[nactive_tasks].running = 0;	
	
	//Don't need to initialize SP, it will get done later.
	nactive_tasks++;
	return nactive_tasks - 1;
}

SIGNAL(TIMER3_COMPA_vect) {
	static uint8_t task_i = 1;
	volatile uint8_t* sp;
	uint8_t i;
	
	//Store sp.
	sp = (uint8_t*)&(PCB[current_task].sp);
	
	asm volatile( \
		"in r0, __SP_L__ \n" \
		"st %a0+, r0 \n" \
		"in r0, __SP_H__ \n" \
		"st %a0, r0 \n"  : : "e" (sp));
	
	//Keep track of the number of 1/16 second's that have passed.
	current_time++;
	
	/******** scheduler ********/

	//Loop over registered tasks, like in round robin order.
	for(i = nactive_tasks; i > 0; i--) { 
		//Launch a new task if it is due to run.
		if(	!PCB[task_i].running
		  && PCB[task_i].next <= current_time) { 
			
			current_task = task_i;	
			PCB[current_task].next += PCB[current_task].period;
			PCB[current_task].running = 1;
		
			task_i++;
			if(task_i >= nactive_tasks)
				task_i = 0;
			
			/***** create launch stack ***/

			sp = (void*)((uint16_t)&STACK[current_task][STACKSIZE - 1]);
			
			//Put task terminate  and the task to execute on the stack.
			*(sp--) = (uint8_t)(uint16_t)*task_terminate;
			*(sp--) = (uint8_t)((uint16_t)*task_terminate >> 8);
			*(sp--) = (uint8_t)(uint16_t)PCB[current_task].exec;
			*(sp--) = (uint8_t)((uint16_t)PCB[current_task].exec >> 8);
			
			//The current state of the registers is what the tasks will
			//see on entry (shouldn't matter).
			asm volatile( \
				"out __SP_L__, %A0 \n" \
				"out __SP_H__, %B0 \n" : : "d" (sp));

	 		/* start process and enable interrupts */
	 		asm volatile("reti \n");
			break;
		}
		
		//Continue an old task.
		if(PCB[task_i].running == 1) {
			current_task = task_i;
			task_i++;
			if(task_i >= nactive_tasks) 
				task_i = 0;
			break;
		}
		
		task_i++;
		if(task_i >= nactive_tasks) 
			task_i = 0;
	}
	
	//If no task was selected to run, go to main
	if(i == 0)
		current_task = MAXTASKS;	
		
	//Reset the stack pointer.
	sp = PCB[current_task].sp;
	asm volatile( \
		"out __SP_L__, %A0 \n" \
		"out __SP_H__, %B0 \n" : : "d" (sp));
		
	//Set the running task.
	PCB[current_task].running = 1;
	
	//avr-gcc inserts all of the necessary pops and the reti here.
}

