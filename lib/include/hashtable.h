#include <stdint.h>
#include <stddef.h>
#ifndef __HASHTABLE_H
#define __HASHTABLE_H

//Murmurhash3 32 bit implementation
uint32_t               MurmurHash3_32(void *key,int len,uint32_t seed);
static inline uint32_t getblock(uint8_t *p,int i);
static inline uint32_t rotl(uint32_t x,uint8_t r);
static inline uint32_t fmix(uint32_t h);


//A node in the queue of entries
struct hash_entry
{
  void *key;
  size_t key_len;
  void *value;
  size_t value_len;
  struct hash_entry *next,*prev;
};

//a queue of hash_entries
struct hash_queue
{
  size_t size;
  struct hash_entry *head,*tail;
};

//hash_table structure we're gonna use murmurhash for hashing
struct hash_table
{
  struct hash_queue **table;
  size_t size;
  size_t num_elements;
  uint32_t seed;
};

//entry functions
struct hash_entry* hash_entry_create(void *key,size_t key_len, void *value,size_t value_len);
void                 hash_entry_destroy(struct hash_entry *entry);
//queue functions
void                 hash_queue_init(struct hash_queue *queue);
void                 hash_queue_push_front(struct hash_queue *queue,void *key,size_t key_len,void *value,size_t value_len);
void                 hash_queue_push_back(struct hash_queue *queue,void *key,size_t key_len,void *value,size_t value_len); 
                     //ownership transfers 
struct hash_entry* hash_queue_pop_front(struct hash_queue *queue);
                     //ownership transfers 
struct hash_entry* hash_queue_pop_back(struct hash_queue *queue);
                     //returning value in case it needs to be freed
void*                hash_queue_remove(struct hash_queue *queue,struct hash_entry *entry);
                     //for complex objects
int                  hash_queue_find(struct hash_queue *queue,struct hash_entry *entry,int (*cmpfunc)(void *elem1,void *elem2));
int                  hash_queue_destroy(struct hash_queue *queue);
//hash table functions
int                  hash_table_init(struct hash_table *hashtable,size_t size);
void                 hash_table_insert(struct hash_table *hashtable,void *key,size_t key_len,void *value,size_t value_len);
void*                hash_table_get(struct hash_table *hashtable,void *key,size_t key_len,size_t* value_len);
void*                hash_table_entry(struct hash_table *hashtable,void *key,size_t key_len);
                     //return the value in case we need to deallocate it
void*                hash_table_remove(struct hash_table *hashtable,void *key,size_t key_len);
int                  hash_table_destroy(struct hash_table *hashtable);
#endif
