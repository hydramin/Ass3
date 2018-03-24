/*
 * alarm_cond.c
 *
 * This is an enhancement to the alarm_mutex.c program, which
 * used only a mutex to synchronize access to the shared alarm
 * list. This version adds a condition variable. The alarm
 * thread waits on this condition variable, with a timeout that
 * corresponds to the earliest timer request. If the main thread
 * enters an earlier timeout, it signals the condition variable
 * so that the alarm thread will wake up and process the earlier
 * timeout first, requeueing the later request.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <semaphore.h>

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */

#define DEBUG 1
#define DEGUG 2


typedef struct alarm_tag {
  struct alarm_tag    *link;
  int                 seconds;
  time_t              time;
  char                message[128];
  int                 number;
  int                 type;           /* type of message*/
  int                 isAssigned;     /* whether the alarm is assigned to a thread or not (is 1 or 0)*/
} alarm_t;

alarm_t *alarm_list = NULL;
time_t current_alarm = 0;

/*semaphores declared here*/
sem_t readCountAccess;
sem_t alarmListAccess;
int readCount=0;

/*Messages to be displayed when there is an error or there is any status update*/
const char msg_3[] = "Replaced";
const char msg_4[] = "Alarm doesn't exist";
/*a method that prints the error message*/
void display_msg(const char *msg) {
  printf("%s\n", msg);
}

/*
 * Insert alarm entry on list, in order.
 WRITER FUNCTION
 */
void * alarm_insert (void * arg){
    int status;
    alarm_t **last, *next, *alarm = (alarm_t *) arg;

    /*
     * LOCKING PROTOCOL:
     *The alarm list is locked before being written to
     */
    sem_wait(&alarmListAccess); /*lock*/
    last = &alarm_list;
    next = *last;
    while (next != NULL) {
        if (next->number > alarm->number) {
            alarm->link = next;
            *last = alarm;
            break;
        } else if (next->number == alarm->number){
            alarm->link = next->link;
            *last = alarm;
            free(next);
            display_msg(msg_3);
            break;
        }
        last = &next->link;
        next = next->link;
    }
    /*
     * If we reached the end of the list, insert the new alarm
     * there.  ("next" is NULL, and "last" points to the link
     * field of the last item, or to the list header.)
     */
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }
    sem_post(&alarmListAccess); /*unlock*/

    #ifdef DEBUG
                printf ("[list: \n");
                for (next = alarm_list; next != NULL; next = next->link)
                    printf ("N : %d, S : %d, Ty : %d, Ti : %ld, Msg : %s \n", next->number,
                    next->seconds, next->type, next->time, next->message);
                printf ("]\n");
    #endif

    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */

}

/*
 * The alarm thread's start routine.
 READER FUNCTION
 */
// void *alarm_thread (void *arg){
//     alarm_t *alarm;
//     struct timespec cond_time;
//     time_t now;
//     int status, expired;
//
//     /*
//      * Loop forever, processing commands. The alarm thread will
//      * be disintegrated when the process exits. Lock the mutex
//      * at the start -- it will be unlocked during condition
//      * waits, so the main thread can insert alarms.
//      */
//     status = pthread_mutex_lock (&alarm_mutex);
//     if (status != 0)
//         err_abort (status, "Lock mutex");
//     while (1) {
//         /*
//          * If the alarm list is empty, wait until an alarm is
//          * added. Setting current_alarm to 0 informs the insert
//          * routine that the thread is not busy.
//          */
//         current_alarm = 0;
//         while (alarm_list == NULL) {
//             status = pthread_cond_wait (&alarm_cond, &alarm_mutex);
//             if (status != 0)
//                 err_abort (status, "Wait on cond");
//             }
//         alarm = alarm_list;
//         alarm_list = alarm->link;
//         now = time (NULL);
//         expired = 0;
//         if (alarm->time > now) {
// #ifdef DEBUG
//             printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
//                 alarm->time - time (NULL), alarm->message);
// #endif
//
//     }
// }
void * periodic_display_threads(void * args){
  alarm_t *alarm;
    time_t now;
    int status, expired;

    while (1) {

    }
}

/*The alarm thread function allows the createion of periodic_display_threads
  This is a reader method, it doesn't modify the alarm_list. It simply reads
  through and assigns an alarm to a thread.*/
void * alarm_thread (void *arg){
  alarm_t *next;
  int *message_type = (int*) arg;
  /*
  - lock the semaphore as a reader
  - search through the list and check if a request with the message_type
    exists.
  - if it exists, create a thread; if it doesn't exits display error*/

  sem_wait(&readCountAccess);
        readCount++;
        if(readCount==1) {
            sem_wait(&alarmListAccess);
        }
  sem_post(&readCountAccess);
  /*<><><><><><><><><><><><><><><><><><><><><><><><><>*/
  /*find an unassigned alarm and assign it to a thread*/
  for (next = alarm_list; next != NULL; next = next->link){
      if (next->type == *message_type && next->isAssigned == 0){
          /*entering if block means it has found there is an alarm to
            be processed. So create a periodic_display_threads to service it*/

          printf("Found and item, Type : %d , Number : %d\n",next->type,next->number);
      }

  }

  /*<><><><><><><><><><><><><><><><><><><><><><><><><>*/
  sem_wait(&readCountAccess);
        readCount--;
        /*the last thread releases the database for writting*/
        if(readCount==0){
            sem_post(&alarmListAccess);
        }
  sem_post(&readCountAccess);

}

