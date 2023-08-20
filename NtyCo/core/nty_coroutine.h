

#ifndef __NTY_COROUTINE_H__
#define __NTY_COROUTINE_H__


#define _GNU_SOURCE
#include <dlfcn.h>

// #define _USE_UCONTEXT


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <netinet/tcp.h>

#ifdef _USE_UCONTEXT
#include <ucontext.h>
#endif

#include <sys/epoll.h>
#include <sys/poll.h>

#include <errno.h>

#include "nty_queue.h"
#include "nty_tree.h"

#define NTY_CO_MAX_EVENTS		(1024*1024)
#define NTY_CO_MAX_STACKSIZE	(128*1024) // {http: 16*1024, tcp: 4*1024}

#define BIT(x)	 				(1 << (x))
#define CLEARBIT(x) 			~(1 << (x))


typedef void (*proc_coroutine)(void *);

// 表示协程的不同状态
typedef enum {
	NTY_COROUTINE_STATUS_WAIT_READ,		// 等待读取状态
	NTY_COROUTINE_STATUS_WAIT_WRITE,	// 等待写入状态

	NTY_COROUTINE_STATUS_NEW,			// 新建状态
	NTY_COROUTINE_STATUS_READY,			// 就绪状态

	NTY_COROUTINE_STATUS_EXITED,		// 已退出状态
	NTY_COROUTINE_STATUS_BUSY,			// 忙碌状态

	NTY_COROUTINE_STATUS_SLEEPING,		// 休眠状态
	NTY_COROUTINE_STATUS_EXPIRED,		// 已过期状态
	NTY_COROUTINE_STATUS_FDEOF,			// 文件结束状态
	NTY_COROUTINE_STATUS_DETACH	// 分离状态,将一个线程中的某个对象（例如协程对象）与该线程解除关联，使其不再受该线程的管理和控制。
} nty_coroutine_status;


LIST_HEAD(_nty_coroutine_link, _nty_coroutine);
TAILQ_HEAD(_nty_coroutine_queue, _nty_coroutine);

RB_HEAD(_nty_coroutine_rbtree_sleep, _nty_coroutine);
RB_HEAD(_nty_coroutine_rbtree_wait, _nty_coroutine);

// 协程链接结构体，用于实现协程之间的关联和切换
typedef struct _nty_coroutine_link nty_coroutine_link;
// 协程队列结构体，用于存储和管理待执行的协程，以支持协程的调度和执行。
typedef struct _nty_coroutine_queue nty_coroutine_queue;

// 协程红黑树休眠结构体，用于实现协程的休眠操作。
typedef struct _nty_coroutine_rbtree_sleep nty_coroutine_rbtree_sleep;
// 协程红黑树等待结构体，用于实现协程的等待操作。
typedef struct _nty_coroutine_rbtree_wait nty_coroutine_rbtree_wait;


#ifndef _USE_UCONTEXT
typedef struct _nty_cpu_ctx {
	void *esp; //
	void *ebp;
	void *eip;
	void *edi;
	void *esi;
	void *ebx;
	void *r1;
	void *r2;
	void *r3;
	void *r4;
	void *r5;
} nty_cpu_ctx;
#endif

// 表示协程调度器的相关信息和状态
typedef struct _nty_schedule {
	uint64_t birth;
#ifdef _USE_UCONTEXT
	ucontext_t ctx;
#else
	nty_cpu_ctx ctx;
#endif
	void *stack;
	size_t stack_size;
	int spawned_coroutines;								// 当前已经生成的协程数量
	uint64_t default_timeout;							// 默认的超时时间
	struct _nty_coroutine *curr_thread;					// 当前线程的协程对象指针
	int page_size;										// 页的大小

	int epfd;										// epoll 实例的文件描述符
	int eventfd;										//  eventfd 实例的文件描述符
	struct epoll_event eventlist[NTY_CO_MAX_EVENTS];	//  epoll 实例的事件列表

	int num_new_events;									// 新生成的事件数量

	nty_coroutine_queue ready;							// 就绪队列，用于存储已经准备好执行的协程对象
	nty_coroutine_rbtree_sleep sleeping;				// 正在休眠的协程，用于管理协程的睡眠状态
	nty_coroutine_rbtree_wait waiting;					// 正在等待的协程，用于管理协程的等待状态

} nty_schedule;

