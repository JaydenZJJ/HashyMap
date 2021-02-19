#include <stdlib.h>
#include "hashmap.h"
#include <string.h>
#include <stdio.h>

struct hash_map* hash_map_new(size_t (*hash)(void*), int (*cmp)(void*,void*),
    void (*key_destruct)(void*), void (*value_destruct)(void*)) {
	if (hash == NULL || cmp == NULL || key_destruct == NULL || value_destruct == NULL){
		return NULL;
	}
	struct hash_map *my_map = malloc(sizeof(struct hash_map));
	my_map->bucket = calloc(sizeof(struct hash_data*),64);

	my_map->curr_size = 0;
	my_map->max_size = 64;
	my_map->hash = hash;
	my_map->cmp = cmp;
	my_map->key_destruct = key_destruct;
	my_map->value_destruct = value_destruct;

	return my_map;
}

void hash_map_put_entry_move(struct hash_map* map, void* k, void* v) {
	if (k == NULL || v == NULL){
		return;
	}
	size_t hash_val = map->hash(k);
	int index = hash_val%map->max_size;
	map->curr_size++;
	if (map->bucket[index]==0){
		// create the head entry
		struct hash_data *temp_entry = malloc(sizeof(struct hash_data));
		pthread_mutex_init(&(temp_entry->lock), NULL);	
		// copy the values into struct hash_data
		pthread_mutex_lock(&(temp_entry->lock));
		temp_entry->key = k;
		temp_entry->data = v;
		temp_entry->prev = NULL;
		temp_entry->next = NULL;
		// compile everything together
		map->bucket[index] = temp_entry;
		pthread_mutex_unlock(&(temp_entry->lock));
		return;
	}
	else{
		// if the head of LL is not NULL
		if (map->bucket[index]!=NULL){
			// check if HEAD is k, then remove head, put in new head and tail
			if (map->cmp(map->bucket[index]->key,k)==1){
				if (map->bucket[index]->next == NULL){
					hash_map_remove_entry(map,k);
					struct hash_data *new_data = malloc(sizeof(struct hash_data));
					new_data->key = k;
					new_data->data = v;
					new_data->next = NULL;
					new_data->prev = NULL;
					map->bucket[index] = new_data;
				}
				else{
					hash_map_remove_entry(map,k);
					struct hash_data *new_data = malloc(sizeof(struct hash_data));
					new_data->key = k;
					new_data->data = v;
					new_data->next = NULL;
					struct hash_data *cursor = map->bucket[index];
					while (cursor->next!=NULL){
						cursor = cursor->next;
					}
					cursor->next = new_data;
					new_data->prev = cursor;
				}
				return;
			}
			// if the head is not k
			else{
				struct hash_data *cursor = map->bucket[index];
				struct hash_data *tail = map->bucket[index];
				// loop through to find k
				while (cursor!=NULL){
					// if we find k, remove it
					if (map->cmp(cursor->key,k)==1){
						struct hash_data *prev_cursor = cursor->prev;
						struct hash_data *next_cursor = cursor->next;
						map->key_destruct(cursor->key);
						map->value_destruct(cursor->data);
						free(cursor);
						prev_cursor->next = next_cursor;
						next_cursor->prev = prev_cursor;
						tail = next_cursor;
						break;
					}
					cursor = cursor->next;
				}
				// k has been removed and new node needs to be added
				struct hash_data *new_data = malloc(sizeof(struct hash_data));
				new_data->key = k;
				new_data->data = v;
				new_data->next = NULL;
				while (tail->next!=NULL){
					tail = tail->next;
				}
				tail->next = new_data;
				new_data->prev = tail;
				return;
			}
		}
		// if the head is NULL
		else{
			struct hash_data *new_data = malloc(sizeof(struct hash_data));
			new_data->key = k;
			new_data->data = v;
			new_data->next = NULL;
			new_data->prev = NULL;
			map->bucket[index] = new_data;
		}
	}
	pthread_mutex_unlock(&(map->bucket[index]->lock));
	// check if the load factor is approaching the limit
	// resize the map is it's beyond 0.75
	if ((double)map->curr_size/map->max_size>0.75){
		hash_map_resize(map);
	}
}

