


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>



#define MAX_KEY_LEN	128
#define MAX_VALUE_LEN	512
#define MAX_TABLE_SIZE	1024



typedef struct hashnode_s {

	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];
	
	struct hashnode_s *next;
	
} hashnode_t;


typedef struct hashtable_s {

	hashnode_t **nodes; //* change **, 

	int max_slots;
	int count;

	pthread_mutex_t lock;

} hashtable_t;

hashtable_t hash;


//Connection 
// 'C' + 'o' + 'n'
static int _hash(char *key, int size) {

	if (!key) return -1;

	int sum = 0;
	int i = 0;

	while (key[i] != 0) {
		sum += key[i];
		i ++;
	}

	return sum % size;

}

hashnode_t *_create_node(char *key, char *value) {

	hashnode_t *node = (hashnode_t*)malloc(sizeof(hashnode_t));
	if (!node) return NULL;
	
	strncpy(node->key, key, MAX_KEY_LEN);
	strncpy(node->value, value, MAX_VALUE_LEN);
	node->next = NULL;

	return node;
}


//
int init_hashtable(hashtable_t *hash) {

	if (!hash) return -1;

	hash->nodes = (hashnode_t**)malloc(sizeof(hashnode_t*) * MAX_TABLE_SIZE);
	if (!hash->nodes) return -1;

	hash->max_slots = MAX_TABLE_SIZE;
	hash->count = 0; 

	pthread_mutex_init(&hash->lock, NULL);

	return 0;
}

// 
void dest_hashtable(hashtable_t *hash) {

	if (!hash) return;

	int i = 0;
	for (i = 0;i < hash->max_slots;i ++) {
		hashnode_t *node = hash->nodes[i];

		while (node != NULL) { // error

			hashnode_t *tmp = node;
			node = node->next;
			hash->nodes[i] = node;
			
			free(tmp);
			
		}
	}

	free(hash->nodes);
	
}



// mp
int put_kv_hashtable(hashtable_t *hash, char *key, char *value) {

	if (!hash || !key || !value) return -1;

	int idx = _hash(key, MAX_TABLE_SIZE);

	pthread_mutex_lock(&hash->lock);

	hashnode_t *node = hash->nodes[idx];
#if 1
	while (node != NULL) {
		if (strcmp(node->key, key) == 0) { // exist
			pthread_mutex_unlock(&hash->lock);
			return 1;
		}
		node = node->next;
	}
#endif

	hashnode_t *new_node = _create_node(key, value);
	new_node->next = hash->nodes[idx];
	hash->nodes[idx] = new_node;
	
	hash->count ++;

	pthread_mutex_unlock(&hash->lock);

	return 0;
}


char * get_kv_hashtable(hashtable_t *hash, char *key) {

	if (!hash || !key) return NULL;

	int idx = _hash(key, MAX_TABLE_SIZE);

	pthread_mutex_lock(&hash->lock);
	hashnode_t *node = hash->nodes[idx];

	while (node != NULL) {

		if (strcmp(node->key, key) == 0) {
			pthread_mutex_unlock(&hash->lock);
			return node->value;
		}

		node = node->next;
	}

	pthread_mutex_unlock(&hash->lock);

	return NULL;

}


int count_kv_hashtable(hashtable_t *hash) {
	return hash->count;
}

int delete_kv_hashtable(hashtable_t *hash, char *key) {
	if (!hash || !key) return -2;

	int idx = _hash(key, MAX_TABLE_SIZE);

	pthread_mutex_lock(&hash->lock);
	hashnode_t *head = hash->nodes[idx];
	if (head == NULL) return -1; // noexist
	// head node
	if (strcmp(head->key, key) == 0) {
		hashnode_t *tmp = head->next;
		hash->nodes[idx] = tmp;
		
		free(head);
		hash->count --;
		pthread_mutex_unlock(&hash->lock);
		
		return 0;
	}

	hashnode_t *cur = head;
	while (cur->next != NULL) {
		if (strcmp(cur->next->key, key) == 0) break; // search node
		
		cur = cur->next;
	}

	if (cur->next == NULL) {
		
		pthread_mutex_unlock(&hash->lock);
		return -1;
	}

	hashnode_t *tmp = cur->next;
	cur->next = tmp->next;
	free(tmp);
	hash->count --;

	pthread_mutex_unlock(&hash->lock);
	
	return 0;
}


int exist_kv_hashtable(hashtable_t *hash, char *key) {

	char *value = get_kv_hashtable(hash, key);
	if (value) return 1;
	else return 0;
	
}