// 协程的各种属性和信息
typedef struct _nty_coroutine {
#ifdef _USE_UCONTEXT
	ucontext_t ctx;							// 保存协程的上下文信息，用于切换到协程时恢复协程的状态。
#else
	nty_cpu_ctx ctx;
#endif
	proc_coroutine func;					// 指向协程函数的指针
	void *arg;
	void *stack;							// 指向协程堆栈的指针
	size_t stack_size;						// 当前协程的堆栈大小
	size_t last_stack_size;					// 上一个协程的堆栈大小
	
	nty_coroutine_status status;			// 协程的运行状态。
	nty_schedule *sched;					// 协程调度器对象

	uint64_t birth;							// 协程的创建时间戳
	uint64_t id;							// 协程的唯一标识符
	int fd;									// 协程的文件描述符
	unsigned short events;  				// 事件标志，用于指示协程的事件类型

	uint64_t sleep_usecs;					// 协程的休眠时间，单位为微秒

	RB_ENTRY(_nty_coroutine) sleep_node;	// 指向用于睡眠等待的红黑树节点的指针
	RB_ENTRY(_nty_coroutine) wait_node;		// 指向用于等待条件的红黑树节点的指针
	TAILQ_ENTRY(_nty_coroutine) ready_next;	// 指向下一个准备好执行的协程的链表节点的指针

} nty_coroutine;



extern pthread_key_t global_sched_key;
//获得线程唯一的sched
static inline nty_schedule *nty_coroutine_get_sched(void) {
	return pthread_getspecific(global_sched_key);	// 获取与当前线程特定数据键关联的值
}

// 计算微秒差
static inline uint64_t nty_coroutine_diff_usecs(uint64_t t1, uint64_t t2) {
	return t2-t1;
}

//计算现在的微秒
static inline uint64_t nty_coroutine_usec_now(void) {
	struct timeval t1 = {0, 0};
	gettimeofday(&t1, NULL);

	return t1.tv_sec * 1000000 + t1.tv_usec;
}


// nty_epoller
int nty_epoller_create(void);
int nty_epoller_ev_register_trigger(void);
int nty_epoller_wait(struct timespec t);


// nty_schedule
int nty_schedule_create(int stack_size);
void nty_schedule_free(nty_schedule *sched);
void nty_schedule_desched_sleepdown(nty_coroutine *co);
void nty_schedule_sched_sleepdown(nty_coroutine *co, uint64_t msecs);
nty_coroutine* nty_schedule_desched_wait(int fd);
void nty_schedule_sched_wait(nty_coroutine *co, int fd, unsigned short events, uint64_t timeout);
void nty_schedule_run(void);
nty_coroutine *nty_schedule_search_wait(int fd);

// nty_coroutine
int nty_coroutine_resume(nty_coroutine *co);
void nty_coroutine_free(nty_coroutine *co);
int nty_coroutine_create(nty_coroutine **new_co, proc_coroutine func, void *arg);
void nty_coroutine_yield(nty_coroutine *co);
void nty_coroutine_sleep(uint64_t msecs);

// socket
int nty_socket(int domain, int type, int protocol);
int nty_accept(int fd, struct sockaddr *addr, socklen_t *len);
ssize_t nty_recv(int fd, void *buf, size_t len, int flags);
ssize_t nty_send(int fd, const void *buf, size_t len, int flags);
int nty_close(int fd);
int nty_connect(int fd, struct sockaddr *name, socklen_t namelen);
ssize_t nty_sendto(int fd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t nty_recvfrom(int fd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);






//-------------------  hook 开关

#define COROUTINE_HOOK 

#ifdef  COROUTINE_HOOK


typedef int (*socket_t)(int domain, int type, int protocol);
extern socket_t socket_f;

typedef int(*connect_t)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern connect_t connect_f;

typedef ssize_t(*read_t)(int sockfd, void *, size_t len);
extern read_t read_f;


typedef ssize_t(*recv_t)(int sockfd, void *buf, size_t len, int flags);
extern recv_t recv_f;

typedef ssize_t(*recvfrom_t)(int sockfd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen);
extern recvfrom_t recvfrom_f;

typedef ssize_t(*write_t)(int sockfd, const void *buf, size_t len);
extern write_t write_f;

typedef ssize_t(*send_t)(int sockfd, const void *buf, size_t len, int flags);
extern send_t send_f;

typedef ssize_t(*sendto_t)(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen);
extern sendto_t sendto_f;

typedef int(*accept_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern accept_t accept_f;

// new-syscall
typedef int(*close_t)(int sockfd);
extern close_t close_f;


int init_hook(void);


#endif


#endif



