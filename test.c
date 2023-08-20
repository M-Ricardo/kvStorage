#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>


#define MAX_MSG_LENGTH 		1024

// 开启表示是否进行qps测试
#define ENABLE_TEXT_QPS		1
// qps测试的次数
#define TEST_COUNT			100000
// INFO 为 printf 时，表示正常打印，当为 // ，表示屏蔽打印，便于后续的qps测试
#define INFO	//

#define TIME_SUB_MS(t1, t2)			((t1.tv_sec - t2.tv_sec) * 1000 + (t1.tv_usec - t2.tv_usec) / 1000	)
int connect_kvstore(const char *ip ,int port){

	int connfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in kvs_addr;
	memset(&kvs_addr, 0, sizeof(struct sockaddr_in)); 

	kvs_addr.sin_family = AF_INET;
	kvs_addr.sin_addr.s_addr = inet_addr(ip); // inet_addr把ip转为点分十进制
	kvs_addr.sin_port = htons(port);

	int ret = connect(connfd, (struct sockaddr*)&kvs_addr, sizeof(struct sockaddr_in));
	// ret = 0，成功
	if (ret) { 
		perror("connect\n");
		return -1;
	}

	return connfd;
}

// 发送msg
int send_msg(int connfd, char *msg) {
	int res = send(connfd, msg, strlen(msg), 0);
	if (res < 0) {
		exit(1);
	}

	return res;
}

// 接收数据并存入msg
int recv_msg(int connfd, char *msg) {
	int res = recv(connfd, msg, MAX_MSG_LENGTH, 0);
	if (res < 0){
		exit(1);
	}

	return res;
}

// 对比接收到的结果 result 与 应返回的结果 pattern 是否一致
void equals (char *pattern, char *result, char *casename) {

	if (0 == strcmp(pattern, result)) {
		INFO(">> PASS --> %s\n", casename);
	} else {
		INFO(">> FAILED --> '%s' != '%s'\n", pattern, result);
	}
}

// cmd: 命令			pattern: 应返回的结果		casename：测试的名称
// 比如测试 SET NAME ZXM，cmd="SET NAME ZXM", pattern="SUCCESS\r\n", 测试的名称casename
void test_case (int connfd, char *cmd, char *pattern, char *casename) {

	char result[MAX_MSG_LENGTH] = {0};
	// 发送命令cmd
	send_msg(connfd, cmd);
	// 接收命令处理后的结果，存入result
	recv_msg(connfd, result);
	// 对比接收到的结果 result 与 应返回的结果 pattern 是否一致
	equals(pattern, result, casename);
}


#if ENABLE_TEXT_QPS

//------------------------------------- array 测试 ---------------------------------
void array_testcase_10w (int connfd) {
	int i = 0;
	for (i = 0; i < TEST_COUNT; i++) {
		char msg[512] = {0};
		snprintf(msg, 512, "SET key%d value%d", i ,i);

		char casenames[64] = {0};
		snprintf(casenames, 64, "casename%d", i);
		test_case(connfd, msg, "SUCCESS\r\n", casenames);
	}
}


//------------------------------------- rbtree 测试 ---------------------------------
void rbtree_testcase_10w (int connfd) {
	int i = 0;
	for (i = 0; i < TEST_COUNT; i++) {
		char msg[512] = {0};
		snprintf(msg, 512, "RSET key%d value%d", i ,i);

		char casenames[64] = {0};
		snprintf(casenames, 64, "casename%d", i);
		test_case(connfd, msg, "SUCCESS\r\n", casenames);
	}
}


//------------------------------------- hash 测试 ---------------------------------

void hash_testcase_10w (int connfd) {
	int i = 0;
	for (i = 0; i < TEST_COUNT; i++) {
		char msg[512] = {0};
		snprintf(msg, 512, "HSET key%d value%d", i ,i);

		char casenames[64] = {0};
		snprintf(casenames, 64, "casename%d", i);
		test_case(connfd, msg, "SUCCESS\r\n", casenames);
	}
}

//------------------------------------- skiptable 测试 ---------------------------------

