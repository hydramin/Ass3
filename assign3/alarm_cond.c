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

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */

#define DEBUG 1

typedef struct alarm_tag {
  struct alarm_tag    *link;
  int                 seconds;
  time_t              time;
  char                message[128];
  int                 number;
  int                 type;           /* type of message*/
  int                 isAssigned;     /* whether the alarm is assigned to a thread or not (is 1 or 0)*/
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
alarm_t *alarm_list = NULL;
time_t current_alarm = 0;

/*
 * Insert alarm entry on list, in order.
 WRITER FUNCTION
 */
void alarm_insert (alarm_t *alarm)
{
    int status;
    alarm_t **last, *next;

    /*
     * LOCKING PROTOCOL:
     *
     * This routine requires that the caller have locked the
     * alarm_mutex!
     */
    last = &alarm_list;
    next = *last;
    while (next != NULL) {
        if (next->time >= alarm->time) {
            alarm->link = next;
            *last = alarm;
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
#ifdef DEBUG
    printf ("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf ("%d(%d)[\"%s\"] ", next->time,
            next->time - time (NULL), next->message);
    printf ("]\n");
#endif
    /*
     * Wake the alarm thread if it is not busy (that is, if
     * current_alarm is 0, signifying that it's waiting for
     * work), or if the new alarm comes before the one on
     * which the alarm thread is waiting.
     */
    if (current_alarm == 0 || alarm->time < current_alarm) {
        current_alarm = alarm->time;
        status = pthread_cond_signal (&alarm_cond);
        if (status != 0)
            err_abort (status, "Signal cond");
    }
}

/*
 * The alarm thread's start routine.
 READER FUNCTION
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    struct timespec cond_time;
    time_t now;
    int status, expired;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits. Lock the mutex
     * at the start -- it will be unlocked during condition
     * waits, so the main thread can insert alarms.
     */
    status = pthread_mutex_lock (&alarm_mutex);
    if (status != 0)
        err_abort (status, "Lock mutex");
    while (1) {
        /*
         * If the alarm list is empty, wait until an alarm is
         * added. Setting current_alarm to 0 informs the insert
         * routine that the thread is not busy.
         */
        current_alarm = 0;
        while (alarm_list == NULL) {
            status = pthread_cond_wait (&alarm_cond, &alarm_mutex);
            if (status != 0)
                err_abort (status, "Wait on cond");
            }
        alarm = alarm_list;
        alarm_list = alarm->link;
        now = time (NULL);
        expired = 0;
        if (alarm->time > now) {
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                alarm->time - time (NULL), alarm->message);
#endif
            cond_time.tv_sec = alarm->time;
            cond_time.tv_nsec = 0;
            current_alarm = alarm->time;
            while (current_alarm == alarm->time) {
                status = pthread_cond_timedwait (
                    &alarm_cond, &alarm_mutex, &cond_time);
                if (status == ETIMEDOUT) {
                    expired = 1;
                    break;
                }
                if (status != 0)
                    err_abort (status, "Cond timedwait");
            }
            if (!expired)
                alarm_insert (alarm);
        } else
            expired = 1;
        if (expired) {
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            free (alarm);
        }
    }
}

int main (int argc, char *argv[])
{
    int status;
    char line[1000];
    alarm_t *alarm;
    pthread_t thread;

    status = pthread_create (&thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");

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

         err_t1 = sscanf(line,"%d Message(%d, %d) %128[^\n]",&t1_sec,&t1_type,&t1_num,t1_msg);
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
            if (t1_sec <= 0 || t1_type < 0 || t1_num < 0){
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
            alarm = (alarm_t*)malloc (sizeof (alarm_t));

            if (alarm == NULL)
                errno_abort ("Allocate alarm");
            /*parse a Type A command and assign the element of the alarm*/
            if (sscanf(line, "%d Message(%d, %d) %1000[^\n]", &alarm->seconds,&alarm->type,&alarm->number,tempS) < 3){
                fprintf (stderr, "Bad command, Not Type A\n");
                free (alarm);
                continue;
            } else {

        strncpy(alarm->message, tempS, 128);
                /*Lock mutex for the alarm list to insert the alarm into the list*/
                status = pthread_mutex_lock (&alarm_mutex);
                if (status != 0)
                    err_abort (status, "Lock mutex");

                alarm->time = time (NULL) + alarm->seconds;
                alarm->isAssigned = 0;


                printf("Alarm Request With Message Type (%d) Inserted by Main Thread <%ld> Into Alarm List at <%ld>: <Type A>\n",t1_type,tmp,time(NULL));
                /*
                * Insert the new alarm into the list of alarms,
                * sorted by expiration time.
                */
                last = &alarm_list;
                next = *last;
                /*Sorting by message type as opposed to message time.
                */
                    while (next != NULL) {
                        if (next->type >= alarm->type) {
                            alarm->link = next;
                            *last = alarm;
                            break;
                        }
                        last = &next->link;
                        next = next->link;
                    }
                /*
                * If we reached the end of the list, insert the new
                * alarm there. ("next" is NULL, and "last" points
                * to the link field of the last item, or to the
                * list header).
                */
                if (next == NULL) {
                    *last = alarm;
                    alarm->link = NULL;
                }
    #ifdef DEBUG
                printf ("[list: \n");
                for (next = alarm_list; next != NULL; next = next->link)
                    printf ("%d(%d)[\"%s\"] isAssigned = %d type = %d \n", next->time,
                        next->time - time (NULL), next->message, next->isAssigned, next->type);
                printf ("]\n");
    #endif
                status = pthread_mutex_unlock (&alarm_mutex);
                if (status != 0)
                    err_abort (status, "Unlock mutex");
            }
/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> CREATE THREAD BLOCK*/
/*2==>*/} else if (err_t2 == 1){
            if ((err_t2 = sscanf(line, "Create_Thread: MessageType(%d)",&t2_type)) < 1){
                fprintf (stderr, "Bad command, Not Type B command\n");
                continue;
            }
            /*create the alarm thread once the parsing is done correctly*/

            status = pthread_create (&thread, NULL, alarm_thread, &t2_type);
            if (status != 0)
                err_abort (status, "Create alarm thread");

            printf("New Alarm Thread <%ld> For Message Type (%d) Created at <%ld>:<Type B>\n",thread,t2_type,time(NULL));

            /*Once the thread is created successfully enter the thread and the type of alarm
            it manages into the thread_ds linked list structure*/

            status = pthread_mutex_lock (&thread_list_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex for thread_list_create thread");

            if(thread_list == NULL) {
                    thread_list = (thread_ds*) malloc(sizeof(thread_ds));
                    thread_list->type = t2_type;
                    thread_list->thread = thread;
                    /*a 0 flag means the termination status of the created thread is false*/
                    thread_list->flag = 0;
                    thread_list->link = NULL;
            }
            else {
                thread_ds *next_2 = thread_list;
                while(next_2->link != NULL){
                    next_2 = next_2->link;
                }
                next_2->link = (thread_ds*) malloc(sizeof (thread_ds));
                next_2 = next_2->link;
                next_2->type = t2_type;
                next_2->thread = thread;
                next_2->flag = 0;
                next_2->link = NULL;
            }

            status = pthread_mutex_unlock (&thread_list_mutex);
            if (status != 0)
                err_abort (status, "Release lock mutex for thread_list_create_thread\n");
/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> TERMINATION BLOCK*/
/*3==>*/}else if (err_t3 == 1){
            thread_ds *s;
            /*access and lock the thread list, */
            status = pthread_mutex_lock (&thread_list_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex for thread_list");

#ifdef DEBUG
        for(s = thread_list; s != NULL; s = s->link)
            printf("List of Threads: \nType: %d -- Thread Id: %ld\n",s->type,s->thread);
#endif
            /*loop through the thread list and set the flag of the threads that
            have the type specified in the terminate command. A flag of 1 means the
            termination status of the threads is true, and so they will terminate*/

            for(s = thread_list; s != NULL; s = s->link){
                if(s->type == t3_type){
                    s->flag = 1;
                }
            }
            status = pthread_mutex_unlock (&thread_list_mutex);
            if (status != 0)
                err_abort (status, "Release lock mutex for thread_list\n");

/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
            /*lock the alarm_list mutex and remove all the alarms that have the
            specified type from the alarm list after the threads are terminated*/

            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock alarm_list mutex Terminate_Thread");
            /*Remove all alarms with the specified type*/
            /* loop until all type=x alarm requests are removed*/
            while(1){
                alarm_t *temp = alarm_list, *prev;
                if (temp != NULL && temp->type == t3_type){
                    alarm_list = temp->link;
                    free(temp);
                    continue;
                } else {
                    while(temp != NULL && temp->type != t3_type){
                        prev = temp;
                        temp = temp->link;
                    }

                    /*if all the specified alarms are removed exit the infinite loop*/
                    if(temp == NULL){
                        status = pthread_mutex_unlock (&alarm_mutex);
                        if (status != 0)
                            err_abort (status, "Release lock mutex for thread_list");
                        break;
                    }
                    prev->link = temp->link;
                    free(temp);
                }
            }

            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock alarm_list mutex Terminate_Thread");
/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> End of Thread_Termination */

            printf("All Alarm Threads For Message Type (%d) Terminated And All Messages of Message Type Removed at <%ld>: <Type C>\n",t3_type,time(NULL));
        }
    }
}
