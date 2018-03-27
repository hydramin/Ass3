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



#define DEBUG 1
#define DEGUG 2

/*A linked list structure that holds the information about the Type A alarm requests*/
typedef struct alarm_tag {
  struct alarm_tag    *link;
  int                 seconds;
  time_t              time;
  char                message[128];
  int                 number;
  int                 type;           /* type of message*/
  int                 is_done;     /*if alarm time has expired is_done = 1, else = 0*/
} alarm_t;

/*A linked list structure that holds information about a thread and the
type of alarm request it manages*/
typedef struct thread_data_structure {
    struct thread_data_structure     *link;    
    int				                 type;
    int                              is_created;
    // int                              flag; /* used to terminate the while loop in the thread so the thread exits safely*/
} thread_ds;

/*a linked list pestructure that holds information abou the messages sheduled to be
deleted*/
typedef struct message_removal_data_structure {
    struct message_removal_data_structure     *link;
    int  			                           number;    
} removal_ds;

/*Initial instantiations of the linked lists*/
alarm_t *alarm_list = NULL;
thread_ds *thread_list = NULL;
removal_ds *removal_list = NULL;

/*semaphores for alarm_list declared here*/
sem_t readCountAccess;
sem_t alarmListAccess;
/*a counter for the number of readers currently reading the alarm_list*/
int   readCount=0;

/*semaphores for thread_list*/
sem_t t_readCountAccess;
sem_t t_threadListAccess;
/*a counter for the number of readers currently reading the thread_list*/
int   t_readCount=0;

/*semaphores for removal_list*/
sem_t r_readCountAccess;
sem_t r_threadListAccess;
/*a counter for the number of readers currently reading the removal_list*/
int   r_readCount=0;

/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>  FUNCTION DEFINITIONS*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*alarm_list function definitions, function details are found
 above the implementation*/

/* removes an alarm from the alarm_list. it takes parameters 
msg_number : the message number of the alarm
print_msg : prints 2 types of messages whether the print_msg=0 or
            less than zero*/ 
void remove_from_alarm_list(int msg_number, int print_msg);
/*adds Type A alarm requests to the alarm_list:
parameter: arg is an struct alarm_t*/
void * add_to_alarm_list (void * arg);

/*it prints the content of the alarm_list*/
void prt_alarm_list();

/*the semaphore lock for the reader functions*/
void alarm_reader_semaphore_lock();

/*the semaphore release for the reader functions*/
void alarm_reader_semaphore_release();

/*returns positive number if the alarm specified exists, 
else returns 0. parameters are:
msg_id: either message_number or message_type of the alarm_t element
type : specifies to the method whether to search alarm using message_number (1)
and message_type(0)*/
int alarm_exists(int msg_id, int type);

/*removes all alarms that are expired*/
void remove_alarms_that_are_done();

/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*thread_list function definitions*/

/*add a Type B request to create a periodic_print_thread into
thread_list, parameter is message_type*/
void * add_to_thread_list(void * args);

/*removes a thread info registered in the thread_list*/
void remove_from_thread_list(int msg_type);

/*prints information about all the currently running thread*/
void prt_thread_list();

/*readers' semaphore lock for thread_list*/
void thread_reader_semaphore_lock();

/*readers' semaphore unlock for thread_list*/
void thread_reader_semaphore_release();

/*checks the thread_list if an element with a 
message_type exists. returns positive number if thread
exists, else returns 0*/
int thread_exists(int msg_type);

/*it checks if the thread specified by the message_type
has any active alarms that it can work with, returns positive number
if true else returns 0*/
int thread_has_active_alarm(int msg_type);

/*it checks the thread list and creates threads for
the message_types specified in the thread_list*/
void check_thread_list_and_create_thread();

/*removes a thread from the threadlist if the thread
has no active alarms*/
void remove_threads_if_no_active_alarm();

/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*functions for editing removal_list*/

/*adds a Type C alarm request into the removal_list.
it takes in the message_number of the Type A alarm request*/
void add_to_removal_list(int msg_number);

/*removes a Type C request from the removal_list*/
void remove_from_removal_list(int msg_number);

