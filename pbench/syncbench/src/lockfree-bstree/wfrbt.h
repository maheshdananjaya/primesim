#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <unistd.h>
#include <vector>

#include "atomic_ops.h"
#include "lockfree.h"
//#include <atomic_ops.h>
//#include "../atomic_ops/atomic_ops.h"
//AO_t size_t
#define RECYCLED_VECTOR_RESERVE 5000000

#define MARK_BIT 1
#define FLAG_BIT 0

enum{INS,DEL};
enum {UNMARK,MARK};
enum {UNFLAG,FLAG};

typedef uintptr_t Word;

//#define B4
//#define B96 38-40

#define B8
//#define B8_MALLOC

//#define B4

//#define B256
//#define B128

typedef long load_t;

typedef struct node{
	int key;

  #ifdef B4
  load_t load1;
  #endif

  #ifdef B8
  load_t load1;
  load_t load2;
  #endif

  #ifdef B8_MALLOC
  load_t *load1;
  load_t *load2;
  #endif

  #ifdef B16
  load_t load1;
  load_t load2;
  load_t load3;
  load_t load4;
  #endif

  #ifdef B16_MALLOC
  load_t *load1;
  load_t *load2;
  load_t *load3;
  load_t *load4;
  #endif

  #ifdef B32
  load_t load1;
  load_t load2;
  load_t load3;
  load_t load4;
  load_t load5;
  load_t load6;
  load_t load7;
  load_t load8;
  #endif

  #ifdef B64
  load_t load1;
  load_t load2;
  load_t load3;
  load_t load4;
  load_t load5;
  load_t load6;
  load_t load7;
  load_t load8;
  load_t load9;
  load_t load10;
  load_t load11;
  load_t load12;
  load_t load13;
  load_t load14;
  load_t load15;
  load_t load16;
  #endif

#ifdef B96
  load_t load1;
  load_t load2;
  load_t load3;
  load_t load4;
  load_t load5;
  load_t load6;
  load_t load7;
  load_t load8;
  load_t load9;
  load_t load10;
  load_t load11;
  load_t load12;
  load_t load13;
  load_t load14;
  load_t load15;
  load_t load16;
  
  load_t load17;
  load_t load18;
  load_t load19;
  load_t load20;
  load_t load21;
  load_t load22;
  load_t load23;
  load_t load24;
  #endif

  #ifdef B128
    load_t load1 __attribute__((aligned(64)));
    load_t load2; 
  #endif

  #ifdef B128_MALLOC
    load_t load1 __attribute__((aligned(64)));
    load_t * load2; 
    load_t load3; 
  #endif

  #ifdef B256
    load_t load1 __attribute__((aligned(64)));
    load_t load2 __attribute__((aligned(64)));
    load_t load3 __attribute__((aligned(64)));
    //load_t load4; __attribute__((aligned(64)));
    load_t load4;

  #endif

  #ifdef B512
    load_t load1; __attribute__((aligned(64)));
    load_t load2; __attribute__((aligned(64)));
    load_t load3; __attribute__((aligned(64)));
    load_t load4; __attribute__((aligned(64)));
    load_t load5; __attribute__((aligned(64)));
    load_t load6; __attribute__((aligned(64)));
    load_t load7; __attribute__((aligned(64)));
    load_t load8; __attribute__((aligned(64)));
  #endif
  

	AO_double_t volatile child;
	#ifdef UPDATE_VAL
		long value;
	#endif
} node_t;

typedef struct seekRecord{
  // SeekRecord structure
  size_t leafKey;
  node_t * parent;
  AO_t pL;
  bool isLeftL; // is L the left child of P?
  node_t * lum;
  AO_t lumC;
  bool isLeftUM; // is  last unmarked node's child on access path the left child of  the last unmarked node?
} seekRecord_t;

/*
typedef struct barrier {
	pthread_cond_t complete;
	pthread_mutex_t mutex;
	int count;
	int crossing;
} barrier_t;
*/
typedef uintptr_t val_t;

typedef struct thread_data {
  val_t first;
  long range;
  int update;
  int alternate;
  int effective;
  int operations;
  int init_size;
  int id;
  unsigned long numThreads;
  unsigned long nb_add;
  unsigned long nb_added;
  unsigned long nb_remove;
  unsigned long nb_removed;
  unsigned long nb_contains;
  unsigned long nb_found;
  unsigned long ops;
  unsigned int seed;
  double search_frac;
  double insert_frac;
  double delete_frac;
  long keyspace1_size;
  node_t* rootOfTree;
  barrier_t *barrier;
  std::vector<node_t *> recycledNodes;
  seekRecord_t * sr; // seek record
  seekRecord_t * ssr; // secondary seek record

} thread_data_t;


inline void *xmalloc(size_t size) {
  void *p = malloc(size);
  if (p == NULL) {
    perror("malloc");
    exit(1);
  }
  return p;
}


// Forward declaration of window transactions
int perform_one_delete_window_operation(thread_data_t* data, seekRecord_t * R, size_t key);

int perform_one_insert_window_operation(thread_data_t* data, seekRecord_t * R, size_t newKey);


/* ################################################################### *
 * Macro Definitions
 * ################################################################### */



inline bool SetBit(volatile unsigned long *array, int bit) {

     bool flag; 
      __asm__ __volatile__("lfence" : : : "memory");
     __asm__ __volatile__("lock bts %2,%1; setb %0" : "=q" (flag) : "m" (*array), "r" (bit)); return flag; 
   return flag;
}

bool mark_Node(volatile AO_t * word){
	return (SetBit(word, MARK_BIT));
}

//#define atomic_cas_full(addr, old_val, new_val) __sync_bool_compare_and_swap(addr, old_val, new_val);
#define atomic_cas_full(addr, old_val, new_val) DIMP_compare_and_swap_full(addr, old_val, new_val);


//-------------------------------------------------------------
#define create_child_word(addr, mark, flag) (((uintptr_t) addr << 2) + (mark << 1) + (flag))
#define is_marked(x) ( ((x >> 1) & 1)  == 1 ? true:false)
#define is_flagged(x) ( (x & 1 )  == 1 ? true:false)

#define get_addr(x) (x >> 2)
#define add_mark_bit(x) (x + 4UL)
#define is_free(x) (((x) & 3) == 0? true:false)

//-------------------------------------------------------------

/* ################################################################### *
 * Correctness Checking
 * ################################################################### */
size_t in_order_visit(node_t * rootNode){
	size_t key = rootNode->key;
	
	if((node_t *)get_addr(rootNode->child.AO_val1) == NULL){
		return (key);
	}
	
	node_t * lChild = (node_t *)get_addr(rootNode->child.AO_val1);
	node_t * rChild = (node_t *)get_addr(rootNode->child.AO_val2);
	
	if((lChild) != NULL){
		size_t lKey = in_order_visit(lChild);
		if(lKey >= key){
			std::cout << "Lkey is larger!!__" << lKey << "__ " << key << std::endl;
			std::cout << "Sanity Check Failed!!" << std::endl;
		}
	}
	
	if((rChild) != NULL){
	        size_t rKey = in_order_visit(rChild);
		if(rKey < key){
			std::cout << "Rkey is smaller!!__" << rKey << "__ " << key <<  std::endl;
			std::cout << "Sanity Check Failed!!" << std::endl;
		}
	}
	return (key);
}

