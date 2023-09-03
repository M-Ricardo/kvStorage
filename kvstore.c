// gcc -o kvstore kvstore.c -I ./NtyCo/core/ -L ./NtyCo/ -lntyco -lpthread -ldl 


#include "nty_coroutine.h"
#include "hash.h"
#include "rbtree.h"
#include "skiptable.h"
#include "dhash.h"

#include <arpa/inet.h>

// INFO 为 printf 时，表示正常打印，当为 // ，表示屏蔽打印，便于后续的qps测试
#define INFO	//

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

// array: set, get, count, delete, exist
// rbtree: rset, rget, rcount, rdelete, rexist
// hash: hset, hget, hcount, hdelete, hexist
// skiptable: zset, zget, zcount, zdelete, zexist



typedef enum kvs_cmd_e {
	KVS_CMD_START = 0,

	// array
	KVS_CMD_SET = KVS_CMD_START,
	KVS_CMD_GET,
	KVS_CMD_COUNT,
	KVS_CMD_DELETE,
	KVS_CMD_EXIST,

	// rbtree
	KVS_CMD_RSET,
	KVS_CMD_RGET,
	KVS_CMD_RCOUNT,
	KVS_CMD_RDELETE,
	KVS_CMD_REXIST,

	// hash
	KVS_CMD_HSET,
	KVS_CMD_HGET,
	KVS_CMD_HCOUNT,
	KVS_CMD_HDELETE,
	KVS_CMD_HEXIST,

	// dhash
	KVS_CMD_DSET,
	KVS_CMD_DGET,
	KVS_CMD_DCOUNT,
	KVS_CMD_DDELETE,
	KVS_CMD_DEXIST,

	// skiptable
	KVS_CMD_ZSET,
	KVS_CMD_ZGET,
	KVS_CMD_ZCOUNT,
	KVS_CMD_ZDELETE,
	KVS_CMD_ZEXIST,

	KVS_CMD_END	= KVS_CMD_ZEXIST,

	// 格式出错
	KVS_CMD_ERROR,
	// 断开
	KVS_CMD_QUIT,

} kvs_cmd_t;

// 命令集
const char *commands[] = {

	"SET", "GET", "COUNT", "DELETE", "EXIST",
	"RSET", "RGET", "RCOUNT", "RDELETE", "REXIST",
	"HSET", "HGET", "HCOUNT", "HDELETE", "HEXIST",
	"DSET", "DGET", "DCOUNT", "DDELETE", "DEXIST",
	"ZSET", "ZGET", "ZCOUNT", "ZDELETE", "ZEXIST"

};

// 为了以后优化的可能，把内存开辟释放封装
void *kvs_malloc(size_t size) {
	return malloc(size);
}

void kvs_free(void *ptr) {
	return free(ptr);
}

//-------------------------------------- array -----------------------------------------
#define KVS_ARRAY_ITEM_SIEZ 		1024

typedef struct kvs_array_item_s {
	char *key;
	char *value;
} kvs_array_item_t;

// kvs_array_table 存储插入的 kv
kvs_array_item_t kvs_array_table [KVS_ARRAY_ITEM_SIEZ] = {0};

// 查找 key 在 kvs_array_table 的位置
kvs_array_item_t *kvs_array_search_item (char *key){

	if (!key) return NULL;

	int i = 0;
	// 注意由于前面的 key 被删除而出现的 key == NULL ， strcmp 不能与 NULL 比较
	for (i = 0;i < KVS_ARRAY_ITEM_SIEZ; i++) {
		if (kvs_array_table[i].key != NULL && strcmp(kvs_array_table[i].key , key) == 0) {
			return &kvs_array_table[i];
		}
	}

	return NULL;

}

// KVS_CMD_EXIST: 判断 key 是否存在，存在返回 1 
int kvs_array_exist (char *key) {
	if (kvs_array_search_item(key) != NULL) return 1;
}

