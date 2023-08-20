
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>



#define MAX_LEVEL 6

#define KEYTYPE_ENABLE		1

#define MAX_KEY_LEN			128
#define MAX_VALUE_LEN		512


typedef struct skipnode_s {
#if KEYTYPE_ENABLE
	char key[MAX_KEY_LEN];
	char value[MAX_VALUE_LEN];
#else
    int key;
    int value;
#endif
    struct skipnode_s** forward;
} skipnode_t;


typedef struct skiptable_s {
    int level; 
    skipnode_t* header; 

	int nb_elements;
    pthread_mutex_t lock;
} skiptable_t;

skiptable_t table;


#if KEYTYPE_ENABLE

skipnode_t* createNode(const char *key, char *value, int level) {

	if (!key || !value) return NULL;

    skipnode_t* node = (skipnode_t*)malloc(sizeof(skipnode_t));
    if (!node) return NULL;
    
    strncpy(node->key , key, MAX_KEY_LEN);
	strncpy(node->value, value, MAX_VALUE_LEN);
    node->forward = (skipnode_t**)malloc(sizeof(skipnode_t*) * level);
    if (!node->forward) {
		free(node);
		return NULL;
    }
    
    for (int i = 0; i < level; ++i) {
        node->forward[i] = NULL;
    }
    
    return node;
}


#else
skipnode_t* createNode(int key, int value, int level) {
    skipnode_t* node = (skipnode_t*)malloc(sizeof(skipnode_t));
    node->key = key;
    node->value = value;
    node->forward = (skipnode_t**)malloc(sizeof(skipnode_t*) * level);
    for (int i = 0; i < level; ++i) {
        node->forward[i] = NULL;
    }
    return node;
}
#endif

skiptable_t* createSkipList() {
    skiptable_t* list = (skiptable_t*)malloc(sizeof(skiptable_t));
    list->level = 1;
    list->header = createNode("", "", MAX_LEVEL);
	list->nb_elements = 0;
	
    return list;
}

#if KEYTYPE_ENABLE

void insert(skiptable_t* list, char *key, char *value) {
    int i, level;
    skipnode_t* update[MAX_LEVEL];
    skipnode_t* p;

    p = list->header;
    for (i = list->level - 1; i >= 0; --i) {
        while (p->forward[i] != NULL && strcmp(p->forward[i]->key, key) < 0) {  // strcmp
            p = p->forward[i];
        }
        update[i] = p;
    }

    if (p->forward[0] != NULL && strcmp(p->forward[0]->key, key) == 0) { // exist, 
        
        strncpy(p->forward[0]->value, value, MAX_VALUE_LEN);
        
    } else {
        
        level = 1;
        while (rand() % 2 && level < MAX_LEVEL) {
            ++level;
        }

       
        skipnode_t* node = createNode(key, value, level);
        if (level > list->level) {
            
            for (i = list->level; i < level; ++i) {
                update[i] = list->header;
            }
            list->level = level;
        }

        
        for (i = 0; i < level; ++i) {
            node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = node;
        }
    }
}



#else

void insert(skiptable_t* list, int key, int value) {
    int i, level;
    skipnode_t* update[MAX_LEVEL];
    skipnode_t* p;

    p = list->header;
    for (i = list->level - 1; i >= 0; --i) {
        while (p->forward[i] != NULL && p->forward[i]->key < key) {
            p = p->forward[i];
        }
        update[i] = p;
    }

    if (p->forward[0] != NULL && p->forward[0]->key == key) {
        
        p->forward[0]->value = value;
    } else {
        
        level = 1;
        while (rand() % 2 && level < MAX_LEVEL) {
            ++level;
        }

       
        skipnode_t* node = createNode(key, value, level);
        if (level > list->level) {
            
            for (i = list->level; i < level; ++i) {
                update[i] = list->header;
            }
            list->level = level;
        }

        
        for (i = 0; i < level; ++i) {
            node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = node;
        }
    }
}

#endif

#if KEYTYPE_ENABLE

char *find(skiptable_t* list, char * key) {
    int i;
    skipnode_t* p = list->header;
    for (i = list->level - 1; i >= 0; --i) {
        while (p->forward[i] != NULL && (strcmp(p->forward[i]->key, key) < 0)) {
            p = p->forward[i];
        }
    }
    
    if (p->forward[0] != NULL && (strcmp(p->forward[0]->key, key) == 0)) {
        return p->forward[0]->value;
    } else {
        return NULL;
    }
}


#else

