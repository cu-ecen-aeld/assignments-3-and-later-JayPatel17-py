#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("DEBUG_LOG: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("ERROR_LOG: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    DEBUG_LOG("Thread_func started...");
    
    struct thread_data* thread_param_args = (struct thread_data *) thread_param;
    DEBUG_LOG("td_wait_to_obtain_ms = %d ms\n", thread_param_args->td_wait_to_obtain_ms);
    /*
        adding 20ms delay here is wrong method.
        It may work in one mutex case.
        it will fail when multiple mutex multiple threads.
    */
    usleep((thread_param_args->td_wait_to_obtain_ms)*1000);
    thread_param_args->thread_complete_success=true;
    
    while ( pthread_mutex_trylock(thread_param_args->td_mutex) );
    pthread_mutex_unlock(thread_param_args->td_mutex);
    
    usleep((thread_param_args->td_wait_to_release_ms)*1000);
        
    pthread_exit(thread_param_args);
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    DEBUG_LOG("Allocating memory for thread_data");
    struct thread_data* thread_func_args = malloc(sizeof(struct thread_data));

    thread_func_args->td_wait_to_obtain_ms = wait_to_obtain_ms;
    thread_func_args->td_wait_to_release_ms = wait_to_release_ms;
    thread_func_args->td_mutex = mutex;

    // NOTE : mutex init & lock has been done by calling function so not repeating
    DEBUG_LOG("Creating thread...");
    if ( pthread_create(thread, NULL, threadfunc, thread_func_args) != 0 )
    {
        ERROR_LOG("Error creating thread!!");
        free(thread_func_args);
        //Unloacking mutex as this failed to create thread
        pthread_mutex_unlock(mutex);
        return false;
    } // else thread creation successfull
    // NOTE : mutex unlock & destroy has been done by calling function so not repeating
    
    return true;
}