// KVS_CMD_SET：插入 kv， 成功返回 0
int array_count = 0; 	// 已插入元素的个数
int kvs_array_set (char *key, char *value) {
	
	if (key == NULL || value == NULL || array_count == KVS_ARRAY_ITEM_SIEZ - 1) {
		return -1;
	}

	// key 已存在，不能插入
	if (kvs_array_exist(key)) {
		return -1;
	}

	char *kvs_key = kvs_malloc(strlen(key) + 1);
	if (kvs_key == NULL) return -1;
	strncpy(kvs_key, key, strlen(key) + 1);

	char *kvs_value = kvs_malloc(strlen(value) + 1);
	if (kvs_value == NULL) {
		free(kvs_key);
		return -1;
	}
	strncpy(kvs_value, value, strlen(value) + 1);

	int i = 0;
	for (i = 0;i < KVS_ARRAY_ITEM_SIEZ; i++) {
		if (kvs_array_table[i].key == NULL && kvs_array_table[i].value == 0) {
			break;
		}
	}

	kvs_array_table[i].key = kvs_key;
	kvs_array_table[i].value = kvs_value;
	array_count++;

	return 0;
}

// KVS_CMD_GET：获取 key 对应的value
char *kvs_array_get(char *key) {
	kvs_array_item_t * item = kvs_array_search_item(key);
	if (item) {
		return item->value;
	}
	return NULL;
}

// KVS_CMD_COUNT：统计以及插入多少个 key
int kvs_array_count (void) {

	return array_count;

}

// KVS_CMD_DELETE：删除 key
int kvs_array_delete(char *key) {
	
	if (key == NULL) return -1;

	kvs_array_item_t * item = kvs_array_search_item(key);
	if (item == NULL) {
		return -1;
	}

	if (item->key) {
		kvs_free(item->key);
		item->key = NULL;
	}

	if (item->value) {
		kvs_free(item->value);
		item->value = NULL;
	}
	
	array_count--;

	return 0;

}




//-------------------------------------- rbtree -----------------------------------------
// typedef struct _rbtree rbtree_t;
// int init_rbtree(rbtree_t *tree);
// void dest_rbtree(rbtree_t *tree);
// int put_kv_rbtree(rbtree_t *tree, char *key, char *value);
// char *get_kv_rbtree(rbtree_t *tree, char *key);
// int count_kv_rbtree(rbtree_t *tree);
// int exist_kv_rbtree(rbtree_t *tree, char *key);
// int delete_kv_rbtree(rbtree_t *tree, char *key);
// extern rbtree_t tree;


// KVS_CMD_REXIST
int kvs_rbtree_exist (char *key) {
	return exist_kv_rbtree(&tree, key);
}

// KVS_CMD_RSET
int kvs_rbtree_set (char *key, char *value) {
	return put_kv_rbtree(&tree, key, value);
}

// KVS_CMD_RGET
char *kvs_rbtree_get (char *key) {
	return 	get_kv_rbtree(&tree, key);
}

// KVS_CMD_RCOUNT
int kvs_rbtree_count(void) {
	return count_kv_rbtree(&tree);
}
// KVS_CMD_RDELETE
int kvs_rbtree_delete(char *key) {
	return delete_kv_rbtree(&tree, key);
}



//-------------------------------------- hash -----------------------------------------

// typedef struct hashtable_s hashtable_t;
// int init_hashtable(hashtable_t *hash);
// void dest_hashtable(hashtable_t *hash);
// int put_kv_hashtable(hashtable_t *hash, char *key, char *value);
// char *get_kv_hashtable(hashtable_t *hash, char *key);
// int count_kv_hashtable(hashtable_t *hash);
// int delete_kv_hashtable(hashtable_t *hash, char *key);
// int exist_kv_hashtable(hashtable_t *hash, char *key);
// extern hashtable_t hash;

int kvs_hash_set(char *key, char *value)  {
	return put_kv_hashtable(&hash, key, value);
}