/*returns positive number if the removal request exists
in the removal list*/
int remove_request_exists(int msg_number);

/*prints out all the Type C alarm requests*/
void prt_removal_list();

/*semaphore lock for the reader functions of the
removal list*/
void removal_reader_semaphore_lock();

/*semaphore unlock for the reader functions of the 
removal list*/
void removal_reader_semaphore_release();

/*removes all the Type A alarms in the removal list*/
void remove_alarms_in_removal_list();

/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*REQUIRED THREADS*/
/*it runs infinitely processing the specified actions in the
    assignment specification document*/
void * alarm_thread (void *arg);

/*it is a thread that prints out a Type A alarm reques
every second it is alive*/
void * periodic_display_threads(void * args);


/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>  TYPE A ALARM_LIST FUNCTIONS*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
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

void * add_to_alarm_list (void * arg){
    int status;
    alarm_t **last, *next, *alarm = (alarm_t *) arg;
    int is_replaced = 0;    
    int replaced_type;

    sem_wait(&alarmListAccess); /*lock*/    
        last = &alarm_list;
        next = *last;

        while (next != NULL) {
            if (next->number > alarm->number) {
                alarm->link = next;
                *last = alarm;
                break;
            } else if (next->number == alarm->number){
                /*if on insertion, the currently pointed alarm has same message_number
                as the new, replace the current by the new*/           
                is_replaced = 1;
                replaced_type = next->type;
                alarm->link = next->link;
                *last = alarm;
                free(next);
                printf("Type A Replacement Alarm Request With Message Number (%d) Inserted Into Alarm List at <%ld>: <Type A>\n",
                        alarm->number,time(NULL));
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
        if(!is_replaced){
            printf("Type A Alarm Request With Message Number (%d) Inserted Into Alarm List at <%ld>: <Type A>\n",
            alarm->number,time(NULL));
        } else {
            printf("Stopped Displaying Replaced Alarm With Message Type (%d) at <%ld>: <Type A>\n",
            replaced_type,time(NULL));
        }
        prt_alarm_list();
    
    sem_post(&alarmListAccess); /*unlock*/
}

/*WRITER FUNCTION*/
void remove_from_alarm_list(int msg_number, int print_msg){
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
  if(print_msg)
    printf("Type C Alarm Request Processed at <%ld>: Alarm Request With Message Number (%d) Removed\n",time(NULL),msg_number);
}


int alarm_exists(int msg_id, int type){
    /*loop through the alarm_list and check if the alarm
    with alarm number msg_number exist. >=1 exists, 0 not
    type: this parameter chooses between existence checking by 
    message_type(0) or message_number(1)*/

    alarm_t *next;
    int alr_exists = 0;
    alarm_reader_semaphore_lock();  
        switch(type)
        {
            case 0: /*message_type search*/
                for (next = alarm_list; next != NULL; next = next->link){
                    if(next->type == msg_id && !next->is_done){
                        alr_exists++;
                    }
                }
                break;
                                
            case 1: /*message_number search*/
                for (next = alarm_list; next != NULL; next = next->link){
                    if(next->number == msg_id && !next->is_done){
                        alr_exists++;
                    }
                }
                break;
                
            default:
                printf("Error, type = 0 for message_type and type = 1 for message_number\n");
                break;
        }      
        
    alarm_reader_semaphore_release();
    return alr_exists;
}


void remove_alarms_that_are_done(){ /*reads alarm_list*/
    alarm_t * next;
    int msg_number;

    while(1){
        msg_number = -1;
        alarm_reader_semaphore_lock();
            for(next = alarm_list; next != NULL; next = next->link){ 
                if(next->is_done){
                    msg_number = next->number;
                    break;
                }
            }
        alarm_reader_semaphore_release();
        if(msg_number < 0)
            break;            
        remove_from_alarm_list(next->number,0);
    }    
}
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>  TYPE B THREAD_LIST FUNCTIONS*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
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

void prt_thread_list(){
  /*assumes thread list has been locked before call*/
  #ifdef DEBUG        
          thread_ds *s;
          printf("List of Threads:\n");   
        
          for(s = thread_list; s != NULL; s = s->link)
              printf("Thread Type: %d \n",s->type);
  #endif
}

void remove_from_thread_list(int msg_type){
  thread_ds *temp, *prev;
    sem_wait(&t_threadListAccess); /*lock*/

    /* thread_ds *temp = thread_list, *prev;*/
    temp = thread_list;

    if (temp != NULL && temp->type == msg_type){
        thread_list = temp->link;
        free(temp);
    } else {
        while(temp != NULL && temp->type != msg_type){
            prev = temp;
            temp = temp->link;
        }

        if(temp != NULL){
        prev->link = temp->link;
        free(temp);
        }
    }
    sem_post(&t_threadListAccess);
}

/*returns 1 if thread exists and 0 if it doesn't*/
int thread_exists(int msg_type){
  thread_ds *s;
  int does_exist = 0;
  thread_reader_semaphore_lock();
    /*lock <<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>*/
    for(s = thread_list; s != NULL; s = s->link){
        if(s->type == msg_type){
            does_exist++;
        }
    }
    /*unlock <<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>*/
  thread_reader_semaphore_release();
  return does_exist;
}

int thread_has_active_alarm(int msg_type){    
    int exists = 0;    
    exists += alarm_exists(msg_type,0);
    return exists;
}

/*WRITER METHOD TO ADD THREAD INFO INTO thread_list*/
void * add_to_thread_list(void * args){
    int msg_type = (int) *((int *) args);
    
  sem_wait(&t_threadListAccess); /*lock*/
    
    if(thread_list == NULL) {
            thread_list = (thread_ds*) malloc(sizeof(thread_ds));
            thread_list->type = msg_type;                    
            // thread_list->flag = 0;
            thread_list->is_created = 0; 
            thread_list->link = NULL;
    }
    else {      
        thread_ds *next = thread_list;
        while(next->link != NULL){
            next = next->link;
        }
        next->link = (thread_ds*) malloc(sizeof (thread_ds));
        next = next->link;
        next->type = msg_type;  
        next->is_created = 0;    
        // next->flag = 0;
        next->link = NULL;
    }
    prt_thread_list();
  sem_post(&t_threadListAccess); /*unlock*/
}

void remove_threads_if_no_active_alarm(){ /*reads thread_list*/
    thread_ds *next;
    int msg_type;
    
    while(1){
        msg_type = -1;
        thread_reader_semaphore_lock();
            for(next = thread_list; next != NULL; next = next->link){ 
                if(!thread_has_active_alarm(next->type)){
                    msg_type = next->type;
                    break;
                }
            }
        thread_reader_semaphore_release();
        if(msg_type<0)
            break;            
        remove_from_thread_list(msg_type);
    }
}


void check_thread_list_and_create_thread(){
    
    thread_ds *next;
    int status;
    pthread_t print_thread;

    thread_reader_semaphore_lock();
        for(next = thread_list; next != NULL; next = next->link){                                  
            if (!next->is_created){  
                next->is_created = 1;                
                status = pthread_create(&print_thread,NULL,periodic_display_threads,&next->type);
                if (status != 0)
                    err_abort (status, "periodic_display_threads not created!\n");
                printf("Type B Alarm Request Processed at <%ld>: New Periodic Display Thread For Message Type (%d) Created.\n",
                        time(NULL),next->type);
            }
        }
    thread_reader_semaphore_release();
}
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>  TYPE C REMOVAL_LIST FUNCTIONS*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
void removal_reader_semaphore_lock(){
    sem_wait(&r_readCountAccess);
        r_readCount++;
        if(r_readCount==1) {
            sem_wait(&r_threadListAccess);
        }
  sem_post(&r_readCountAccess);
}

void removal_reader_semaphore_release(){
    sem_wait(&r_readCountAccess);
        r_readCount--;
        /*the last thread releases the database for writting*/
        if(r_readCount==0){
            sem_post(&r_threadListAccess);
        }
  sem_post(&r_readCountAccess);
}

void prt_removal_list(){
  /*assumes thread list has been locked before call*/
  #ifdef DEBUG        
          removal_ds *s;
          printf("List of Removal_Requests:\n");
          for(s = removal_list; s != NULL; s = s->link)
              printf("Msg_Number: %d\n",s->number);
  #endif
}

/*returns 1 if thread exists and 0 if it doesn't*/
int remove_request_exists(int msg_number){
  removal_ds *s;
  int does_exist = 0;
  removal_reader_semaphore_lock();  
  /*lock <<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>*/
  for(s = removal_list; s != NULL; s = s->link){
    if(s->number == msg_number){
      does_exist = 1;
    }
  }
  /*unlock <<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>*/
  removal_reader_semaphore_release();  
  return does_exist;
}

void remove_alarms_in_removal_list(){ /*reads alarm_list*/
    int msg_number;   
    removal_ds *next;            
    removal_reader_semaphore_lock();  
        for(next = removal_list; next != NULL; next = next->link){
            remove_from_alarm_list(next->number,1);            
        }        
        /*all alarms specified in the removal_list will be removed so
        the removal list must be cleared.*/
        removal_list = NULL;
    removal_reader_semaphore_release();  
}


/*WRITER METHOD TO ADD INTO removal_list*/
void add_to_removal_list(int msg_number){
  sem_wait(&r_threadListAccess); /*lock*/
  if(removal_list == NULL) {
          removal_list = (removal_ds*) malloc(sizeof(removal_ds));
          removal_list->number = msg_number;
          removal_list->link = NULL;
  }
  else {
      removal_ds *next = removal_list;
      while(next->link != NULL){
          next = next->link;
      }
      next->link = (removal_ds*) malloc(sizeof (removal_ds));
      next = next->link;
      next->number = msg_number;            
      next->link = NULL;
  }
  prt_removal_list();
  sem_post(&r_threadListAccess); /*unlock*/
}


/*WRITER METHOD TO REMOVE FROM removal_list*/
/*
void remove_from_removal_list(int msg_number){
  removal_ds *temp, *prev;

    sem_wait(&r_threadListAccess);    
        temp = removal_list;

        if (temp != NULL && temp->number == msg_number){
            removal_list = temp->link;
            free(temp);
        } else {
            while(temp != NULL && temp->number != msg_number){
                prev = temp;
                temp = temp->link;
            }

            if(temp != NULL){
                prev->link = temp->link;
                free(temp);
            }
        }
    sem_post(&r_threadListAccess);  
}
*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>  REQUIRED METHODSS*/
/*<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>*/


void * periodic_display_threads(void * args){
    int remaining_time;
    alarm_t * next;
    int message_type = (int) *((int *) args);
     
    while(1){
        alarm_reader_semaphore_lock();
            for (next = alarm_list; next != NULL; next = next->link){                                       
                if (next->type == message_type && !next->is_done){  
                    remaining_time = next->time - time(NULL);                    
                    if (remaining_time >= 0 ){
                        // printf("Alarm With Message Type (%d) and Message Number (%d) Displayed at <%ld>: <Type B>\n",
                        //     message_type, next->number, time(NULL));
                        printf("Printing message, Type : %d , Number : %d , Msg : %s , Tim : %ld\n",
                        next->type,next->number, next->message, remaining_time);
                        
                    } else {
                        next->is_done = 1;
                        printf("ALARM IS NOW DONE\n");
                    }
                }
            }
        alarm_reader_semaphore_release();
        if (!thread_exists(message_type)){
            printf("Type A Alarm Request Processed at <%ld>: Periodic Display Thread For Message Type (%d) Terminated: No more Alarm Requests For Message Type (%d).\n",
            time(NULL), message_type, message_type );
            break;
        }
        sleep(1);
    }
}



/*The alarm thread function allows the createion of periodic_display_threads
  This is a reader method, it doesn't modify the alarm_list. It simply reads
  through and assigns an alarm to a thread.*/

void * alarm_thread (void *arg){    
    while(1){      
        remove_alarms_that_are_done();
        remove_threads_if_no_active_alarm();        
        check_thread_list_and_create_thread();   
        remove_alarms_in_removal_list();     
        sleep(1);
    }
}


void invalid_input_error(){
    printf("Bad Command. Usage: \nType A: <+ve integer> Message(Message_Type : <+ve integer>, Message_Number : <+ve integer>) <string message> \nType B: Create_Thread: MessageType(Message_Type : <+ve integer>) \nType C: Cancle: Message(Message_Number : <+ve integer>)\n");
}




int main (int argc, char *argv[]){
    /*initialize the semaphores*/
    sem_init(&readCountAccess,0,1);
    sem_init(&alarmListAccess,0,1);
    sem_init(&t_threadListAccess,0,1);
    sem_init(&t_readCountAccess,0,1);
    sem_init(&r_threadListAccess,0,1);
    sem_init(&r_readCountAccess,0,1);

    /*local variables*/
    int status;
    char line[1500]; /*holds the initially entered string from user*/
    char tempS[1000]; /*temporarily stores the message string*/
    alarm_t *alarm;

    /*thread creation id variable*/
    pthread_t writer_thread;
    pthread_t alr_thread;

    /*create the alarm_thread thread*/
    status = pthread_create(&alr_thread,NULL,alarm_thread,NULL);
    if (status != 0)
        err_abort (status, "alarm_thread not created!\n");
    
    /*infinitely loops asking user for input*/
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
            invalid_input_error();
            continue;
        }
        /*if alarm request command, thread creation and thread termination commands are
         correct, check if the seconds and/or type of message have non negative values,
          if negative values entered display error message and restart the loop*/
        if(err_t1 == 4){
            if (t1_sec <= 0 || t1_type <= 0 || t1_num <= 0){
                invalid_input_error();
                continue;
            }
        }
        else if(err_t2 == 1){
            if (t2_type <= 0){
                invalid_input_error();
                continue;
            }
        }
        else if(err_t3 == 1){
            if (t3_num <= 0){
                invalid_input_error();
                continue;
            }
        }

// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> INPUT TYPE A ALARMS
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
            alarm->is_done = 0;

            /*call a writer thread to write to save the alarm created into the alarm thread*/

            status = pthread_create (&writer_thread, NULL, add_to_alarm_list, alarm);            
            if (status != 0)
                err_abort (status, "Insert alarm into alarm list");
  #ifdef DEGUG
              pthread_join(writer_thread,NULL);
  #endif

/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> INPUT TYPE B THREAD REQUEST*/
/*2==>*/} else if (err_t2 == 1){
            if(alarm_exists(t2_type,0)){
                /*alarm exists in alarm_list, searched by type(0)*/
                if (!thread_exists(t2_type)) {                    
                    /*1 = exists; 0 = not; create thread it already not created*/
                    status = pthread_create (&writer_thread, NULL, add_to_thread_list, &t2_type);
                    if (status != 0)
                        err_abort (status, "Create alarm thread");
                    printf("Type B Create Thread Alarm Request For Message Type (%d) Inserted Into Alarm List at <%ld>!\n",
                            t2_type,time(NULL)); 
        #ifdef DEGUG
                    pthread_join(writer_thread,NULL);
        #endif                   
                } else {
                    /*thread with msg_num = tw_type exists ; print error*/
                    printf("Error: More Than One Type B Alarm Request With Message Type (%d)!\n",t2_type);
                }
            } else {
                printf("Type B Alarm Request Error: No Alarm Request With Message Type (%d)!\n",t2_type);
            }             
/* <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><> TYPE C TERMINATION INPUT REQUEST*/
/*3==>*/}else if (err_t3 == 1){
          
          if(alarm_exists(t3_num,1)){
              if(!remove_request_exists(t3_num)){
                /*alarm with msg_number = t3_num exists; add to removal_list*/
                add_to_removal_list(t3_num);
                //  remove_alarm_request(t3_num);
                printf("Type C Cancel Alarm Request With Message Number (%d) Inserted Into Alarm List at <%ld>: <Type C>\n",t3_num,time(NULL));
              } else {
                  printf("Error: More Than One Request to Cancel Alarm Request With Message Number (%d)!\n",t3_num);
              }
          } else {
              /*alarm with msg_number = t3_num exists not*/
              printf("Error: No Alarm Request With Message Number (%d) to Cancel!\n",t3_num);
          }
        }
    }
}
