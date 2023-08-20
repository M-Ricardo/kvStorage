// gcc -o kvstore kvstore.c -I ./NtyCo/core/ -L ./NtyCo/ -lntyco -lpthread -ldl 


#include "nty_coroutine.h"

#include <arpa/inet.h>
#define ENABLE_ARRAY		1

#define MAX_CLIENT_NUM			1000000
#define MAX_PORT 				1
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


// array: set, get, count, delete, exist
// rbtree: rset, rget, rcount, rdelete, rexist
// btree: bset, bget, bcount, bdelete, bexist
// hash: hset, hget, hcount, hdelete, hexist
// skiptable: zset, zget, zcount, zdelete, zexist


typedef enum kvs_cmd_e {

	KVS_CMD_START = 0,

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

	// skiptable
	KVS_CMD_ZSET,
	KVS_CMD_ZGET,
	KVS_CMD_ZCOUNT,
	KVS_CMD_ZDELETE,
	KVS_CMD_ZEXIST,

	// 格式出错
	KVS_CMD_ERROR,
	// 断开
	KVS_CMD_QUIT,
	// 
	KVS_CMD_END
} kvs_cmd_t;

const char *commands[] = {
	"SET", "GET", "COUNT", "DELETE", "EXIST",
	"RSET", "RGET", "RCOUNT", "RDELETE", "REXIST",
	"HSET", "HGET", "HCOUNT", "HDELETE", "HEXIST",
	"ZSET", "ZGET", "ZCOUNT", "ZDELETE", "ZEXIST"
};

typedef enum kvs_result_e {
	KVS_RESULT_SUCCESS,
	KVS_RESULT_FAILED
} kvs_result_t;


const char *result[] = {
	"SUCCESS",
	"FAILED"
};

#define MAX_TOKENS  			32
#define CLINET_MSG_LENGTH		1024

void *kvs_malloc(size_t size) {
	return malloc(size);
}

void kvs_free(void *ptr) {
	return free(ptr);
}


#if ENABLE_ARRAY

typedef struct kvs_array_item_s {

	char *key;
	char *value;

} kvs_array_item_t;

#define KVS_ARRAY_ITEM_SIZE		1024

kvs_array_item_t array_table[KVS_ARRAY_ITEM_SIZE] = {0};

int array_count = 0;

// 寻找key所对应的array_table[i]
kvs_array_item_t *kvs_array_searcha_item(const char *key) {
	if (!key) return NULL;

	int i = 0;
	for (i = 0; i < array_count; i++){
		if (0 == strcmp(array_table[i].key, key)){
			return &array_table[i];
		}
	}

	return NULL;
}

// KVS_CMD_EXIST: 判断key是否存在与array_table[]，存在返回 1 
int kvs_array_exist(const char *key) {
	return (kvs_array_searcha_item(key) != NULL);
}


// KVS_CMD_SET：往array_table[]插入[key,value]
int kvs_array_set(const char *key, const char *value) {
	// array_count == KVS_ARRAY_ITEM_SIZE - 1 达到最大内存
	if (key == NULL || value == NULL || array_count == KVS_ARRAY_ITEM_SIZE - 1) return -1;

	// key已经存在，不能再插入
	if (kvs_array_exist(key)) return -1; 

	char *kcopy = kvs_malloc(strlen(key) + 1);
	if (kcopy == NULL) return -1;
	strncpy(kcopy, key, strlen(key) + 1);

	char *vcopy = kvs_malloc(strlen(value) + 1);
	if (vcopy == NULL) {
		kvs_free(kcopy);
		return -1;
	}
	strncpy(vcopy, value, strlen(value) + 1);

	// 在第一个空的位置插入
	int i = 0;
	for (i=0; i < KVS_ARRAY_ITEM_SIZE; i++) {
		if (array_table[i].key == NULL && array_table[i].value == NULL) {
			break;
		}
	}
	array_table[i].key = kcopy;
	array_table[i].value = vcopy;
	array_count++;

	return 0;
}


//KVS_CMD_GET: 获取key对应的value
char *kvs_array_get(const char *key){
	kvs_array_item_t *item = kvs_array_searcha_item(key);

	if (item) {
		return item->value;
	}
	
	return NULL;
}
//KVS_CMD_COUNT
int kvs_array_count(void) {
	return array_count;
}

//KVS_CMD_DELETE
int kvs_array_delete(const char *key){
	if (key == NULL) return -1;
	kvs_array_item_t *item = kvs_array_searcha_item(key);
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

	return 0;
}

#endif

