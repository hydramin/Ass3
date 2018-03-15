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
#include<semaphore.h>
#include "errors.h"

 #define DEBUG  1 // UNCOMMENT TO ALLOW DEBUGGIN PRINT

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {    
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;           /*seconds from EPOCH*/
    char                message[128];
    int                 type;           /*Message type*/
    int                 number;           /*Message number*/
    /*int                 isAssigned;     // whether the alarm is assigned to a thread or not (is 1 or 0)*/

} alarm_t;

sem_t reader_count_mutex;
sem_t write_mutex;
int reader_count=0;

void *Reader(void *arg);
void *Writer(void *arg);

alarm_t *alarm_list = NULL;
time_t current_alarm = 0;

/*
 * Insert alarm entry on list, in order.
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
        if (next->number >= alarm->number) {
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
        // printf ("%d(%d)[\"%s\"] ", next->time,next->time - time (NULL), next->message);
        printf ("Num-> %d ; Type-> %d ; Time-> %d ; Msg-> %s \n", next->number,next->type,next->time - time (NULL), next->message);
    // printf ("]\n");
#endif

}

// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> ALARM THREAD FUNCTION
/*
 * The alarm thread's start routine.
 */
// void *alarm_thread (void *arg)
// {
//     alarm_t *alarm;    
//     time_t now;
//     int status, expired;

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
//             cond_time.tv_sec = alarm->time;
//             cond_time.tv_nsec = 0;
//             current_alarm = alarm->time;
//             while (current_alarm == alarm->time) {
//                 status = pthread_cond_timedwait (
//                     &alarm_cond, &alarm_mutex, &cond_time);
//                 if (status == ETIMEDOUT) {
//                     expired = 1;
//                     break;
//                 }
//                 if (status != 0)
//                     err_abort (status, "Cond timedwait");
//             }
//             if (!expired)
//                 alarm_insert (alarm);
//         } else
//             expired = 1;
//         if (expired) {
//             printf ("(%d) %s\n", alarm->seconds, alarm->message);
//             free (alarm);
//         }
//     }
// }

// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> MAIN METHOD
int main (int argc, char *argv[])
{    
    /*Initialize semaphores*/
    sem_init(&reader_count_mutex,0,1);
    sem_init(&write_mutex,0,1);

    int status;
    char line[1000];
    alarm_t *alarm;
    pthread_t thread;

    /*status = pthread_create (&thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");*/

    while (1) {
        printf ("Alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        

// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> INPUT PARSING BLOCK
        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.

         Alarm> Time Message(Message_Type, Message_Number) Message
         Alarm> Create_Thread: MessageType(Message_Type)
         Alarm> Cancel: Message(Message_Number)

         */
        int err_t1, err_t2, err_t3;
        int t1_sec, t1_type, t1_num;
        char t1_msg[128];
        int t2_type, t3_num;
        /*parse the input string into the componenents required and use the if 
        block below to check validity of the values entered*/
        err_t1 = sscanf(line,"%d Message(%d, %d) %128[^\n]",&t1_sec,&t1_type,&t1_num,t1_msg);
        err_t2 = sscanf(line, "Create_Thread: MessageType(%d)",&t2_type);
        err_t3 = sscanf(line, "Cancel: Message(%d)",&t3_num);
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> INPUT VALIDATION BLOCK
        /*Check if there is any error in the commands entered, 
        if error exists, restart the loop*/        
        if (err_t1 < 4 && err_t2 < 1 && err_t3 < 1) {            
            printf("Bad Command. Usage: \nType A: <+ve integer> Message(<+ve integer>, <+ve integer>) <string message> \nType B: Create_Thread: MessageType(<+ve integer>) \nType C: Calcel: Message(<+ve integer>)\n");
            continue;
        } 
        /*if alarm request command, thread creation and thread termination commands are
         correct, check if the seconds and/or type of message have non negative values,
          if negative values entered display error message and restart the loop*/
        if(err_t1 == 4){            
            if (t1_sec <= 0 || t1_type < 0 || t1_num < 0){
                printf("Bad Command. Usage: \nType A: <+ve integer> Message(<+ve integer>, <+ve integer>) <string message> \nType B: Create_Thread: MessageType(<+ve integer>) \nType C: Calcel: Message(<+ve integer>)\n");
                continue;
            }
        }
        else if(err_t2 == 1){
            if (t2_type <= 0){
                printf("Bad Command. Usage: \nType A: <+ve integer> Message(<+ve integer>, <+ve integer>) <string message> \nType B: Create_Thread: MessageType(<+ve integer>) \nType C: Calcel: Message(<+ve integer>)\n");
                continue;
            }
        }
        else if(err_t3 == 1){
            if (t3_num <= 0){                
                printf("Bad Command. Usage: \nType A: <+ve integer> Message(<+ve integer>, <+ve integer>) <string message> \nType B: Create_Thread: MessageType(<+ve integer>) \nType C: Calcel: Message(<+ve integer>)\n");
                continue;
            }
        }

// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> ALARM SETTING/INPUT BLOCK
/*1==>*/if(err_t1 == 4){

            alarm = (alarm_t*)malloc (sizeof (alarm_t));             
            if (alarm == NULL)
                errno_abort ("Allocate alarm");                    

            /*parse a Type A command and assign the element of the alarm*/
            if (sscanf(line,"%d Message(%d, %d) %128[^\n]", &alarm->seconds,&alarm->type,&alarm->number,alarm->message) < 4){        
                fprintf (stderr, "Bad command, Not Type A\n");
                free (alarm);
                continue;
            } else {
                /*set the Type A alarm and put it in the list
                create the alarm and pass it to be added into the
                alarm_list*/
                alarm->time = time(NULL) + alarm->seconds;
                
                status = sem_wait(&write_mutex);
                if (status != 0)
                    err_abort (status, "Lock mutex");
                
                /*
                * Insert the new alarm into the list of alarms,
                * sorted by message type.
                */
                alarm_insert (alarm);
                status = sem_post(&write_mutex);
                if (status != 0)
                    err_abort (status, "Unlock mutex");
            }     

        } else if (err_t2 == 1){

        } else if(err_t3 == 1){

        }
    }
}
