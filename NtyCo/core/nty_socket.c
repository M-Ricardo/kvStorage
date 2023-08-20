

#include "nty_coroutine.h"

// 利用了协程调度器和 epoll 实例来实现非阻塞的异步轮询。通过添加、删除文件描述符，
// 并使用协程的等待和恢复执行机制，在指定的超时时间内等待事件发生，然后再次恢复执行。
// 若超时>=0，加入epoll中，然后yield挂起
static int nty_epoll_inner(struct epoll_event *ev, int ev_num, int timeout) {
	// timeout 小于 0，设置为 INT_MAX，表示无限等待
	if (timeout < 0)
	{
		timeout = INT_MAX;
	}
	// 获取当前线程的调度器对象sched
	nty_schedule *sched = nty_coroutine_get_sched();
	if (sched == NULL) {
		printf("scheduler not exit!\n");
		return -1;
	}
	// 获取当前协程 co，即正在运行的协程。
	nty_coroutine *co = sched->curr_thread;
	
	// 遍历传入的数组 fds，对每个文件描述符进行处理
	int i = 0;
	for (i = 0;i < ev_num; i ++) {
		// 将文件描述符添加到协程调度器的 epoll 实例中，监听指定的事件。
		epoll_ctl(sched->epfd, EPOLL_CTL_ADD, ev->data.fd, ev);
		co->events = ev->events;
		// 在协程调度器中等待事件发生或超时
		nty_schedule_sched_wait(co, ev->data.fd, ev->events, timeout);
	}
	// yield， 暂停当前协程执行，将控制权交给调度器
	nty_coroutine_yield(co); 

	// 当恢复执行时，遍历传入的数组 fds，对每个文件描述符进行处理：
	for (i = 0;i < ev_num;i ++) {
		// 将文件描述符从协程调度器的 epoll 实例中删除，停止监听事件。
		epoll_ctl(sched->epfd, EPOLL_CTL_DEL, ev->data.fd, ev);
		// 将指定的文件描述符从协程调度器的等待队列中移除。
		nty_schedule_desched_wait(ev->data.fd);
	}
	// 返回传入的文件描述符数量 ev_num，表示成功进行了轮询操作。
	return ev_num;
}

// 创建一个新的套接字（socket）
int nty_socket(int domain, int type, int protocol) {
	// 创建新的 socket套接字，返回的文件描述符 fd
	int fd = socket(domain, type, protocol);
	if (fd == -1) {
		printf("Failed to create a new socket\n");
		return -1;
	}
	// 将套接字文件描述符 fd 设置为非阻塞模式
	int ret = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (ret == -1) {
		close(ret);
		return -1;
	}
	// 设置套接字地址重用选项，允许在同一地址上重新使用套接字。
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	
	return fd;
}

// accept接口，failed == -1, success > 0
int nty_accept(int fd, struct sockaddr *addr, socklen_t *len) {
	int sockfd = -1;

	while (1) {
		struct epoll_event ev;
        ev.events = POLLIN | POLLERR | POLLHUP;
        ev.data.fd = fd;
		nty_epoll_inner(&ev, 1, 1);
		sockfd = accept(fd, addr, len);
		// 若连接失败
		if (sockfd < 0) {
			// 暂时没有可用的连接，继续下一次循环等待连接
			if (errno == EAGAIN) {
				continue;
			} 
			// 连接被远程主机中断，输出错误信息并跳出循环
			else if (errno == ECONNABORTED) {
				printf("accept : ECONNABORTED\n");
				
			} 
			// 文件描述符数量已达到上限，输出错误信息并跳出循环
			else if (errno == EMFILE || errno == ENFILE) {
				printf("accept : EMFILE || ENFILE\n");
			}
			return -1;
		} 
		// 连接成功，跳出
		else {
			break;
		}
	}
	// 设置sockfd为非阻塞
	int ret = fcntl(sockfd, F_SETFL, O_NONBLOCK);
	if (ret == -1) {
		close(sockfd);
		return -1;
	}
	// 设置套接字地址重用选项，允许在同一地址上重新使用套接字。
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	
	return sockfd;
}


int nty_connect(int fd, struct sockaddr *name, socklen_t namelen) {

	int ret = 0;

	while (1) {

        struct epoll_event ev;
        ev.events = POLLOUT | POLLERR | POLLHUP;
        ev.data.fd = fd;
		nty_epoll_inner(&ev, 1, 1);

		ret = connect(fd, name, namelen);
		if (ret == 0) break;

		if (ret == -1 && (errno == EAGAIN ||
			errno == EWOULDBLOCK || 
			errno == EINPROGRESS)) {
			continue;
		} else {
			break;
		}
	}

	return ret;
}

