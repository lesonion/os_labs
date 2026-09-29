/*
 * Exercise on thread synchronization.
 *
 * Assume a half-duplex communication bus with limited capacity, measured in
 * tasks, and 2 priority levels:
 *
 * - tasks: A task signifies a unit of data communication over the bus
 *
 * - half-duplex: All tasks using the bus should have the same direction
 *
 * - limited capacity: There can be only 3 tasks using the bus at the same time.
 *                     In other words, the bus has only 3 slots.
 *
 *  - 2 priority levels: Priority tasks take precedence over non-priority tasks
 *
 *  Fill-in your code after the TODO comments
 */

#include <stdio.h>
#include <string.h>

#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "timer.h"

/* This is where the API for the condition variables is defined */
#include "threads/synch.h"

/* This is the API for random number generation.
 * Random numbers are used to simulate a task's transfer duration
 */
#include "lib/random.h"

#define MAX_NUM_OF_TASKS 200

#define BUS_CAPACITY 3

typedef enum {
  SEND,
  RECEIVE,

  NUM_OF_DIRECTIONS
} direction_t;

typedef enum {
  NORMAL,
  PRIORITY,

  NUM_OF_PRIORITIES
} priority_t;

typedef struct {
  direction_t direction;
  priority_t priority;
  unsigned long transfer_duration;
} task_t;

void init_bus (void);
void batch_scheduler (unsigned int num_priority_send,
                      unsigned int num_priority_receive,
                      unsigned int num_tasks_send,
                      unsigned int num_tasks_receive);

/* Thread function for running a task: Gets a slot, transfers data and finally
 * releases slot */
static void run_task (void *task_);

/* WARNING: This function may suspend the calling thread, depending on slot
 * availability */
static void get_slot (const task_t *task);

/* Simulates transfering of data */
static void transfer_data (const task_t *task);

/* Releases the slot */
static void release_slot (const task_t *task);

//-------------New code-------------
typedef enum { SEND, RECEIVE } direction_t;
struct semaphore sema;
struct lock bridge; 
direction_t current_direction;
direction_t turn;                   
/* initializes semaphores */ 
void init_bus(void){ 
    random_init((unsigned int)123456789); 
    sema_init(&sema, BUS_CAPACITY);
    
    current_direction = SEND; 
    turn = SEND;
    lock_init(&bridge);
// three locks available for the current direction. When the other side wants to aquire a lock
// A binary semaphore goes to 0 which makes it not possible for the current side to aquire the locks.
// When all locks are released then the other direction can aquire the locks. We need two waiting ques. 

}

void batch_scheduler (unsigned int num_priority_send,
                      unsigned int num_priority_receive,
                      unsigned int num_tasks_send,
                      unsigned int num_tasks_receive) {
  ASSERT (num_tasks_send + num_tasks_receive + num_priority_send +
             num_priority_receive <= MAX_NUM_OF_TASKS);

  static task_t tasks[MAX_NUM_OF_TASKS] = {0};

  char thread_name[32] = {0};

  unsigned long total_transfer_dur = 0;

  int j = 0;

  /* create priority sender threads */
  for (unsigned i = 0; i < num_priority_send; i++) {
    tasks[j].direction = SEND;
    tasks[j].priority = PRIORITY;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "sender-prio");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create priority receiver threads */
  for (unsigned i = 0; i < num_priority_receive; i++) {
    tasks[j].direction = RECEIVE;
    tasks[j].priority = PRIORITY;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "receiver-prio");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create normal sender threads */
  for (unsigned i = 0; i < num_tasks_send; i++) {
    tasks[j].direction = SEND;
    tasks[j].priority = NORMAL;
    tasks[j].transfer_duration = random_ulong () % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "sender");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create normal receiver threads */
  for (unsigned i = 0; i < num_tasks_receive; i++) {
    tasks[j].direction = RECEIVE;
    tasks[j].priority = NORMAL;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "receiver");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* Sleep until all tasks are complete */
  timer_sleep (2 * total_transfer_dur);
}

/* Thread function for the communication tasks */
void run_task(void *task_) {
  task_t *task = (task_t *)task_;

  get_slot (task);

  msg ("%s acquired slot", thread_name());
  transfer_data (task);

  release_slot (task);
}

static direction_t other_direction(direction_t this_direction) {
  return this_direction == SEND ? RECEIVE : SEND;
}

void get_slot (const task_t *task) {
  while(sema.value <= BUS_CAPACITY && task->direction != current_direction){
    if(){
      lock_try_acquire(&bridge);
    }
  }
  // not your turn, wait for the turn to be on your side
  if( task->direction != current_direction && task->direction != turn){
    timer_sleep(1);    
  }


  //if the task is on the turn side it can transefere data
  if(task->direction == turn){ 
    // if the task is on the current direction and there are available slots it can send data
    if(task->direction == current_direction && sema.value > 0){ 
      sema_down;
      
    }
    // if the task is on the other side and the buss is emptyit can transefere data
    else if(sema.value == BUS_CAPACITY && current_direction != task->direction){ 
      turn = other_direction(current_direction);
      sema_down;
      
    }
    // if the turn is not on the task side
    if(task->direction != turn){
      // if the bus is empty and the current direction is the same as the task direction it can transfer data
      if(sema.value == BUS_CAPACITY && current_direction == task->direction){
        //other side dont want to use the bus,
      }
      // if the bus is empty and the current direction is not the same as the task direction it can transfer data
      else if(sema.value == BUS_CAPACITY && current_direction != task->direction){
        
          lock_try_acquire(&bridge);
      }

    }
    lock_try_acquire(&bridge);{
      if(current_direction == task->direction && sema.value > 0){
        sema_down;
      }
      if(sema.value == BUS_CAPACITY && current_direction != task->direction){
        turn = other_ int direction(current_direction);
        current_direction = turn;
      }
    }
  }
  /* TODO: Try to get a slot, respect the following rules:
   *        1. There can be only BUS_CAPACITY tasks using the bus
   *        2. The bus is half-duplex: All tasks using the bus should be either
   * sending or receiving
   *        3. A normal task should not get the bus if there are priority tasks
   * waiting
   *
   * You do not need to guarantee fairness or freedom from starvation:
   * feel free to schedule priority tasks of the same direction,
   * even if there are priority tasks of the other direction waiting
   */
}

void transfer_data (const task_t *task) {
  /* Simulate bus send/receive */
  timer_sleep (task->transfer_duration);
}

void release_slot (const task_t *task) {
  sema_up;
  if(sema.value == BUS_CAPACITY){
    lock_release(&bridge);
  }
  /* TODO: Release the slot, think about the actions you need to perform:
   *       - Do you need to notify any waiting task?
   *       - Do you need to increment/decrement any counter?
   */
}
