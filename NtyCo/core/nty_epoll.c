

#include <sys/eventfd.h>

#include "nty_coroutine.h"


int nty_epoller_create(void) {
	return epoll_create(1024);
} 

int nty_epoller_wait(struct timespec t) {
  // 获取当前线程的调度器对象sched
	nty_schedule *sched = nty_coroutine_get_sched();  
	return epoll_wait(sched->epfd, sched->eventlist, NTY_CO_MAX_EVENTS, t.tv_sec*1000.0 + t.tv_nsec/1000000.0);
}

int nty_epoller_ev_register_trigger(void) {
  // 获取当前线程的调度器对象 sched
	nty_schedule *sched = nty_coroutine_get_sched();

  // 检查 sched 是否已经有一个关联的文件描述符 eventfd
	if (!sched->eventfd) {
    // 若无，使用 eventfd() 函数创建一个非阻塞的文件描述符，并将其与 sched 关联。
		sched->eventfd = eventfd(0, EFD_NONBLOCK);
		assert(sched->eventfd != -1);
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sched->eventfd;
	int ret = epoll_ctl(sched->epfd, EPOLL_CTL_ADD, sched->eventfd, &ev);

	assert(ret != -1);
}


