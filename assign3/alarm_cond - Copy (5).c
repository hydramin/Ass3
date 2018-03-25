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

/*A data structure that holds information about a thread and the
type of alarm request it manages*/
typedef struct thread_data_structure {
    struct thread_data_structure     *link;
    pthread_t 			                 thread;
    int				                       type;
    int                              flag; /* used to terminate the while loop in the thread so the thread exits safely*/
} thread_ds;

alarm_t *alarm_list = NULL;
thread_ds *thread_list = NULL;
time_t current_alarm = 0;

/*semaphores for alarm_list declared here*/
sem_t readCountAccess;
sem_t alarmListAccess;
int readCount=0;

/*semaphores for thread_list*/
sem_t t_readCountAccess;
sem_t t_threadListAccess;
int t_readCount=0;

/*function definitions*/
void save_thread(int msg_type, pthread_t prt_thread);
void remove_alarm_request(int msg_number);
void remove_thread_info(int msg_type);
void * alarm_thread (void *arg);
void * alarm_insert (void * arg);
void * periodic_display_threads(void * args);
void prt_thread_list();
void prt_alarm_list();
void alarm_reader_semaphore_lock();
void alarm_reader_semaphore_release();


/*
 * Insert alarm entry on list, in order of Message_Number.
 WRITER FUNCTION
 */
void prt_alarm_list(){
  /*assumes alarm list has been locked before call*/
  #ifdef DEBUG
    printf ("[list: \n");
    alarm_t *next;
    for (next = alarm_list; next != NULL; next = next->link)
        printf ("N : %d, S : %d, Ty : %d, Ti : %ld, Msg : %s \n",
        next->number, next->seconds, next->type, next->time, next->message);
    printf ("]\n");
  #endif
}

void prt_thread_list(){
  /*assumes thread list has been locked before call*/
  #ifdef DEGUG
          thread_ds *s;
          for(s = thread_list; s != NULL; s = s->link)
              printf("List of Threads: \nType: %d -- Thread Id: %ld\n",s->type,s->thread);
  #endif
}

void remove_thread_info(int msg_type){
  thread_ds *temp, *prev;
  /* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> THREAD_LIST ELEMENT REMOVAL*/
          /*lock the mutex for the thread_list and remove the current
          thread from the list of threads because it is terminated*/

          /*Remove the thread from thread_list after
          it is terminated, after the while loop exits.
          finally, free the memory space held by it.*/

          sem_wait(&t_threadListAccess); /*lock*/

          /* thread_ds *temp = thread_list, *prev;*/
          temp = thread_list;
          if (temp != NULL && pthread_equal(temp->thread,tid_l)){
              thread_list = temp->link;
              free(temp);
          } else {
              while(temp != NULL && !pthread_equal(temp->thread,tid_l)){
                  prev = temp;
                  temp = temp->link;
              }

              if(temp == NULL){
                sem_post(&t_threadListAccess);
              }else{
                prev->link = temp->link;
                free(temp);
              }
          }
          sem_post(&t_threadListAccess);
  /* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> END OF THREAD CODE*/
}

void * alarm_insert (void * arg){
    int status;
    alarm_t **last, *next, *alarm = (alarm_t *) arg;
    int is_replaced = 0;

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
            /*if on insertion, the currently pointed alarm is being printed
            and if next->type and alarm->type are different printed
            message*/
            printf("assigned --> %d new type --> %d current type --> %d\n",next->isAssigned,alarm->type,next->type );
            if (next->isAssigned /*&& alarm->type != next->type*/){
              printf("Stopped Displaying Replaced Alarm With Message Type (%d) at <%ld>:<Type A>\n",
                      alarm->type,time(NULL));
            } else {
              is_replaced = 1;
              printf("Type A Replacement Alarm Request With Message Number (%d) Inserted Into Alarm List at <%ld>: <Type A>\n",
                      alarm->number,time(NULL));
              /*Type A Replacement Alarm Request With Message Number (Message_Number)
              Inserted Into Alarm List at <time>: <alarm_request>*/
            }

            alarm->link = next->link;
            *last = alarm;
            free(next);
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
    if(!is_replaced)
      printf("Type A Alarm Request With Message Number (%d) Inserted Into Alarm List at <%ld>: <Type A>\n",
      alarm->number,time(NULL));

      prt_alarm_list();

    sem_post(&alarmListAccess); /*unlock*/


}

