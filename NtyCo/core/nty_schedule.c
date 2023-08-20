

#include "nty_coroutine.h"


// 比较协程 co1 的休眠时间是否大于协程 co2 的休眠时间
static inline int nty_coroutine_sleep_cmp(nty_coroutine *co1, nty_coroutine *co2) {
	if (co1->sleep_usecs < co2->sleep_usecs) {
		return -1;
	}
	if (co1->sleep_usecs == co2->sleep_usecs) {
		return 0;
	}
	return 1;
}

// 比较协程 co1 的fd是否大于协程 co2 的fd
// 如果 co1->fd > co2->fd，则 co2 对应的文件描述符比 co1 对应的文件描述符靠前。
static inline int nty_coroutine_wait_cmp(nty_coroutine *co1, nty_coroutine *co2) {

    if (co1->fd < co2->fd) {
        return -1;
    }
    if (co1->fd == co2->fd) {
        return 0;
    }
    return 1;

}

// 生成红黑树的一个节点
RB_GENERATE(_nty_coroutine_rbtree_sleep, _nty_coroutine, sleep_node, nty_coroutine_sleep_cmp);
RB_GENERATE(_nty_coroutine_rbtree_wait, _nty_coroutine, wait_node, nty_coroutine_wait_cmp);


// 将协程 co 加入到调度器的睡眠队列中，并设置睡眠的时间
void nty_schedule_sched_sleepdown(nty_coroutine *co, uint64_t msecs) {
	uint64_t usecs = msecs * 1000u;
	// 查找是否已经存在相同的协程 co，如果存在，则将其从睡眠队列中移除
	nty_coroutine *co_tmp = RB_FIND(_nty_coroutine_rbtree_sleep, &co->sched->sleeping, co);
	if (co_tmp != NULL) {
		RB_REMOVE(_nty_coroutine_rbtree_sleep, &co->sched->sleeping, co_tmp);
	}
	// 计算协程的睡眠时间
	co->sleep_usecs = nty_coroutine_diff_usecs(co->sched->birth, nty_coroutine_usec_now()) + usecs;

	// 每次循环中，函数尝试将协程插入到红黑树的睡眠队列中
	// 如果插入成功（即之前没有相同的协程），则打印出协程的睡眠时间 sleep_usecs，并将睡眠时间加一。continue
	// 如果插入失败（即已经存在相同的协程），则将协程的状态设置为睡眠状态，并通过位运算将 NTY_COROUTINE_STATUS_SLEEPING 位设置为 1。然后跳出循环。
	while (msecs) {
		co_tmp = RB_INSERT(_nty_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		if (co_tmp) {
			printf("1111 sleep_usecs %"PRIu64"\n", co->sleep_usecs);
			co->sleep_usecs ++;
			continue;
		}
		// 将状态置为休眠
		co->status |= BIT(NTY_COROUTINE_STATUS_SLEEPING);
		break;
	}

}

// 将处于睡眠状态的协程从调度器中移除
void nty_schedule_desched_sleepdown(nty_coroutine *co) {
	//检查传入的协程 co 的状态是否为睡眠状态
	if (co->status & BIT(NTY_COROUTINE_STATUS_SLEEPING)) {
		// 若处于睡眠状态
		// 1.将协程从调度器的睡眠队列中移除
		RB_REMOVE(_nty_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		// 2.更新协程为不再处于睡眠的状态
		co->status &= CLEARBIT(NTY_COROUTINE_STATUS_SLEEPING);
	}
}

//找一个wait状态的协程
nty_coroutine *nty_schedule_search_wait(int fd) {
	nty_coroutine find_it = {0};
	find_it.fd = fd;
	// 获取当前协程所在的调度器
	nty_schedule *sched = nty_coroutine_get_sched();
	// 在调度器的等待队列中查找具有指定文件描述符 fd 的协程
	nty_coroutine *co = RB_FIND(_nty_coroutine_rbtree_wait, &sched->waiting, &find_it);
	// 将找到的协程的状态设置为 0，表示未被阻塞
	co->status = 0;

	return co;
}

//移除wait集合，移除wait状态
nty_coroutine* nty_schedule_desched_wait(int fd) {
	
	nty_coroutine find_it = {0};
	find_it.fd = fd;
	// 获取当前协程所在的调度器
	nty_schedule *sched = nty_coroutine_get_sched();
	// 在调度器的等待队列中查找具有指定文件描述符 fd 的协程
	nty_coroutine *co = RB_FIND(_nty_coroutine_rbtree_wait, &sched->waiting, &find_it);
	// 找得到，从等待队列中移除
	if (co != NULL) {
		RB_REMOVE(_nty_coroutine_rbtree_wait, &co->sched->waiting, co);
	}
	// 不可读，不可写
    co->status &= CLEARBIT(NTY_COROUTINE_STATUS_WAIT_READ);
    co->status &= CLEARBIT(NTY_COROUTINE_STATUS_WAIT_WRITE);
	// 从调度器中解除睡眠
	nty_schedule_desched_sleepdown(co);
	
	return co;
}

// 将协程添加到等待队列中，并设置其状态和文件描述符
void nty_schedule_sched_wait(nty_coroutine *co, int fd, unsigned short events, uint64_t timeout) {
	// 检查协程的状态是否已经处于等待读取或等待写入状态。如果是，则输出错误信息并终止程序执行
	if (co->status & BIT(NTY_COROUTINE_STATUS_WAIT_READ) ||
		co->status & BIT(NTY_COROUTINE_STATUS_WAIT_WRITE)) {
		printf("Unexpected event. lt id %"PRIu64" fd %"PRId32" already in %"PRId32" state\n",
            co->id, co->fd, co->status);
		assert(0);
	}
	// 根据传入的事件类型 events，设置协程状态
	if (events & POLLIN) {
		co->status |= NTY_COROUTINE_STATUS_WAIT_READ;	// 等待读取
	} else if (events & POLLOUT) {
		co->status |= NTY_COROUTINE_STATUS_WAIT_WRITE;	// 等待写入
	} else {
		printf("events : %d\n", events);
		assert(0);
	}

	// 将协程插入到等待队列中的红黑树节点中
	co->fd = fd;
	co->events = events;
	nty_coroutine *co_tmp = RB_INSERT(_nty_coroutine_rbtree_wait, &co->sched->waiting, co);

	assert(co_tmp == NULL);

	//printf("timeout --> %"PRIu64"\n", timeout);
	if (timeout == 1) return ; //Error
	// 加入调度器的睡眠队列
	nty_schedule_sched_sleepdown(co, timeout);
	
}


// 释放调度器
// 释放 nty_schedule 结构体所占用的内存，并清除线程特定的数据。
void nty_schedule_free(nty_schedule *sched) {
	if (sched->epfd > 0) {
		close(sched->epfd);
	}
	if (sched->eventfd > 0) {
		close(sched->eventfd);
	}
	if (sched->stack != NULL) {
		free(sched->stack);
	}
	
	free(sched);

	assert(pthread_setspecific(global_sched_key, NULL) == 0);
}

// 创建调度器
int nty_schedule_create(int stack_size) {
	// 确定调度器栈的大小
	int sched_stack_size = stack_size ? stack_size : NTY_CO_MAX_STACKSIZE;

	nty_schedule *sched = (nty_schedule*)calloc(1, sizeof(nty_schedule));
	if (sched == NULL) {
		printf("Failed to initialize scheduler\n");
		return -1;
	}
	// 将线程特定的数据设置为 sched
	assert(pthread_setspecific(global_sched_key, sched) == 0);

	sched->epfd = nty_epoller_create();
	if (sched->epfd == -1) {
		printf("Failed to initialize epoller\n");
		nty_schedule_free(sched);
		return -2;
	}
	// 触发器（里面是 epoll_ctl ）
	nty_epoller_ev_register_trigger();

	sched->stack_size = sched_stack_size;	// 栈大小
	sched->page_size = getpagesize();		// 页大小

#ifdef _USE_UCONTEXT
    // 为调度器的堆栈分配内存，并进行内存对齐 
	int ret = posix_memalign(&sched->stack, sched->page_size, sched->stack_size);
	assert(ret == 0);
#else
	sched->stack = NULL;
	bzero(&sched->ctx, sizeof(nty_cpu_ctx));
#endif

	sched->spawned_coroutines = 0;		// 协程数量
	sched->default_timeout = 3000000u;	// 默认超时时间

	RB_INIT(&sched->sleeping);
	RB_INIT(&sched->waiting);

	sched->birth = nty_coroutine_usec_now();	// 协程创建时间

	TAILQ_INIT(&sched->ready);

}

// 取出睡眠队列中的超时协程
static nty_coroutine *nty_schedule_expired(nty_schedule *sched) {
	
	uint64_t t_diff_usecs = nty_coroutine_diff_usecs(sched->birth, nty_coroutine_usec_now());
	nty_coroutine *co = RB_MIN(_nty_coroutine_rbtree_sleep, &sched->sleeping);
	if (co == NULL) return NULL;
	
	// 若超时，移除，返回
	if (co->sleep_usecs <= t_diff_usecs) {
		RB_REMOVE(_nty_coroutine_rbtree_sleep, &co->sched->sleeping, co);
		return co;
	}
	return NULL;
}

// 判断当前调度器下是否还有协程
static inline int nty_schedule_isdone(nty_schedule *sched) {
	return (RB_EMPTY(&sched->waiting) && 
		RB_EMPTY(&sched->sleeping) &&
		TAILQ_EMPTY(&sched->ready));
}

// 超时协程中，最小超时了多久
static uint64_t nty_schedule_min_timeout(nty_schedule *sched) {
	uint64_t t_diff_usecs = nty_coroutine_diff_usecs(sched->birth, nty_coroutine_usec_now());
	uint64_t min = sched->default_timeout;

	nty_coroutine *co = RB_MIN(_nty_coroutine_rbtree_sleep, &sched->sleeping);
	if (!co) return min;	// 没有处于休眠状态的协程

	min = co->sleep_usecs;
	if (min > t_diff_usecs) {
		return min - t_diff_usecs;
	}

	return 0;
} 

// 使用 epoll 机制来监听调度器的就绪事件
static int nty_schedule_epoll(nty_schedule *sched) {

	sched->num_new_events = 0;

	struct timespec t = {0, 0};
	uint64_t usecs = nty_schedule_min_timeout(sched);
	// 若 usecs 不为0，就绪队列不为空，根据 usecs 计算出等待的时间，并更新 t 的值。
	if (usecs && TAILQ_EMPTY(&sched->ready)) {
		t.tv_sec = usecs / 1000000u;		// 得到秒
		if (t.tv_sec != 0) {
			t.tv_nsec = (usecs % 1000u) * 1000u;	// 得到纳秒
		} else {
			t.tv_nsec = usecs * 1000u;				// 得到纳秒
		}
	} else {
		return 0;
	}

	// 调用 nty_epoller_wait 函数等待事件发生
	int nready = 0;
	while (1) {
		nready = nty_epoller_wait(t);
		if (nready == -1) {
			if (errno == EINTR) continue;
			else assert(0);
		}
		break;
	}

	sched->num_new_events = nready;

	return 0;
}

/*
睡眠 sleep 是协程主动暂停执行并等待一段时间或条件满足
就绪 ready 是协程已经准备好执行但还没有得到执行的机会
等待 wait 是协程暂停执行并等待特定条件或事件发生
*/
// 运行调度器
void nty_schedule_run(void) {
	// 1. 获取调度器
	nty_schedule *sched = nty_coroutine_get_sched();
	if (sched == NULL) return ;

	// 直到调度器里没有协程
	while (!nty_schedule_isdone(sched)) {
		
		// 2.1 （sleep rbtree）遍历睡眠集合，使用 resume 恢复过期协程 expired 的协程运行权
		nty_coroutine *expired = NULL;
		while ((expired = nty_schedule_expired(sched)) != NULL) {
			nty_coroutine_resume(expired);
		}
		// 2.2 （ready queue）遍历就绪集合，使用 resume 恢复 ready 的协程运行权
		nty_coroutine *last_co_ready = TAILQ_LAST(&sched->ready, _nty_coroutine_queue);
		// 直到就绪队列为空
		while (!TAILQ_EMPTY(&sched->ready)) {
			// 获取就绪队列中的第一个协程对象 co
			nty_coroutine *co = TAILQ_FIRST(&sched->ready);
			// 将该协程对象 co 从就绪队列中移除
			TAILQ_REMOVE(&co->sched->ready, co, ready_next);
			// 检查协程对象 co 的状态是否表示已断开连接
			if (co->status & BIT(NTY_COROUTINE_STATUS_FDEOF)) {
				// 如果是，则释放该协程对象并跳出循环。
				nty_coroutine_free(co);
				break;
			}
			// 恢复协程对象 co 的执行
			nty_coroutine_resume(co);
			// 当前协程对象等于 last_co_ready，则说明已经处理完所有待处理的协程对象，跳出循环
			if (co == last_co_ready) break;
		}

		// 3. （wait rbtree）遍历等待集合，使用 resume 恢复 wait 的协程运行权
		// 在 epoll 事件表中注册 sched 的事件，以便在该事件触发时能够及时处理。
		nty_schedule_epoll(sched);
		while (sched->num_new_events) {
			int idx = --sched->num_new_events;
			struct epoll_event *ev = sched->eventlist+idx;
			
			int fd = ev->data.fd;
			// 检查事件类型是否表示连接关闭（is_eof 为真）
			int is_eof = ev->events & EPOLLHUP;
			// 如果是，则将错误号 errno 设置为 ECONNRESET。
			if (is_eof) errno = ECONNRESET;

			// 在wait队列中找到 fd 对应的协程，并恢复
			nty_coroutine *co = nty_schedule_search_wait(fd);
			if (co != NULL) {
				if (is_eof) {
					co->status |= BIT(NTY_COROUTINE_STATUS_FDEOF);
				}
				nty_coroutine_resume(co);
			}

			is_eof = 0;
		}
	}

	// 释放调度器对象
	nty_schedule_free(sched);
	
	return ;
}