char *kvs_hash_get(char *key) {
	return 	get_kv_hashtable(&hash, key);
}

int kvs_hash_count(void) {
	return count_kv_hashtable(&hash);
}

int kvs_hash_exist(char *key) {
	return exist_kv_hashtable(&hash, key);
}

int kvs_hash_delete(char *key) {
	return delete_kv_hashtable(&hash, key);
}

//-------------------------------------- dhash -----------------------------------------
hash_table *dhash = NULL;
hash_table* init_dhashtable(){
    hash_table *table = create_hash_table(INITIAL_SIZE);
    if (!table){
        return NULL;
    }
    return dhash = table;
}

int kvs_dhash_set(char *key, char *value)  {
	return put_kv_dhashtable(dhash, key, value);
}

char *kvs_dhash_get(char *key) {
	printf("key: %s\n",key);
	return 	get_kv_dhashtable(dhash, key);
}

int kvs_dhash_count(void) {
	return count_kv_dhashtable(dhash);
}

int kvs_dhash_exist(char *key) {
	return exist_kv_dhashtable(dhash, key);
}

int kvs_dhash_delete(char *key) {
	return delete_kv_dhashtable(dhash, key);
}

//----------------------------------- skiptable --------------------------------------

int kvs_skiptable_set(char *key, char *value)  {
	return put_kv_skiptable(&table, key, value);
}

char *kvs_skiptable_get(char *key) {
	return 	get_kv_skiptable(&table, key);
}

int kvs_skiptable_count(void) {
	return count_kv_skiptable(&table);
}

int kvs_skiptable_exist(char *key) {
	return exist_kv_skiptable(&table, key);
}

int kvs_skiptable_delete(char *key) {
	return delete_kv_skiptable(&table, key);
}



//---------------------------------------------------------------------------------------------
//-------------------------------------- 引擎层 -----------------------------------------

void init_kvengine(void) {
	init_rbtree(&tree);
	init_hashtable(&hash);
	init_skiptable(&table);
	init_dhashtable();
}


void dest_kvengine(void) {
	dest_rbtree(&tree);
	dest_hashtable(&hash);
	dest_skiptable(&table);
}
//---------------------------------------------------------------------------------------------
//-------------------------------------- 协议层 -----------------------------------------

#define MAX_TOKENS		32
#define CLINET_MSG_LENGTH		1024		// client发送msg的最大长度


