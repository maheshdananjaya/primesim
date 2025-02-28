// A simple producer/consumer program using semaphores and threads
//
// Adapted from Greg Andrews' textbook website
// http://www.cs.arizona.edu/people/greg/mpdbook/programs/
//

// compile: gcc -lpthread prodcom.c -o prodcom
// run:     ./prodcom numIters  (eg ./prodcom 20)
 
#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <cstdlib>
#include <atomic>

#define SHARED 1

void *Producer(void *);  // the two threads
void *Consumer(void *);

//sem_t empty, full;       // the global semaphores
int data;                // shared buffer
int numIters;

// main() :    read command line and create threads, then
//             print result when the threads have quit
std::atomic <int> empty;
//int data_arr[10];
//std::atomic <int *> data_a [10];
std::atomic <int> full;  

int main(int argc, char *argv[]) {
  // thread ids and attributes
  empty.store(0, std::memory_order_release);
  for(int k=0;k<10;k++){
    //data_arr[k]= k;
    //data_a[k].store (0, std::memory_order_release);   
  }

  full.store (0, std::memory_order_release);
  
  pthread_t pid, cid;  
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  numIters = atoi(argv[1]);
  //sem_init(&empty, SHARED, 1);  // sem empty = 1
  //sem_init(&full, SHARED, 0);   // sem full = 0 

  printf("running for %d iterations\n",numIters);
  printf("main started\n");
  pthread_create(&pid, &attr, Producer, NULL);
  pthread_create(&cid, &attr, Consumer, NULL);
  pthread_join(pid, NULL);
  pthread_join(cid, NULL);
  printf("main done\n");
}

// deposit 1, ..., numIters into the data buffer
void *Producer(void *arg) {
  int produced;
  printf("Producer created\n");
  for (produced = 1; produced <= numIters; produced++) {
    //sem_wait(&empty);
    int expected = 0;
      while(!empty.compare_exchange_weak(expected, 1, std::memory_order_acquire)){
        expected=0;
        printf("Produce\n");
      }
      printf("producing\n");
      data = produced;
     // printf("producing %d \n", data);
      //sem_post(&full);
      full.store (1, std::memory_order_release);    
  }
}

// fetch numIters items from the buffer and sum them
void *Consumer(void *arg) {
  int total = 0, consumed;
  printf("Consumer created\n");
  for (consumed = 0; consumed < numIters; consumed++) {
    //sem_wait(&full);
    while(!full.load(std::memory_order_acquire)){printf("Consume\n");};
    total = total+data;
    //std::this_thread::sleep_for(3);
    printf("consuming %d \n", data);
    full = 0;
    empty.store (0, std::memory_order_release);
    //sem_post(&empty);
  }
  printf("after %d iterations, the total is %d (should be %d)\n",
   numIters, total, numIters*(numIters+1)/2);
}

//https://stackoverflow.com/questions/19774778/when-is-it-necessary-to-use-use-the-flag-stdlib-libstdc
