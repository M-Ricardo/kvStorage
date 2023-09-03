#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_SIZE 16  
#define GROWTH_FACTOR 2  

typedef struct {
    char *key;
    char *value;
} hash_node;

typedef struct {
    int size;            
    int count;           
    hash_node **nodes;   
} hash_table;

hash_node *create_hash_node( char *key,  char *value) {
    hash_node *node = (hash_node *) malloc(sizeof(hash_node));
    node->key = strdup(key);
    node->value = strdup(value);
    
    return node;
}

void destroy_hash_node(hash_node *node) {
    free(node->key);
    free(node);
}

hash_table *create_hash_table(int size) {
    hash_table *table = (hash_table *) malloc(sizeof(hash_table));
    
    table->size = size;
    table->count = 0;
    
    table->nodes = (hash_node **) calloc(size, sizeof(hash_node *));
    
    return table;
}

void destroy_hash_table(hash_table *table) {
    for (int i = 0; i < table->size; i++) {
        hash_node *node = table->nodes[i];
        
        if (node != NULL) {
            destroy_hash_node(node);
        }
    }
    
    free(table->nodes);
    free(table);
}

int hash_function( char *key, int size) {
    unsigned long int index = 0;
    
    for (int i = 0; i < strlen(key); i++) {
        index = index * 37 + key[i];
    }
    
    index = index % size;

    return (int) index;
}



//   =======



int put_kv_dhashtable(hash_table *table,  char *key,  char *value) {
    // 如果当前节点数目超过了哈希表大小的一半，则进行扩展操作
    if ((table->count * 2) >= table->size) {
        hash_table *new_table = create_hash_table(table->size * GROWTH_FACTOR);

        for (int i = 0; i < table->size; i++) {
            hash_node *node = table->nodes[i];
            
            if (node != NULL) {
                put_kv_dhashtable(new_table, node->key, node->value);
            }
        }

        table->size *= GROWTH_FACTOR;
        table->count = new_table->count;
		
        hash_node **tmp_nodes = table->nodes;
        table->nodes = new_table->nodes;
        new_table->nodes = tmp_nodes;
        
		new_table->size /= GROWTH_FACTOR;
        destroy_hash_table(new_table);
		
    }
    
    // 计算该键对应的哈希值，并找到其对应在哈希表中的位置
    int index = hash_function(key, table->size);
    while (table->nodes[index] != NULL) {
		if (table->nodes[index]->key != NULL && strcmp(table->nodes[index]->key, key)) {
			index++;
        
	        if (index == table->size) {
	            index = 0;
	        }
		}
        
    }
    // 如果该键对应的节点已经存在，则更新其值，否则创建新的节点
    if (table->nodes[index] != NULL) {
        destroy_hash_node(table->nodes[index]);
    } else {
        table->count++;
    }
    
    table->nodes[index] = create_hash_node(key, value);

    return 0;
}

int delete_kv_dhashtable(hash_table *table, char *key) {
    // 计算要删除的键的哈希值，并找到其对应在哈希表中的位置
    int index = hash_function(key, table->size);
    while (table->nodes[index] != NULL) {
        if (table->nodes[index]->key != NULL && strcmp(table->nodes[index]->key, key) == 0) {
            destroy_hash_node(table->nodes[index]);
            table->nodes[index] = NULL;
            table->count--;
            return 0;
        } else {
            index++;
            
            if (index == table->size) {
                index = 0;
            }
        }
    }

    return -1;  // 没有找到要删除的键
}


char * get_kv_dhashtable(hash_table *table,  char *key) {
    int index = hash_function(key, table->size);
    while (table->nodes[index] != NULL && strcmp(table->nodes[index]->key, key)) {
        index++;
        
        if (index == table->size) {
            index = 0;
        }
    }
    
    return (table->nodes[index] == NULL) ? NULL : table->nodes[index]->value;
}


int count_kv_dhashtable(hash_table *table) {
	return table->count;
}


int exist_kv_dhashtable(hash_table *table, char *key) {

	char *value = get_kv_dhashtable(table, key);
	if (value) return 1;
	else return 0;
}

void print_dhash(hash_table *table) {
    if (table == NULL) {
        printf("table is NULL.\n");
        return;
    }

    printf("Printing table:\n");
    printf("-----------------\n");

    for (int i = 0; i < table->size; i++) {
        hash_node *node = table->nodes[i];
        if (node != NULL) {
            printf("Key: %s, Value: %s\n", node->key, node->value);
        }
        
    }
    printf("-----------------\n");
}


#if 0
int main() {
    hash_table *table = create_hash_table(INITIAL_SIZE);
    
    put_kv_dhashtable(table, "apple", "10");
    put_kv_dhashtable(table, "banana", "20");
    put_kv_dhashtable(table, "orange", "30");
	put_kv_dhashtable(table, "Name", "zxm");
    put_kv_dhashtable(table, "Nationality", "China");
	put_kv_dhashtable(table, "bcd", "10");
    put_kv_dhashtable(table, "cde", "20");
    put_kv_dhashtable(table, "edf", "30");
	put_kv_dhashtable(table, "grd", "20");
    put_kv_dhashtable(table, "dsd", "30");

    printf("apple: %s\n", get_kv_dhashtable(table, "apple"));
    printf("Nationality: %s\n", get_kv_dhashtable(table, "Nationality"));
    printf("Name: %s\n", get_kv_dhashtable(table, "Name"));
    printf("count: %d\n", count_kv_dhashtable(table));

    delete_kv_dhashtable(table, "apple");

    printf("count: %d\n", count_kv_dhashtable(table));

    print_dhash(table);
    
    destroy_hash_table(table);
    
   return 0;
}

#endif