// 根据msg，解析其命令协议
int kvs_parser_protocol(char *msg, char **tokens, int count) {
	if (tokens == NULL || tokens[0] == NULL || count == 0) return KVS_CMD_ERROR;

	// 判断msg的命令tokens[0]是否在命令集commands[]中，若存在，对于kvs_cmd_t里的哪一个cmd
	int cmd = KVS_CMD_START;
	for (cmd = KVS_CMD_START; cmd <= KVS_CMD_ZEXIST; cmd++){
		// 存在返回0，停止for循环
		if (0 == strcmp(tokens[0], commands[cmd])) break;
	}

	// 根据cmd，选择对应的命令
	switch (cmd) {
	// array
	case KVS_CMD_SET : {
		assert(count == 3);
		printf("cmd: %s\n", tokens[0]); 	//	SET
		printf("key: %s\n", tokens[1]);		// NAME
		printf("value: %s\n", tokens[2]);	// ZXM

		int ret = 0;
		// 往array_table[]插入[key,value]，成功返回0
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

	case KVS_CMD_GET :{
		int ret = 0;
		char *value = kvs_array_get(tokens[1]);
		if (value) {
			memset(msg, 0, CLINET_MSG_LENGTH);
			// 返回值为格式化后的字符串长度（不包括终止符 \0）
			ret = snprintf(msg, CLINET_MSG_LENGTH, "%s\r\n", value);
		} else {
			memset(msg, 0, CLINET_MSG_LENGTH);
			ret = snprintf(msg, CLINET_MSG_LENGTH, "FAILED: NO EXIST\r\n");
		}

		return ret;

	}

	case KVS_CMD_COUNT : {
		int res = kvs_array_count();
		memset(msg, 0, CLINET_MSG_LENGTH);
		int	ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;
	}

	case KVS_CMD_DELETE : 

	case KVS_CMD_EXIST : {
		int res = kvs_array_exist(tokens[1]);
		memset(msg, 0, CLINET_MSG_LENGTH);
		int	ret = snprintf(msg, CLINET_MSG_LENGTH, "%d\r\n", res);

		return ret;		
	}

	// rbtree
	case KVS_CMD_RSET :
	case KVS_CMD_RGET :
	case KVS_CMD_RCOUNT :
	case KVS_CMD_RDELETE :
	case KVS_CMD_REXIST :

	// hash
	case KVS_CMD_HSET :
	case KVS_CMD_HGET :
	case KVS_CMD_HCOUNT :
	case KVS_CMD_HDELETE :
	case KVS_CMD_HEXIST :

	// skiptable
	case KVS_CMD_ZSET :
	case KVS_CMD_ZGET :
	case KVS_CMD_ZCOUNT :
	case KVS_CMD_ZDELETE :
	case KVS_CMD_ZEXIST :
	}
	return 0;
}

// 分割msg
int kvs_split_tokens(char **tokens, char *msg) { 	
	int count = 0;
	char *token = strtok(msg, " ");

	while (token != NULL) {
		tokens[count++] = token;
		token = strtok(NULL, " ");
	}

	return count;
}

// 解析协议
int kvs_protocol(char *msg, int length){

	char *tokens[MAX_TOKENS] = {0};

	// 分割msg，比如msg为SET NAME ZXM ,分割为SET,NAME,ZXM，分别存储在tokens[]
	int count = kvs_split_tokens(tokens, msg);

	int i = 0;
	for (i = 0; i < count; i++){
		printf("%s\n", tokens[i]);
	}

	// 根据msg，解析其命令协议
	// msg：命令    tokens：分割后的msg   
	return kvs_parser_protocol(msg, tokens, count);
}	



//--------------------------------------NtyCo底层的协程-----------------------------------------

void server_reader(void *arg) {
	int fd = *(int *)arg;
	int ret = 0;

	while (1) {
		
		char buf[CLINET_MSG_LENGTH] = {0};
		// 接收msg，存放到buf中
		ret = nty_recv(fd, buf, CLINET_MSG_LENGTH, 0);
		if (ret > 0) {
			printf("read from server: %.*s\n", ret, buf);

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
	printf("listen port : %d\n", port);

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
			
			printf("client fd : %d, time_used: %d\n", cli_fd, time_used);
		}
		printf("new client comming\n");

		nty_coroutine *read_co;
		nty_coroutine_create(&read_co, server_reader, &cli_fd);

	}
	
}



int main(int argc, char *argv[]) {
	nty_coroutine *co = NULL;

	int i = 0;
	unsigned short base_port = 9999;

	for (i = 0;i < MAX_PORT; i ++) {
		unsigned short *port = calloc(1, sizeof(unsigned short));
		*port = base_port + i;

		// 为每一个连接端口创建一个协程
		nty_coroutine_create(&co, server, port); 
	}

	nty_schedule_run(); //run

	return 0;
}