void skiptable_testcase_10w (int connfd) {
	int i = 0;
	for (i = 0; i < TEST_COUNT; i++) {
		char msg[512] = {0};
		snprintf(msg, 512, "ZSET key%d value%d", i ,i);

		char casenames[64] = {0};
		snprintf(casenames, 64, "casename%d", i);
		test_case(connfd, msg, "SUCCESS\r\n", casenames);
	}
}

#else

//------------------------------------- array 测试 ---------------------------------

void array_testcase(int connfd ){

	test_case(connfd, "SET Name zxm", "SUCCESS\r\n", "SetNameCase");
	test_case(connfd, "COUNT", "1\r\n", "COUNTCase");

	test_case(connfd, "SET Sex man", "SUCCESS\r\n", "SetNameCase");
	test_case(connfd, "COUNT", "2\r\n", "COUNT");

	test_case(connfd, "SET Score 100", "SUCCESS\r\n", "SetNameCase");
	test_case(connfd, "COUNT", "3\r\n", "COUNT");

	test_case(connfd, "SET Nationality China", "SUCCESS\r\n", "SetNameCase");
	test_case(connfd, "COUNT", "4\r\n", "COUNT");
	
	test_case(connfd, "EXIST Name", "1\r\n", "EXISTCase");
	test_case(connfd, "GET Name", "zxm\r\n", "GetNameCase");
	test_case(connfd, "DELETE Name", "SUCCESS\r\n", "DELETECase");
	test_case(connfd, "COUNT", "3\r\n", "COUNT");
	test_case(connfd, "EXIST Name", "0\r\n", "EXISTCase");

	test_case(connfd, "EXIST Sex", "1\r\n", "EXISTCase");
	test_case(connfd, "GET Sex", "man\r\n", "GetNameCase");
	test_case(connfd, "DELETE Sex", "SUCCESS\r\n", "DELETECase");
	test_case(connfd, "COUNT", "2\r\n", "COUNT");

	test_case(connfd, "EXIST Score", "1\r\n", "EXISTCase");
	test_case(connfd, "GET Score", "100\r\n", "GetNameCase");
	test_case(connfd, "DELETE Score", "SUCCESS\r\n", "DELETECase");
	test_case(connfd, "COUNT", "1\r\n", "COUNT");

	test_case(connfd, "EXIST Nationality", "1\r\n", "EXISTCase");
	test_case(connfd, "GET Nationality", "China\r\n", "GetNameCase");
	test_case(connfd, "DELETE Nationality", "SUCCESS\r\n", "DELETECase");
	test_case(connfd, "COUNT", "0\r\n", "COUNT");

}


//------------------------------------- rbtree 测试 ---------------------------------

void rbtree_testcase(int connfd ){

	test_case(connfd, "RSET Name zxm", "SUCCESS\r\n", "RSetNameCase");
	test_case(connfd, "RCOUNT", "1\r\n", "COUNTCase");

	test_case(connfd, "RSET Sex man", "SUCCESS\r\n", "RSetNameCase");
	test_case(connfd, "RCOUNT", "2\r\n", "COUNT");

	test_case(connfd, "RSET Score 100", "SUCCESS\r\n", "RSetNameCase");
	test_case(connfd, "RCOUNT", "3\r\n", "COUNT");

	test_case(connfd, "RSET Nationality China", "SUCCESS\r\n", "RSetNameCase");
	test_case(connfd, "RCOUNT", "4\r\n", "COUNT");
	
	test_case(connfd, "REXIST Name", "1\r\n", "REXISTCase");
	test_case(connfd, "RGET Name", "zxm\r\n", "RGetNameCase");
	test_case(connfd, "RDELETE Name", "SUCCESS\r\n", "RDELETECase");
	test_case(connfd, "RCOUNT", "3\r\n", "COUNT");
	test_case(connfd, "REXIST Name", "0\r\n", "REXISTCase");

	test_case(connfd, "REXIST Sex", "1\r\n", "REXISTCase");
	test_case(connfd, "RGET Sex", "man\r\n", "RGetNameCase");
	test_case(connfd, "RDELETE Sex", "SUCCESS\r\n", "RDELETECase");
	test_case(connfd, "RCOUNT", "2\r\n", "COUNT");

	test_case(connfd, "REXIST Score", "1\r\n", "REXISTCase");
	test_case(connfd, "RGET Score", "100\r\n", "RGetNameCase");
	test_case(connfd, "RDELETE Score", "SUCCESS\r\n", "RDELETECase");
	test_case(connfd, "RCOUNT", "1\r\n", "RCOUNT");

	test_case(connfd, "REXIST Nationality", "1\r\n", "REXISTCase");
	test_case(connfd, "RGET Nationality", "China\r\n", "RGetNameCase");
	test_case(connfd, "RDELETE Nationality", "SUCCESS\r\n", "RDELETECase");
	test_case(connfd, "RCOUNT", "0\r\n", "RCOUNT");

}