void hash_map_remove_entry(struct hash_map* map, void* k) {
	if (k == NULL){
		return;
	}
	// three cases to consider:
	// 1.1 the node to remove is head and no next
	// 1.2 the node to remove is head and have next
	// 2. the node to remove is in the middle
	// 3. the node to remove is at the end

	size_t hash_val = map->hash(k);
	int index = hash_val%map->max_size;
	if (map->bucket[index]==0 || map->bucket[index]==NULL){
		return;
	}
	else{
		pthread_mutex_lock(&(map->bucket[index]->lock));
		struct hash_data *cursor = map->bucket[index];
		// case 1.1 the node to remove is head and no next
		// free everything and head is NULL
		if (map->cmp(cursor->key,k)==1){
			if (cursor->next == NULL){
				map->key_destruct(cursor->key);
				map->value_destruct(cursor->data);
				free(cursor);
				map->bucket[index] = NULL;
			}
			// case 1.2 the node to remove is head and have next
			else{
				struct hash_data *new_head = cursor->next;
				map->key_destruct(cursor->key);
				map->value_destruct(cursor->data);
				free(cursor);
				map->bucket[index] = new_head;
				new_head->prev = NULL;
			}
			map->curr_size--;
			return;
		}
		// At this point, the node to remove is behind head, find node
		
		while (cursor != NULL){
			if (cursor->next == NULL){
				return;
			}
			cursor = cursor->next;
			if (map->cmp(cursor->key,k)==1){
				// case 3 node to remove is last 
				if (cursor->next == NULL){
					pthread_mutex_lock(&(cursor->lock));
					struct hash_data *prev_cursor = cursor->prev;
					map->key_destruct(cursor->key);
					map->value_destruct(cursor->data);
					free(cursor);
					prev_cursor->next = NULL;
					map->curr_size--;
					return;
				}
				// case 2 node to remove is middle
				else{
					pthread_mutex_lock(&(cursor->prev->lock));
					pthread_mutex_lock(&(cursor->next->lock));
					struct hash_data *prev_cursor = cursor->prev;
					struct hash_data *next_cursor = cursor->next;
					map->key_destruct(cursor->key);
					map->value_destruct(cursor->data);
					free(cursor);
					prev_cursor->next = next_cursor;
					next_cursor->prev = prev_cursor;
					pthread_mutex_unlock(&(prev_cursor->lock));
					pthread_mutex_unlock(&(next_cursor->lock));
					map->curr_size--;
					return;
				}
			}
		}
	}
	// decrease size
	map->curr_size--;
}

void* hash_map_get_value_ref(struct hash_map* map, void* k) {
	if (k == NULL){
		return NULL;
	}
	size_t hash_val = map->hash(k);
	int index = hash_val%map->max_size;
	if (map->bucket[index]==0){
		return NULL;
	}
	else{
		struct hash_data *cursor = map->bucket[index];
		while (cursor!=NULL){
			pthread_mutex_lock(&(cursor->lock));
			if (map->cmp(cursor->key,k)==1){
				pthread_mutex_unlock(&(map->bucket[index]->lock));
				return cursor->data;
			}
			pthread_mutex_unlock(&(cursor->lock));
			cursor = cursor->next;
		}
	}
	pthread_mutex_unlock(&(map->bucket[index]->lock));
	return NULL;
}

void hash_map_destroy(struct hash_map* map) {
	
	for (int x = 0; x<map->max_size; x++){
		if (map->bucket[x]!=0){
			struct hash_data *cursor = map->bucket[x];
			struct hash_data *escape = cursor;
			while (cursor!=NULL){
				escape = cursor->next;
				map->key_destruct(cursor->key);
				map->value_destruct(cursor->data);
				free(cursor);
				cursor = escape;
			}
		}
	}
	free(map->bucket);
	free(map);
	return;
}

void hash_map_resize(struct hash_map* map){
	struct hash_data **temp = map->bucket;
	map->bucket = calloc(sizeof(struct hash_data*),map->max_size*2);
	int old_max = map->max_size;
	map->max_size = map->max_size*2;
	map->curr_size = 0;
	for (int x = 0; x < old_max; x++){
		if (temp[x] != 0 && temp[x] != NULL){
			struct hash_data *cursor = temp[x];
			while (cursor != NULL){
				hash_map_put_entry_move(map,cursor->key,cursor->data);
			}
		}
	}
}
//