int main (int argc, char *argv[]){
    /*initialize the semaphores*/
    sem_init(&readCountAccess,0,1);
    sem_init(&alarmListAccess,0,1);

    /*local variables*/
    int status;
    char line[1500];
    char tempS[1000];
    alarm_t *alarm;

    /*thread creation variable*/
    pthread_t writer_thread;

    while (1) {
        printf ("Alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;

        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> INPUT PARSING BLOCK
        /*
         * Parse input line into seconds (%d) and a message
         * (%128[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.

         Alarm> Time Message(Message_Type, Message_Number) Message
         Alarm> Create_Thread: MessageType(Message_Type)
         Alarm> Cancel: Message(Message_Number)
         */
         int err_t1, err_t2, err_t3;
         int t1_sec, t1_type, t1_num;
         char t1_msg[128];
         int t2_type, t3_num;

         err_t1 = sscanf(line,"%d Message(%d, %d) %1000[^\n]",&t1_sec,&t1_type,&t1_num,tempS);
         strncpy(t1_msg, tempS, 128);
         err_t2 = sscanf(line, "Create_Thread: MessageType(%d)",&t2_type);
         err_t3 = sscanf(line, "Cancle: Message(%d)",&t3_num);
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> INPUT VALIDATION BLOCK

         /*Check if there is any error in the commands entered,
        if error exists, restart the loop*/
        if (err_t1 < 4 && err_t2 < 1 && err_t3 < 1) {
            printf("Bad Command. Usage: \nType A: <+ve integer> Message(Message_Type : <+ve integer>, Message_Number : <+ve integer>) <string message> \nType B: Create_Thread: MessageType(Message_Type : <+ve integer>) \nType C: Cancle: Message(Message_Number : <+ve integer>)\n");
            continue;
        }
        /*if alarm request command, thread creation and thread termination commands are
         correct, check if the seconds and/or type of message have non negative values,
          if negative values entered display error message and restart the loop*/
        if(err_t1 == 3){
            if (t1_sec <= 0 || t1_type <= 0 || t1_num <= 0){
                printf("Bad Command. Usage: \nType A: <+ve integer> MessageType(<+ve integer>) <string message> \nType B: Create_Thread: MessageType(<+ve integer>) \nType C: Terminate_Thread: MessageType(<+ve integer>)\n");
                continue;
            }
        }
        else if(err_t2 == 1){
            if (t2_type <= 0){
                printf("Bad Command. Usage: \nType A: <+ve integer> MessageType(<+ve integer>) <string message> \nType B: Create_Thread: MessageType(<+ve integer>) \nType C: Terminate_Thread: MessageType(<+ve integer>)\n");
                continue;
            }
        }
        else if(err_t3 == 1){
            if (t3_num <= 0){
                printf("Bad Command. Usage: \nType A: <+ve integer> MessageType(<+ve integer>) <string message> \nType B: Create_Thread: MessageType(<+ve integer>) \nType C: Terminate_Thread: MessageType(<+ve integer>)\n");
                continue;
            }
        }

// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> ALARM SETTING/INPUT BLOCK
/*1==>*/if(err_t1 == 4){
            alarm = (alarm_t*) malloc (sizeof (alarm_t));
            if (alarm == NULL)
                errno_abort ("Allocate alarm");

            /*parse a Type A command and assign the element of the alarm*/
            alarm->seconds = t1_sec;
            alarm->type = t1_type;
            alarm->number = t1_num;
            strncpy(alarm->message, t1_msg, 128);
            alarm->time = time (NULL) + t1_sec;
            alarm->isAssigned = 0;

            /*call the writer thread to write to save the alarm created into the alarm thread*/

            status = pthread_create (&writer_thread, NULL, alarm_insert, alarm);
            if (status != 0)
                err_abort (status, "Insert alarm into alarm list");
  #ifdef DEGUG
              pthread_join(writer_thread,NULL);
  #endif

/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> CREATE THREAD BLOCK*/
/*2==>*/} else if (err_t2 == 1){
            status = pthread_create (&writer_thread, NULL, alarm_thread, &t2_type);
            if (status != 0)
                err_abort (status, "Create alarm thread");
  #ifdef DEGUG
              pthread_join(writer_thread,NULL);
  #endif
/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> TERMINATION BLOCK*/
/*3==>*/}else if (err_t3 == 1){
      }
    }
}