//------------------------------------- hash 测试 ---------------------------------

void hash_testcase(int connfd ){

	test_case(connfd, "HSET Name zxm", "SUCCESS\r\n", "HSetNameCase");
	test_case(connfd, "HCOUNT", "1\r\n", "HCOUNTCase");

	test_case(connfd, "HSET Sex man", "SUCCESS\r\n", "HSetNameCase");
	test_case(connfd, "HCOUNT", "2\r\n", "HCOUNT");

	test_case(connfd, "HSET Score 100", "SUCCESS\r\n", "HSetNameCase");
	test_case(connfd, "HCOUNT", "3\r\n", "HCOUNT");

	test_case(connfd, "HSET Nationality China", "SUCCESS\r\n", "HSetNameCase");
	test_case(connfd, "HCOUNT", "4\r\n", "HCOUNT");
	
	test_case(connfd, "HEXIST Name", "1\r\n", "HEXISTCase");
	test_case(connfd, "HGET Name", "zxm\r\n", "HGetNameCase");
	test_case(connfd, "HDELETE Name", "SUCCESS\r\n", "HDELETECase");
	test_case(connfd, "HCOUNT", "3\r\n", "HCOUNT");
	test_case(connfd, "HEXIST Name", "0\r\n", "HEXISTCase");

	test_case(connfd, "HEXIST Sex", "1\r\n", "HEXISTCase");
	test_case(connfd, "HGET Sex", "man\r\n", "HGetNameCase");
	test_case(connfd, "HDELETE Sex", "SUCCESS\r\n", "HDELETECase");
	test_case(connfd, "HCOUNT", "2\r\n", "HCOUNT");

	test_case(connfd, "HEXIST Score", "1\r\n", "HEXISTCase");
	test_case(connfd, "HGET Score", "100\r\n", "HGetNameCase");
	test_case(connfd, "HDELETE Score", "SUCCESS\r\n", "HDELETECase");
	test_case(connfd, "HCOUNT", "1\r\n", "HCOUNT");

	test_case(connfd, "HEXIST Nationality", "1\r\n", "HEXISTCase");
	test_case(connfd, "HGET Nationality", "China\r\n", "HGetNameCase");
	test_case(connfd, "HDELETE Nationality", "SUCCESS\r\n", "HDELETECase");
	test_case(connfd, "HCOUNT", "0\r\n", "HCOUNT");

}

//------------------------------------- skiptable 测试 ---------------------------------