//recv 
// add epoll first
ssize_t nty_recv(int fd, void *buf, size_t len, int flags) {
	
    struct epoll_event ev;
    ev.events = POLLIN | POLLERR | POLLHUP;
    ev.data.fd = fd;
    //加入epoll，然后yield
    nty_epoll_inner(&ev, 1, 1);
	// resume
	int ret = recv(fd, buf, len, flags);
	if (ret < 0) {
		if (errno == ECONNRESET) return -1;
	}
	return ret;
}


ssize_t nty_send(int fd, const void *buf, size_t len, int flags) {
	
	int sent = 0;	// sent 记录已发送的长度

	int ret = send(fd, ((char*)buf)+sent, len-sent, flags);
	if (ret == 0) return ret;
	if (ret > 0) sent += ret;

	while (sent < len) {
        struct epoll_event ev;
        ev.events = POLLOUT | POLLERR | POLLHUP;
        ev.data.fd = fd;
        //加入epoll，然后yield
        nty_epoll_inner(&ev, 1, 1);
		// 发送剩余的数据
		ret = send(fd, ((char*)buf)+sent, len-sent, flags);
		if (ret <= 0) {			
			break;
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0) return ret;
	
	return sent;
}


ssize_t nty_sendto(int fd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {


	int sent = 0;

	while (sent < len) {
        struct epoll_event ev;
        ev.events = POLLOUT | POLLERR | POLLHUP;
        ev.data.fd = fd;
        //加入epoll，然后yield
        nty_epoll_inner(&ev, 1, 1);
		int ret = sendto(fd, ((char*)buf)+sent, len-sent, flags, dest_addr, addrlen);
		if (ret <= 0) {
			if (errno == EAGAIN) continue;
			else if (errno == ECONNRESET) {
				return ret;
			}
			printf("send errno : %d, ret : %d\n", errno, ret);
			assert(0);
		}
		sent += ret;
	}
	return sent;
	
}

ssize_t nty_recvfrom(int fd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {

    struct epoll_event ev;
    ev.events = POLLIN | POLLERR | POLLHUP;
    ev.data.fd = fd;
    //加入epoll，然后yield
    nty_epoll_inner(&ev, 1, 1);

	int ret = recvfrom(fd, buf, len, flags, src_addr, addrlen);
	if (ret < 0) {
		if (errno == EAGAIN) return ret;
		if (errno == ECONNRESET) return 0;
		
		printf("recv error : %d, ret : %d\n", errno, ret);
		assert(0);
	}
	return ret;

}

int nty_close(int fd) {
	return close(fd);
}




// hook 开关
#ifdef  COROUTINE_HOOK

socket_t socket_f = NULL;

read_t read_f = NULL;
recv_t recv_f = NULL;
recvfrom_t recvfrom_f = NULL;

write_t write_f = NULL;
send_t send_f = NULL;
sendto_t sendto_f = NULL;

accept_t accept_f = NULL;
close_t close_f = NULL;
connect_t connect_f = NULL;


int init_hook(void) {

	socket_f = (socket_t)dlsym(RTLD_NEXT, "socket");
	
	read_f = (read_t)dlsym(RTLD_NEXT, "read");
	recv_f = (recv_t)dlsym(RTLD_NEXT, "recv");
	recvfrom_f = (recvfrom_t)dlsym(RTLD_NEXT, "recvfrom");

	write_f = (write_t)dlsym(RTLD_NEXT, "write");
	send_f = (send_t)dlsym(RTLD_NEXT, "send");
    sendto_f = (sendto_t)dlsym(RTLD_NEXT, "sendto");

	accept_f = (accept_t)dlsym(RTLD_NEXT, "accept");
	close_f = (close_t)dlsym(RTLD_NEXT, "close");
	connect_f = (connect_t)dlsym(RTLD_NEXT, "connect");

}



int socket(int domain, int type, int protocol) {

	if (!socket_f) init_hook();

	int fd = socket_f(domain, type, protocol);
	if (fd == -1) {
		printf("Failed to create a new socket\n");
		return -1;
	}
	int ret = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (ret == -1) {
		close(ret);
		return -1;
	}
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	
	return fd;
}

ssize_t read(int fd, void *buf, size_t count) {

	if (!read_f) init_hook();

	struct epoll_event ev;
	ev.events = POLLIN | POLLERR | POLLHUP;
	ev.data.fd = fd;
	//加入epoll，然后yield
	nty_epoll_inner(&ev, 1, 1);

	int ret = read_f(fd, buf, count);
	if (ret < 0) {
		//if (errno == EAGAIN) return ret;
		if (errno == ECONNRESET) return -1;
		//printf("recv error : %d, ret : %d\n", errno, ret);
		
	}
	return ret;
}

ssize_t recv(int fd, void *buf, size_t len, int flags) {

	if (!recv_f) init_hook();

	struct epoll_event ev;
	ev.events = POLLIN | POLLERR | POLLHUP;
	ev.data.fd = fd;
	//加入epoll，然后yield
	nty_epoll_inner(&ev, 1, 1);

	int ret = recv_f(fd, buf, len, flags);
	if (ret < 0) {
		//if (errno == EAGAIN) return ret;
		if (errno == ECONNRESET) return -1;
		//printf("recv error : %d, ret : %d\n", errno, ret);
		
	}
	return ret;
}


ssize_t recvfrom(int fd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {

	if (!recvfrom_f) init_hook();

	struct epoll_event ev;
	ev.events = POLLIN | POLLERR | POLLHUP;
	ev.data.fd = fd;
	//加入epoll，然后yield
	nty_epoll_inner(&ev, 1, 1);

	int ret = recvfrom_f(fd, buf, len, flags, src_addr, addrlen);
	if (ret < 0) {
		if (errno == EAGAIN) return ret;
		if (errno == ECONNRESET) return 0;
		
		printf("recv error : %d, ret : %d\n", errno, ret);
		assert(0);
	}
	return ret;

}


ssize_t write(int fd, const void *buf, size_t count) {

	if (!write_f) init_hook();

	int sent = 0;

	int ret = write_f(fd, ((char*)buf)+sent, count-sent);
	if (ret == 0) return ret;
	if (ret > 0) sent += ret;

	while (sent < count) {
		struct epoll_event ev;
		ev.events = POLLOUT | POLLERR | POLLHUP;
		ev.data.fd = fd;
		//加入epoll，然后yield
		nty_epoll_inner(&ev, 1, 1);
		ret = write_f(fd, ((char*)buf)+sent, count-sent);
		//printf("send --> len : %d\n", ret);
		if (ret <= 0) {			
			break;
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0) return ret;
	
	return sent;
}


ssize_t send(int fd, const void *buf, size_t len, int flags) {

	if (!send_f) init_hook();

	int sent = 0;

	int ret = send_f(fd, ((char*)buf)+sent, len-sent, flags);
	if (ret == 0) return ret;
	if (ret > 0) sent += ret;

	while (sent < len) {
		struct epoll_event ev;
		ev.events = POLLOUT | POLLERR | POLLHUP;
		ev.data.fd = fd;
		//加入epoll，然后yield
		nty_epoll_inner(&ev, 1, 1);
		ret = send_f(fd, ((char*)buf)+sent, len-sent, flags);
		//printf("send --> len : %d\n", ret);
		if (ret <= 0) {			
			break;
		}
		sent += ret;
	}

	if (ret <= 0 && sent == 0) return ret;
	
	return sent;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen) {

	if (!sendto_f) init_hook();

	struct epoll_event ev;
	ev.events = POLLOUT | POLLERR | POLLHUP;
	ev.data.fd = sockfd;
	//加入epoll，然后yield
	nty_epoll_inner(&ev, 1, 1);

	int ret = sendto_f(sockfd, buf, len, flags, dest_addr, addrlen);
	if (ret < 0) {
		if (errno == EAGAIN) return ret;
		if (errno == ECONNRESET) return 0;
		
		printf("recv error : %d, ret : %d\n", errno, ret);
		assert(0);
	}
	return ret;

}



int accept(int fd, struct sockaddr *addr, socklen_t *len) {
	if (!accept_f) init_hook();

	int sockfd = -1;
	int timeout = 1;
	while (1) {
		struct epoll_event ev;
		ev.events = POLLIN | POLLERR | POLLHUP;
		ev.data.fd = fd;
		//加入epoll，然后yield
		nty_epoll_inner(&ev, 1, 1);

		sockfd = accept_f(fd, addr, len);
		if (sockfd < 0) {
			if (errno == EAGAIN) {
				continue;
			} else if (errno == ECONNABORTED) {
				printf("accept : ECONNABORTED\n");
				
			} else if (errno == EMFILE || errno == ENFILE) {
				printf("accept : EMFILE || ENFILE\n");
			}
			return -1;
		} else {
			break;
		}
	}

	int ret = fcntl(sockfd, F_SETFL, O_NONBLOCK);
	if (ret == -1) {
		close(sockfd);
		return -1;
	}
	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
	
	return sockfd;
}

int close(int fd) {

	if (!close_f) init_hook();

	return close_f(fd);
}



int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {

	if (!connect_f) init_hook();

	int ret = 0;

	while (1) {

		struct epoll_event ev;
		ev.events = POLLOUT | POLLERR | POLLHUP;
		ev.data.fd = fd;
		//加入epoll，然后yield
		nty_epoll_inner(&ev, 1, 1);

		ret = connect_f(fd, addr, addrlen);
		if (ret == 0) break;

		if (ret == -1 && (errno == EAGAIN ||
			errno == EWOULDBLOCK || 
			errno == EINPROGRESS)) {
			continue;
		} else {
			break;
		}
	}

	return ret;
}



#endif











