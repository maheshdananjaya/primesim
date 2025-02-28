//===========================================================================
// page_table.cpp contains a simple page translation model to allocate 
// physical pages sequentially starting from 0
//===========================================================================
/*
Copyright (c) 2015 Princeton University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Princeton University nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY PRINCETON UNIVERSITY "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL PRINCETON UNIVERSITY BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "pbuff.h"

using namespace std;

void PBuff::init(int pbuff_size_in, int delay_in, int offset_in)
{
    cur_size = 0;
    max_size = pbuff_size_in;
    access_delay = delay_in;      
    offset = offset_in;

    lock = new pthread_mutex_t;
    pthread_mutex_init(lock, NULL);
    pbuff = NULL;
    head = NULL;
    tail = NULL;

    num_persists = 0;
    num_barriers = 0;
    num_acq_barr = 0;
    num_wr = 0;
    num_cl_insert = 0;
    num_cl_evict = 0;
    num_dep_conflicts = 0;
    num_dep_cls_flushed = 0;
}

//This should in InsMem          
PBuffLine * PBuff::access(uint64_t addr){
    
    //If the access successful then. This sends a access hit or access miss.
  uint64_t ctag =  addr >> offset;
  // /cout << "PBuff Access : addr- "<< addr << " and cache-tag-" << ctag << endl;
  printf("[ACCESS] :  addr 0x%lx cache 0x%lx \n", addr, ctag);
  printBuffer();

  PBuffLine * tmp  = head;
  PBuffLine * catched = NULL;
  bool barrier_hit = false; 

    while(tmp != NULL){
      //printf("acecss:\t  0x%lx \t  0x%lx \n", tmp->cache_tag, tmp->last_addr);
      if(tmp->cache_tag == ctag){
        printf("[ACCESS]: Found the line : %lx \n",tmp->cache_tag );
        catched = tmp;
      }
      if(catched !=NULL && tmp->is_barrier){
        barrier_hit = true;
      }
      tmp = tmp->next_line;
      //break;
      // /printf("A");
    }    

    if(catched !=NULL && !barrier_hit) return catched; // Hit  
    return NULL; // Miss

    //Access Delay?.
}  

void PBuff::printBuffer(){
  PBuffLine * tmp  = head;
  int count=0;
  printf("\nPersist Buffer %d with size %d\n", core_id, cur_size);
  printf("---------------------- --------------------------------\n");
    while(tmp != NULL){
      printf("| Entry %d \t |  0x%lx \t | 0x%lx \t | 0x%lx \t | 0x%s \n",count, tmp->cache_tag, tmp->last_addr, tmp->unique_wr_id, (tmp->is_barrier)?"Barrier":" ");
      printf("---------------------- --------------------------------\n");
      count++;
      tmp = tmp->next_line;

      if(count >= max_size) break;
    }
    
}

int PBuff::insert(PBuffLine * new_line){
    if(isFull()){
      return -1; //This must be avoided
    } 
    else if (isEmpty()){
        printf("[INSERT]: Buffer is Empty : %lx cache %lx \n", new_line->last_addr, new_line->cache_tag);
        new_line->next_line = NULL;
        tail = new_line;
        head = new_line;
        cur_size++;
        num_cl_insert++;
        if(new_line->is_barrier) num_barriers++; // Later distinguish as release and acquire.
        printBuffer();
        return 1;
    }
    else{
      printf("[INSERT]: Buffer is NOT Empty : %lx cache %lx \n", new_line->last_addr, new_line->cache_tag);
      new_line->next_line = NULL;
      tail->next_line = new_line;
      tail = new_line;
      cur_size++;
      num_cl_insert++;
      if(new_line->is_barrier) num_barriers++;
      printBuffer();
      return 1;
    }    
}

int PBuff::insert(uint64_t addr){
    
    PBuffLine pline;
    pline.timestamp = 0;
    printf("Dummy Insert : %lx \n", pline.timestamp);
    return 1;
}

int PBuff::getBuffSize(){
  return cur_size;
}

bool PBuff::isFull(){
  if(cur_size >= max_size)return true;
  else return false;
}

bool PBuff::isEmpty(){
  if(cur_size == 0) return true;
  else return false;
}

//Remove the head-only head can be removed. remove(uint64_t addr)
PBuffLine * PBuff::remove(){

    printf("Removing a line from persist buffer %d \n",core_id);

    if(!isEmpty()){
      PBuffLine * rm_line = head;
      head = head->next_line;
      cur_size--;
      num_cl_evict++;
      return rm_line;
    }

    return NULL;
}

int PBuff::getAccessDelay(){
  return this->access_delay;
}

int PBuff::getOffset(){
  return this->offset;
}

void PBuff::report(ofstream* result)
{
    *result << "--------------Persist-Buffer - ["<< core_id << "]------------" << endl;
    *result << "Number of Writes : " << num_wr << endl;
    *result << "Number of Inserts : " << num_cl_insert << endl;
    *result << "Number of Evicts : " << num_cl_evict << endl;
    *result << "Number of Barriers : " << num_barriers << endl;
    *result << "Number of Acquire Barriers : " <<  num_acq_barr << endl;  
    *result << "Number of DEP conflicts : " << num_dep_conflicts << endl;
    *result << "Number of DEP cls flushed : " << num_dep_cls_flushed << endl;

    *result << "-----------------------------------------------------" << endl;
}

PBuff::~PBuff()
{
    pthread_mutex_destroy(lock);
    delete lock;
}


//-------------------------------------------------------------------------------------------
//Memory Controller Queue
void MCQBuff::init(int mcq_buff_size_in, int delay_in){
   
   cur_size = 0;
   max_size = mcq_buff_size_in;   
   access_delay = delay_in;
   head = NULL;
   tail = NULL;

    num_barriers = 0;
    num_cls = 0; //in
    num_access = 0;
    num_persists = 0;

}

//May be access for a cacheline can be added.

int MCQBuff::insert(PBuffLine * new_pbuff_line, uint64_t addr, int core_id){
    cout << "MCQBuff Insert" << endl;
    //This cannot happen by design
    if(!isFull()){        
      if(isEmpty()){
        //core id and addr are alredy inside.
        new_pbuff_line->next_line = NULL;
        tail = new_pbuff_line;
        head = new_pbuff_line;
        cur_size++;

      }else{ //if not empty
          new_pbuff_line->next_line = NULL;
          tail->next_line = new_pbuff_line;
          tail = new_pbuff_line;
          cur_size++;
      }

      num_access++;
      num_cls++;
      if(new_pbuff_line->is_barrier) num_barriers++;

      return 1;
    }

    else cout << "Error--------" << endl;
    
    return -1;
} 

PBuffLine * MCQBuff::remove(){
    cout << "MCQBuff Remove" << endl;
    
    if(!isEmpty()){
      //showing.
      PBuffLine * rm_line = head;
      head = head->next_line;
      cur_size--;

      num_persists++;

      return rm_line;     

    }
    else return  NULL;
} 

PBuffLine * MCQBuff::remove(uint64_t addr){

    cout << "MCQBuff Remove wit address" << endl;
    return  NULL;
} 

int MCQBuff::getAccessDelay(){
  return access_delay;
}

bool MCQBuff::isEmpty(){
  if(cur_size == 0) return true;
  else return false;
}

bool MCQBuff::isFull(){
  if(cur_size >= max_size) return true;
  else return false;
}

void MCQBuff::report(ofstream * result){

    *result << "--------------Memory Controller Queue----------------" << endl;
    *result << "Number of Accesses : " << num_access << endl;
    *result << "Number of Persists : " << num_persists << endl;
    *result << "Number of Barriers : " << num_barriers << endl;
    *result << "-----------------------------------------------------" << endl;
}

MCQBuff::~MCQBuff(){
    pthread_mutex_destroy(lock);
    delete lock;
}