int find(skiptable_t* list, int key) {
    int i;
    skipnode_t* p = list->header;
    for (i = list->level - 1; i >= 0; --i) {
        while (p->forward[i] != NULL && p->forward[i]->key < key) {
            p = p->forward[i];
        }
    }
    if (p->forward[0] != NULL && p->forward[0]->key == key) {
        return p->forward[0]->value;
    } else {
        return -1;
    }
}
#endif


#if KEYTYPE_ENABLE 

void deletes(skiptable_t* list, char *key) {
    int i;
    skipnode_t* update[MAX_LEVEL];
    skipnode_t* p;

    p = list->header;
    for (i = list->level - 1; i >= 0; --i) {
        while (p->forward[i] != NULL && strcmp(p->forward[i]->key, key) < 0) {
            p = p->forward[i];
        }
        update[i] = p;
    }

    if (p->forward[0] != NULL && strcmp(p->forward[0]->key, key) == 0) {
        
        skipnode_t* node = p->forward[0];
        for (i = 0; i < list->level; ++i) {
            if (update[i]->forward[i] == node) {
                update[i]->forward[i] = node->forward[i];
            }
        }
        free(node);

    }
    
}


#else
void deletes(skiptable_t* list, int key) {
    int i;
    skipnode_t* update[MAX_LEVEL];
    skipnode_t* p;

    p = list->header;
    for (i = list->level - 1; i >= 0; --i) {
        while (p->forward[i] != NULL && p->forward[i]->key < key) {
            p = p->forward[i];
        }
        update[i] = p;
    }

    if (p->forward[0] != NULL && p->forward[0]->key == key) {
        
        skipnode_t* node = p->forward[0];
        for (i = 0; i < list->level; ++i) {
            if (update[i]->forward[i] == node) {
                update[i]->forward[i] = node->forward[i];
            }
        }
        free(node);
    }
}
#endif

void print(skiptable_t* list) {
    int i;
    skipnode_t* p;
    for (i = list->level - 1; i >= 0; --i) {
        printf("Level %d: ", i);
        p = list->header->forward[i];
        while (p != NULL) {
        #if KEYTYPE_ENABLE
			printf("(%s, %s) ", p->key, p->value);
        #else
            printf("(%d,%d) ", p->key, p->value);
        #endif
            p = p->forward[i];
        }
        printf("\n");
    }
    printf("\n");
}



// init_skiptable
int init_skiptable(skiptable_t *table) {

	if (!table) return -1;

	table->level = 1;
    table->header = createNode("", "", MAX_LEVEL);

    pthread_mutex_init(&table->lock, NULL);
	
	return 0;
}

// dest_skiptable
void dest_skiptable(skiptable_t *table) {

	if (!table) return ;

	int i;
    skipnode_t* p;
    for (i = table->level - 1; i >= 0; --i) {

		p = table->header->forward[i];

		while (p != NULL) {
			pthread_mutex_lock(&table->lock);
			deletes(table, p->key);
			pthread_mutex_unlock(&table->lock);

			p = p->forward[i];
		}
    }

    free(table->header);

}

// put_kv_skiptable
int put_kv_skiptable(skiptable_t *table, char *key, char *value) {

	if (!table || !key || !value) return -1;

	pthread_mutex_lock(&table->lock);
	insert(table, key, value);
	table->nb_elements ++;
	pthread_mutex_unlock(&table->lock);

	return 0;

}


// get_kv_skiptable
char *get_kv_skiptable(skiptable_t *table, char *key) {

	if (!table || !key) return NULL;

	return find(table, key);
}

int count_kv_skiptable(skiptable_t *table) {

	return table->nb_elements;

}


int delete_kv_skiptable(skiptable_t *table, char *key) {

	char *value = find(table, key);
	if (value == NULL) { // no exist
		return -1;
	}

	deletes(table, key);

    table->nb_elements --;
	return 0;

}

int exist_kv_skiptable(skiptable_t *table, char *key) {

	char *value = find(table, key);
	if (value == NULL) { // no exist
		return 0;
	}
	return 1;
}

// int main () {

//     init_skiptable(&table);
//     char *k1 = "Name";
// 	char *v1 = "zxm";
//     char *k2 = "home";
// 	char *v2 = "china";

//     put_kv_skiptable(&table, k1, v1);
//     put_kv_skiptable(&table, k2, v2);
//     int r1 = count_kv_skiptable(&table);
//     print(&table);
//     printf("r1:%d\n",r1);

//     delete_kv_skiptable(&table, k1);
//     int r2 = count_kv_skiptable(&table);
//     print(&table);
//     printf("r2:%d\n",r2);
// }