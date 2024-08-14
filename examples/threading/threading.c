#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    DEBUG_LOG("Thread received wait for ");
    
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    //usleep((thread_func_args->td_wait_to_obtain_ms)*1);
    
    pthread_exit(NULL);
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

    //thread_func_args->td_wait_to_obtain_ms = wait_to_obtain_ms;
    //thread_func_args->td_wait_to_release_ms = wait_to_release_ms;

    //mutex init & lock has been done by calling function so not repeating
    DEBUG_LOG("Creating thread");
    if ( pthread_create(thread, NULL, threadfunc, &thread_func_args) != 0 ) {
        ERROR_LOG("Error creating thread");
        free(thread_func_args);
        //Unloacking mutex as this failed to create thread
        pthread_mutex_unlock(mutex);
        return false;
    } // else thread creation successfull
    //mutex unlock & destroy has been done by calling function so not repeating

    // prinitng thread ID
    DEBUG_LOG("Thread cereated successfully. Thread ID: %lu", *thread);
    
    free(thread_func_args);
    DEBUG_LOG("Wait for the thread to finish");
    if ( pthread_join(*thread, NULL) != 0 ) {
        ERROR_LOG("Error joining thread");
        return false;
    }// else thread exited successfully
    
    // usleep((thread_func_args->td_wait_to_release_ms)*1);
    thread_func_args->thread_complete_success=true;
    return true;
}

