
先看一下前置知识[协程设计原理](https://blog.csdn.net/Ricardo2/article/details/131164949)
# 一、为什么需要协程？
讨论协程之前，我们需要先了解**同步和异步**。以epoll多路复用器为例子，其主循环框架如下：

```c
while (1){
    int nready = epoll_wait(epfd, events, EVENT_SIZE, -1);

    int i=0;
    for (i=0; i<nready; i++){

        int sockfd = events[i].data.fd;
        if (sockfd == listenfd){

            int connfd = accept(listenfd, addr, &addr_len);
            
            setnonblock(connfd); //置为非阻塞

            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = connfd;
            epoll_ctl(epfd, EPOLL_CTL_ADD,connfd,&ev);
        }else{
            handel(sockfd); //进行读写操作
        }
    }

}
```
在通过 accept 建立服务端与客户端的连接之后，需要行读写操作，也就是 handel 函数。根据同步和异步，有两种不同的处理方式。

同步的处理方式
![在这里插入图片描述](https://img-blog.csdnimg.cn/a716988be0714c3aa9059721806dc788.png)
异步的处理方式
![在这里插入图片描述](https://img-blog.csdnimg.cn/597f5c29c1be41a09b3b708204b3f0e8.png)
可见，同步和异步主要区别在于对于 handle 函数的处理。同步在需要等待 handle 函数处理完成，主循环才能继续执行，阻塞了 epoll_wait。而异步是单独为 handle 函数创建一个线程异步处理，主循环不需要等待 handle 函数。

但是问题在于线程的创建、销毁，十分消耗资源。面对来自客户端的数百万连接，每一条都创建线程，很容易把服务器干崩溃。

因此就有了协程，在一个线程里面创建多个协程，共享一个线程的资源，但又能异步（看起来）处理事务。
# 二、协程的实现原理
前面说到，协程能异步处理事务，这只是看起来而已。协程的异步处理在于对CPU的调度，即需要的时候切入获取CPU操作权，不需要的时候让出CPU操作权。
![在这里插入图片描述](https://img-blog.csdnimg.cn/177d4460f7c341f09f273f6e49871e13.png)
这边涉及到以下几个问题：  
1、切换的时候怎么做到跟切换前一致？  
2、有协程1、协程2、协程3，……，怎么决定由那个协程执行？  


首先第一个问题，就是协程切换前后需要进行上下文切换。有汇编、ucontext、longjmp / setjmp。当然，汇编效果最快。

其次第二个问题，协程是一种用户态的轻量级线程，协程的调度完全由用户控制。也就是说，由我们自定义的调度器管理。  
在讲调度规则之前，我们需要先了解一下协程创建后会有哪些状态：  
**1、新创建的协程，创建完成后，加入到就绪集合，等待调度器的调度；  
2、协程在运行完成后，进行 IO 操作，此时 IO 并未准备好，进入等待状态集合；  
3、IO 准备就绪，协程开始运行，后续进行 sleep 操作，此时进入到睡眠状态集合。**  
![在这里插入图片描述](https://img-blog.csdnimg.cn/16f6c348ca6442069950a8d667a81c19.png)
在协程的上下文 IO 异步操作（nty_recv，nty_send）函数，步骤如下：  
1）将 sockfd 添加到 epoll 管理中。  
2）进行上下文环境切换，由协程上下文 yield 到调度器的上下文。  
3）调度器获取下一个协程上下文。Resume 新的协程  

IO 异步操作的上下文切换的时序图如下：
![在这里插入图片描述](https://img-blog.csdnimg.cn/abd13685661448a992607ea621fb5422.png)

就绪：都准备好了，就等着执行。就绪(ready)集合并不没有设置优先级的选型，所有在协程优先级一致，所以可以使用队列来存储就绪的协程，简称为就绪队列

等待：没准备好，比如IO操作的recv，信息还没来，recv就还没准备好。等待(wait)集合，其功能是在等待 IO 准备就绪，等待 IO 也是有时长的，所以等待(wait)集合采用红黑树的来存储，简称等待树(wait_tree)

睡眠：指协程主动挂起，等待某个时间后再恢复执行。比如等待IO我们可以设置一个时间，时间内还是没触发，那就算过期超时了。睡眠(sleep)集合需要按照睡眠时长进行排序，采用红黑树来存储，简称睡眠树(sleep_tree)红黑树在工程实用为<key, value>, key 为睡眠时长，value 为对应的协程结点。


因此，基于以上，协程如何被调度？有两种
**1、 生产者消费者模式**
![在这里插入图片描述](https://img-blog.csdnimg.cn/15d689c0ea964d71973c2ffd6072ef1e.png)

```c
while (1) {
	//遍历睡眠集合，将满足条件的加入到 ready
	nty_coroutine *expired = NULL;
	while ((expired = sleep_tree_expired(sched)) != ) {
		TAILQ_ADD(&sched->ready, expired);
	}
	//遍历等待集合，将满足添加的加入到 ready
	nty_coroutine *wait = NULL;
	int nready = epoll_wait(sched->epfd, events, EVENT_MAX, 1);
	for (i = 0;i < nready;i ++) {
		wait = wait_tree_search(events[i].data.fd);
		TAILQ_ADD(&sched->ready, wait);
	}
	// 使用 resume 回复 ready 的协程运行权
	while (!TAILQ_EMPTY(&sched->ready)) {
		nty_coroutine *ready = TAILQ_POP(sched->ready);
		resume(ready);
	}
}
```


**2、多状态运行**
![在这里插入图片描述](https://img-blog.csdnimg.cn/6e74ccb390ef4da49f9538a1ff4e6268.png)

```c
while (1) {
	//遍历睡眠集合，使用 resume 恢复 expired 的协程运行权
	nty_coroutine *expired = NULL;
	while ((expired = sleep_tree_expired(sched)) != ) {
		resume(expired);
	}
	//遍历等待集合，使用 resume 恢复 wait 的协程运行权
	nty_coroutine *wait = NULL;
	int nready = epoll_wait(sched->epfd, events, EVENT_MAX, 1);
	for (i = 0;i < nready;i ++) {
		wait = wait_tree_search(events[i].data.fd);
		resume(wait);
	}
	// 使用 resume 恢复 ready 的协程运行权
	while (!TAILQ_EMPTY(sched->ready)) {
		nty_coroutine *ready = TAILQ_POP(sched->ready);
		resume(ready);
	}
}
```

# 三、NtyCo 的接口
![在这里插入图片描述](https://img-blog.csdnimg.cn/8e880f42f77640d1a27bd3537b27833e.png#pic_center)
大致介绍一下协程工作的流程：  
1、为accept事件创建一个协程co1，并注册监听事件到co1的epoll，加入等待队列，然后yield，让出CPU控制权  
2、为recv事件创建一个协程co2，并注册监听事件到co2的epoll，加入等待队列，然后yield，让出CPU控制权  
3、为send事件创建一个协程co3，并注册监听事件到co3的epoll，加入等待队列，然后yield，让出CPU控制权  
（以上设置默认睡眠时间，同步加入睡眠队列）  
（调度器接手）  
4、遍历睡眠集合，使用 resume 恢复过期协程 expired 的协程运行权  
5、遍历就绪集合，使用 resume 恢复 ready 的协程运行权  
6、遍历等待集合，使用 resume 恢复 wait 的协程运行权  
![在这里插入图片描述](https://img-blog.csdnimg.cn/5b6a47a0441449faa29b3154dadb1d53.png#pic_center)

# 四、测试结果
4台Ubuntu虚拟机，其中一台服务端4核12G，另外三台1核4G。测试并发连接。  
需要做一些配置[测试搭建百万并发项目](https://blog.csdn.net/Ricardo2/article/details/130899171)  
服务端
```c
make
./bin/nty_server
```

客户端
```c
gcc mul_port_client_epoll.c -o mul_port_client_epoll
./mul_port_client_epoll 192.168.3.128 8080
```
![在这里插入图片描述](https://img-blog.csdnimg.cn/9cdd01e89a1a435383a3f7e3963c6d3a.png)  


# 五、代码地址
[Github：NtyCo](https://github.com/M-Ricardo/kvStorage)