void skiptable_testcase(int connfd ){

	test_case(connfd, "ZSET Name zxm", "SUCCESS\r\n", "ZSetNameCase");
	test_case(connfd, "ZCOUNT", "1\r\n", "ZCOUNTCase");

	test_case(connfd, "ZSET Sex man", "SUCCESS\r\n", "ZSetNameCase");
	test_case(connfd, "ZCOUNT", "2\r\n", "ZCOUNT");

	test_case(connfd, "ZSET Score 100", "SUCCESS\r\n", "ZSetNameCase");
	test_case(connfd, "ZCOUNT", "3\r\n", "ZCOUNT");

	test_case(connfd, "ZSET Nationality China", "SUCCESS\r\n", "ZSetNameCase");
	test_case(connfd, "ZCOUNT", "4\r\n", "ZCOUNT");
	
	test_case(connfd, "ZEXIST Name", "1\r\n", "ZEXISTCase");
	test_case(connfd, "ZGET Name", "zxm\r\n", "ZGetNameCase");
	test_case(connfd, "ZDELETE Name", "SUCCESS\r\n", "ZDELETECase");
	test_case(connfd, "ZCOUNT", "3\r\n", "ZCOUNT");
	test_case(connfd, "ZEXIST Name", "0\r\n", "ZEXISTCase");

	test_case(connfd, "ZEXIST Sex", "1\r\n", "ZEXISTCase");
	test_case(connfd, "ZGET Sex", "man\r\n", "ZGetNameCase");
	test_case(connfd, "ZDELETE Sex", "SUCCESS\r\n", "ZDELETECase");
	test_case(connfd, "ZCOUNT", "2\r\n", "ZCOUNT");

	test_case(connfd, "ZEXIST Score", "1\r\n", "ZEXISTCase");
	test_case(connfd, "ZGET Score", "100\r\n", "ZGetNameCase");
	test_case(connfd, "ZDELETE Score", "SUCCESS\r\n", "ZDELETECase");
	test_case(connfd, "ZCOUNT", "1\r\n", "ZCOUNT");

	test_case(connfd, "ZEXIST Nationality", "1\r\n", "ZEXISTCase");
	test_case(connfd, "ZGET Nationality", "China\r\n", "ZGetNameCase");
	test_case(connfd, "ZDELETE Nationality", "SUCCESS\r\n", "ZDELETECase");
	test_case(connfd, "ZCOUNT", "0\r\n", "ZCOUNT");

}

#endif

int main(int argc, char *argv[]) {
	if (argc < 3){
		printf("argc < 3\n");
		return -1;
	}

	const char *ip = argv[1];
	int port = atoi(argv[2]);

	int connfd = connect_kvstore(ip, port);

	#if ENABLE_TEXT_QPS

	// array
	printf(" ---------> array testcase 10w <-----------\n");
	struct timeval array_begin;
	gettimeofday(&array_begin, NULL);

	array_testcase_10w(connfd);

	struct timeval array_end;
	gettimeofday(&array_end, NULL);

	int array_time_used = TIME_SUB_MS(array_end, array_begin);

	printf("time_used: %d, qps: %d (request/second)\n", array_time_used, (TEST_COUNT / array_time_used) * 1000);

	// rbtree
	printf(" ---------> rbtree testcase 10w <-----------\n");
	struct timeval rbtree_begin;
	gettimeofday(&rbtree_begin, NULL);

	rbtree_testcase_10w(connfd);

	struct timeval rbtree_end;
	gettimeofday(&rbtree_end, NULL);

	int rbtree_time_used = TIME_SUB_MS(rbtree_end, rbtree_begin);

	printf("time_used: %d, qps: %d (request/second)\n", rbtree_time_used, (TEST_COUNT / rbtree_time_used) * 1000);

	// hash
	printf(" ---------> hash testcase 10w <-----------\n");
	struct timeval hash_begin;
	gettimeofday(&hash_begin, NULL);

	hash_testcase_10w(connfd);

	struct timeval hash_end;
	gettimeofday(&hash_end, NULL);

	int hash_time_used = TIME_SUB_MS(hash_end, hash_begin);

	printf("time_used: %d, qps: %d (request/second)\n", hash_time_used, (TEST_COUNT / hash_time_used) * 1000);

	// skiptable
	printf(" ---------> skiptable testcase 10w <-----------\n");
	struct timeval skiptable_begin;
	gettimeofday(&skiptable_begin, NULL);

	skiptable_testcase_10w(connfd);

	struct timeval skiptable_end;
	gettimeofday(&skiptable_end, NULL);

	int skiptable_time_used = TIME_SUB_MS(skiptable_end, skiptable_begin);

	printf("time_used: %d, qps: %d (request/second)\n", skiptable_time_used, (TEST_COUNT / skiptable_time_used) * 1000);

	#else
	// array
	printf(" ---------> array testcase <-----------\n");
	array_testcase(connfd);

	// rbtree
	printf(" ---------> rbtree testcase <-----------\n");
	rbtree_testcase(connfd);

	// hash
	printf(" ---------> hash testcase <-----------\n");
	hash_testcase(connfd);

	// skiptable
	printf(" ---------> skiptable testcase <-----------\n");
	skiptable_testcase(connfd);

	#endif

	close(connfd);
} 