// 根据msg，解析其具体的命令协议
int kvs_parser_protocol (char *msg, char **tokens, int count) {
	if (tokens == NULL || tokens[0] == NULL || count == 0) {
		return KVS_CMD_ERROR;
	}

	// 判断命令，即tokens[0]，是否在命令集中
	int cmd = KVS_CMD_START;
	for (cmd = KVS_CMD_START; cmd < KVS_CMD_END; cmd++) {
		if (strcmp(tokens[0], commands[cmd]) == 0) {
			break;
		}
	}

	// 根据cmd，选择对应的命令
	switch (cmd) {
	//----------------------------------- array --------------------------------------
	case KVS_CMD_SET: {
		assert(count == 3);		// SET NAME ZXM，应该有三个

		int ret = 0;
		int res = kvs_array_set(tokens[1], tokens[2]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");			
		}
		return ret;
	}

	case KVS_CMD_GET: {
		assert(count == 2);

		int ret = 0;
		char *value = kvs_array_get(tokens[1]);
		if (value) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "%s\r\n", value);
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED: NO EXIST\r\n");
		}

		return ret;
	}

	case KVS_CMD_COUNT: {
		assert(count == 1);

		int res = kvs_array_count();
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;
	}

	case KVS_CMD_DELETE: {
		assert(count == 2);

		int ret = 0;
		int res = kvs_array_delete(tokens[1]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");
		}
		return ret;
	}
	case KVS_CMD_EXIST: {
		assert(count == 2);

		int res = kvs_array_exist(tokens[1]);
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;
	}

	//----------------------------------- rbtree --------------------------------------
	case KVS_CMD_RSET: {
		assert(count == 3);		// SET NAME ZXM，应该有三个

		int ret = 0;
		int res = kvs_rbtree_set(tokens[1], tokens[2]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");			
		}
		return ret;
	}
	
	case KVS_CMD_RGET: {
		assert(count == 2);

		int ret = 0;
		char *value = kvs_rbtree_get(tokens[1]);
		if (value) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "%s\r\n", value);
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED: NO EXIST\r\n");
		}

		return ret;		
	}

	case KVS_CMD_RCOUNT: {
		assert(count == 1);

		int res = kvs_rbtree_count();
	
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;		
	}	

	case KVS_CMD_RDELETE: {
		assert(count == 2);

		int ret = 0;
		int res = kvs_rbtree_delete(tokens[1]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");
		}
		return ret;		
	}	

	case KVS_CMD_REXIST: {
		assert(count == 2);

		int res = kvs_rbtree_exist(tokens[1]);
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;		
	}
	
	//----------------------------------- hash --------------------------------------
	case KVS_CMD_HSET: {
		assert(count == 3);		// SET NAME ZXM，应该有三个

		int ret = 0;
		int res = kvs_hash_set(tokens[1], tokens[2]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");			
		}
		return ret;
	}

	case KVS_CMD_HGET: {
		assert(count == 2);

		int ret = 0;
		char *value = kvs_hash_get(tokens[1]);
		if (value) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "%s\r\n", value);
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED: NO EXIST\r\n");
		}

		return ret;
	}

	case KVS_CMD_HCOUNT: {
		assert(count == 1);

		int res = kvs_hash_count();
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;
	}

	case KVS_CMD_HDELETE: {
		assert(count == 2);

		int ret = 0;
		int res = kvs_hash_delete(tokens[1]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");
		}
		return ret;
	}
	case KVS_CMD_HEXIST: {
		assert(count == 2);

		int res = kvs_hash_exist(tokens[1]);
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;
	}


	//----------------------------------- dhash --------------------------------------
	case KVS_CMD_DSET: {
		assert(count == 3);		// SET NAME ZXM，应该有三个

		int ret = 0;
		int res = kvs_dhash_set(tokens[1], tokens[2]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");			
		}
		//print_dhash(dhash);
		return ret;
	}

	case KVS_CMD_DGET: {
		assert(count == 2);

		int ret = 0;
		char *value = kvs_dhash_get(tokens[1]);
		if (value) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "%s\r\n", value);
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED: NO EXIST\r\n");
		}

		return ret;
	}

	case KVS_CMD_DCOUNT: {
		assert(count == 1);

		int res = kvs_dhash_count();
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;
	}

	case KVS_CMD_DDELETE: {
		assert(count == 2);

		int ret = 0;
		int res = kvs_dhash_delete(tokens[1]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");
		}
		return ret;
	}
	case KVS_CMD_DEXIST: {
		assert(count == 2);

		int res = kvs_dhash_exist(tokens[1]);
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;
	}



	//----------------------------------- skiptable --------------------------------------

	case KVS_CMD_ZSET: {
		assert(count == 3);		// SET NAME ZXM，应该有三个

		int ret = 0;
		int res = kvs_skiptable_set(tokens[1], tokens[2]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");			
		}
		return ret;
	}
	
	case KVS_CMD_ZGET: {
		assert(count == 2);

		int ret = 0;
		char *value = kvs_skiptable_get(tokens[1]);
		if (value) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "%s\r\n", value);
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED: NO EXIST\r\n");
		}

		return ret;		
	}

	case KVS_CMD_ZCOUNT: {
		assert(count == 1);

		int res = kvs_skiptable_count();
	
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;		
	}	

	case KVS_CMD_ZDELETE: {
		assert(count == 2);

		int ret = 0;
		int res = kvs_skiptable_delete(tokens[1]);
		if (!res) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "SUCCESS\r\n");
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED\r\n");
		}
		return ret;		
	}	

	case KVS_CMD_ZEXIST: {
		assert(count == 2);

		int res = kvs_skiptable_exist(tokens[1]);
		
		memset(msg, 0, CLINET_MSG_LENGTH);
		int ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;		
	}

	}

	return 0;
}


