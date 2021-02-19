
#ifndef HASHMAP_H
#define HASHMAP_H
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>


struct hash_data {
	struct hash_data *prev;
	void *key;
	void *data;
	struct hash_data *next;
	pthread_mutex_t lock;
};



struct hash_map {
	// placeholder for mutex
	int curr_size;
	int max_size;
	struct hash_data **bucket;
	pthread_mutex_t lock;
	size_t (*hash)();
	int (*cmp)();
	void (*key_destruct)();
	void (*value_destruct)();
}; //Modify this!

struct hash_map* hash_map_new(size_t (*hash)(void*), int (*cmp)(void*,void*),
    void (*key_destruct)(void*), void (*value_destruct)(void*));

void hash_map_put_entry_move(struct hash_map* map, void* k, void* v);

void hash_map_remove_entry(struct hash_map* map, void* k);

void* hash_map_get_value_ref(struct hash_map* map, void* k);

void hash_map_destroy(struct hash_map* map);

void hash_map_resize(struct hash_map* map);

#endif