/*WRITER FUNCTION*/
void remove_alarm_request(int msg_number){
  sem_wait(&alarmListAccess); /*lock*/

  /*Remove all alarms with the specified type*/
  /* loop until all type=x alarm requests are removed*/
  while(1){
      alarm_t *temp = alarm_list, *prev;
      /*If found at first remove it there*/
      if (temp != NULL && temp->number == msg_number){
          alarm_list = temp->link;
          free(temp);
          continue;
      } else {
          /*If alarm is not at first, then iterate through and find it*/
          while(temp != NULL && temp->number != msg_number){
              prev = temp;
              temp = temp->link;
          }

          /*if all the specified alarms are removed exit the infinite loop*/
          if(temp == NULL){
              sem_post(&alarmListAccess); /*unlock*/
              break;
          }
          prev->link = temp->link;
          free(temp);
      }
  }
  prt_alarm_list();
}

void alarm_reader_semaphore_lock(){
  sem_wait(&readCountAccess);
        readCount++;
        if(readCount==1) {
            sem_wait(&alarmListAccess);
        }
  sem_post(&readCountAccess);
}

void alarm_reader_semaphore_release(){
  sem_wait(&readCountAccess);
        readCount--;
        /*the last thread releases the database for writting*/
        if(readCount==0){
            sem_post(&alarmListAccess);
        }
  sem_post(&readCountAccess);
}

void thread_reader_semaphore_lock(){
  sem_wait(&t_readCountAccess);
        t_readCount++;
        if(t_readCount==1) {
            sem_wait(&t_threadListAccess);
        }
  sem_post(&t_readCountAccess);
}

void thread_reader_semaphore_release(){
  sem_wait(&t_readCountAccess);
        t_readCount--;
        /*the last thread releases the database for writting*/
        if(t_readCount==0){
            sem_post(&t_threadListAccess);
        }
  sem_post(&t_readCountAccess);
}

void * periodic_display_threads(void * args){
  alarm_t *alarm, *next;
  time_t now;
  int status;
  int message_type = (int) *((int *) args);
  int request_count;
  printf("Type B Alarm Request Processed at <%ld>: New Periodic Display Thread For Message Type (%d) Created. \n",
          time(NULL),message_type);

  /* READER loop that reads through the alarm list and prints the relevant
  alarms*/
    while (1) {
      request_count = 0;
      alarm_reader_semaphore_lock();
      /*<><><><><><><><><><><><><><><><><><><><><><><><><>*/
      /*loop through the alarm list and print the alarms with the specified
      message types*/
  #ifdef DEBUG
      for (next = alarm_list; next != NULL; next = next->link){
          if (next->type == message_type /*&& next->isAssigned == 1*/ && next->time >= time(NULL)){
              request_count++;
              // printf("Printing message, Type : %d , Number : %d , Msg : %s , Tim : %ld\n",
              // next->type,next->number, next->message, (next->time - time(NULL)));
              printf("Alarm With Message Type (%d) and Message Number (%d) Displayed at <%ld>: <Type B>\n",
                    next->type, next->number, time(NULL));
          }
      }
  #endif
      /*<><><><><><><><><><><><><><><><><><><><><><><><><>*/
      /*the lock on the alarm list is freed if all readers are done*/
      alarm_reader_semaphore_release();
      if (request_count)
        break;
      sleep(1);
    }
    printf("=<>=<>=<>=<>= PRINTING THREAD EXITED =<>=<>=<>=<>=\n");
}