/*分割msg，比如 msg为 SET NAME ZXM ,分割为SET,NAME,ZXM，分别存储在tokens[]
 * tokens[0]: SET	--------- 对应的是命令 cmd
 * tokens[1]: NAME	--------- 对应的是命令 key
 * tokens[2]: ZXM	--------- 对应的是命令 value
 */
int kvs_spilt_tokens (char **tokens, char *msg) {
	
	char *token = strtok(msg, " ");   // 按空格分割

	int count  = 0;
	while (token != NULL) {
		tokens[count++] = token;
		token  = strtok(NULL, " "); 
	}

	return count;
}


// 解析协议
int kvs_protocol (char *msg, int length) {
	char *tokens[MAX_TOKENS] = {0};
	// 分割msg
	int count = kvs_spilt_tokens(tokens, msg);
	// 根据分割后的 msg ，解析其具体命令协议
	// msg：命令    tokens：分割后的 msg   
	INFO("msg: %s, count: %d\n",msg,count);
	return  kvs_parser_protocol(msg, tokens, count);
}




//---------------------------------------------------------------------------------------------
//--------------------------------------NtyCo底层的协程-----------------------------------------

void server_reader(void *arg) {
	int fd = *(int *)arg;
	int ret = 0;

	while (1) {
		
		char buf[CLINET_MSG_LENGTH] = {0};
		// 接收msg，存放到buf中
		ret = nty_recv(fd, buf, CLINET_MSG_LENGTH, 0);
		if (ret > 0) {
			INFO("read from server: %.*s\n", ret, buf);

			// 根据协议解析msg： rec: SET NAME ZXM  
			// 解析后：SET\r\n NAME\r\n ZXM\r\n
			ret = kvs_protocol(buf, ret);

			// 发送解析后的msg
			ret = nty_send(fd, buf, ret, 0);
			if (ret == -1) {
				nty_close(fd);
				break;
			}
		} else if (ret == 0) {	
			nty_close(fd);
			break;
		}

	}
}


void server(void *arg) {

	unsigned short port = *(unsigned short *)arg;
	free(arg);

	int fd = nty_socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return ;

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;				// 设置地址族为IPv4
	local.sin_port = htons(port);			// 设置端口号
	local.sin_addr.s_addr = INADDR_ANY;		// 设置IP地址, INADDR_ANY 是一个常量，表示可以接受来自任意 IP 地址的连接。
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));

	listen(fd, 20);
	INFO("listen port : %d\n", port);

	//获取当前的时间和日期信息
	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		int cli_fd = nty_accept(fd, (struct sockaddr*)&remote, &len);
		if (cli_fd % 1000 == 999) {

			struct timeval tv_cur;
			memcpy(&tv_cur, &tv_begin, sizeof(struct timeval));
			
			gettimeofday(&tv_begin, NULL);
			int time_used = TIME_SUB_MS(tv_begin, tv_cur);
			
			INFO("client fd : %d, time_used: %d\n", cli_fd, time_used);
		}
		INFO("new client comming\n");

		nty_coroutine *read_co;
		nty_coroutine_create(&read_co, server_reader, &cli_fd);

	}
	
}



int main(int argc, char *argv[]) {

	init_kvengine();

	nty_coroutine *co = NULL;

	unsigned short base_port = 9999;

	unsigned short *port = calloc(1, sizeof(unsigned short));
	*port = base_port ;

	// 为每一个连接端口创建一个协程
	nty_coroutine_create(&co, server, port); 


	nty_schedule_run(); //run

	dest_kvengine();

	return 0;
}