/*returns 1 if thread exists and 0 if it doesn't*/
int check_thread_existance(int msg_type){
  thread_ds *s;
  int does_exist = 0;
  thread_reader_semaphore_lock();
  /*lock <<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>*/
  for(s = thread_list; s != NULL; s = s->link){
    if(s->type == msg_type){
      does_exist = 1;
    }
  }
  /*unlock <<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>*/
  thread_reader_semaphore_release();
  return does_exist;
}

/*The alarm thread function allows the createion of periodic_display_threads
  This is a reader method, it doesn't modify the alarm_list. It simply reads
  through and assigns an alarm to a thread.*/


void * alarm_thread (void *arg){
  alarm_t *next;
  pthread_t print_thread;
  int status, *message_type =  (int*) arg ;
  /*the specified message_type (int value) exists = 1, doesn't exist = 0
   by default it is assumed to not exist, unless proved.*/
  int type_exists = 0;
  /*
  - lock the semaphore as a reader
  - check if thread has alredy been created
  - search through the list and check if a request with the message_type
    exists.
  - if it exists, create a thread; if it doesn't exits display error*/

  alarm_reader_semaphore_lock();

  /*<><><><><><><><><><><><><><><><><><><><><><><><><>*/
  /*find an unassigned alarm and assign it to a thread it the message_type
  exists in the alarm_list*/

  if (!check_thread_existance(*message_type)) {
    /*thread doesn't exist*/
    for (next = alarm_list; next != NULL; next = next->link){
        if (next->type == *message_type /*&& next->isAssigned == 0*/){
            /*entering if block means it has found there is an alarm to
              be processed. So create a periodic_display_threads to service it*/
            type_exists = 1;
            next->isAssigned = 1;
            printf("Found and item, Type : %d , Number : %d\n",next->type,next->number);
        }
    }
  } else {
    type_exists = 2; /*if thread already exists*/
    printf("Error: More Than One Type B Alarm Request With Message Type (%d)!\n",*message_type );
  }
  /*<><><><><><><><><><><><><><><><><><><><><><><><><>*/
  /*the lock on the alarm list is freed if all readers are done*/

alarm_reader_semaphore_release();

  /*based on the results from the loop above, decide the next step*/
  if (type_exists == 1) {
    /*Type A exists*/
    status = pthread_create(&print_thread,NULL,periodic_display_threads,message_type);
    if (status != 0)
      err_abort (status, "periodic_display_threads not created!\n");
    printf("Type B Create Thread Alarm Request For Message Type (%d) Inserted Into Alarm List at <%ld>!\n",
            message_type,time(NULL));
    save_thread((int) *message_type, print_thread);
  } else if (type_exists == 0) {
      /*Type A message doesn't exist*/
      printf("Type B Alarm Request Error: No Alarm Request With Message Type (%d)!",*message_type);
  }

}

/*WRITER METHOD TO ADD THREAD INFO INTO thread_list*/
void save_thread(int msg_type, pthread_t prt_thread){
  sem_wait(&t_threadListAccess); /*lock*/
  if(thread_list == NULL) {
          thread_list = (thread_ds*) malloc(sizeof(thread_ds));
          thread_list->type = msg_type;
          thread_list->thread = prt_thread;
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
      next_2->type = msg_type;
      next_2->thread = prt_thread;
      next_2->flag = 0;
      next_2->link = NULL;
  }
  sem_post(&t_threadListAccess); /*unlock*/
}

int main (int argc, char *argv[]){
    /*initialize the semaphores*/
    sem_init(&readCountAccess,0,1);
    sem_init(&alarmListAccess,0,1);
    sem_init(&t_threadListAccess,0,1);
    sem_init(&t_readCountAccess,0,1);

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
         err_t3 = sscanf(line, "Cancel: Message(%d)",&t3_num);
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
          prt_thread_list();
          remove_alarm_request(t3_num);
        }
    }
}
