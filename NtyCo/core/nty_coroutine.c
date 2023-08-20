

#include "nty_coroutine.h"

pthread_key_t global_sched_key;
static pthread_once_t sched_key_once = PTHREAD_ONCE_INIT;

#ifdef _USE_UCONTEXT

// 保存协程的堆栈信息到一个缓冲区
static void _save_stack(nty_coroutine *co) {
	// top指向栈顶
	char* top = co->sched->stack + co->sched->stack_size;
	char dummy = 0;		//	占位符
	assert(top - &dummy <= NTY_CO_MAX_STACKSIZE);	// 栈从高地址向低地址。top代表了栈的顶部地址，即栈的最高地址。dummy后定义的，&dummy代表了栈的最低地址
	// 若当前协程的堆栈大小 co->stack_size 小于 top - &dummy，realloc 重新分配
	if (co->stack_size < top - &dummy) {
		co->stack = realloc(co->stack, top - &dummy);
		assert(co->stack != NULL);
	}
	// 更新当前协程的大小
	co->stack_size = top - &dummy;
	memcpy(co->stack, &dummy, co->stack_size);
}

// 将协程栈的数据拷贝到调度器栈中，实现了栈的加载
static void _load_stack(nty_coroutine *co) {
	memcpy(co->sched->stack + co->sched->stack_size - co->stack_size, co->stack, co->stack_size);
}
// 执行协程对象 co 的函数
static void _exec(void *lt) {
	// 类型转换
	nty_coroutine *co = (nty_coroutine*)lt;
	// 执行协程的函数体
	co->func(co->arg);
	// 设置协程的状态标志位
	co->status |= (BIT(NTY_COROUTINE_STATUS_EXITED) | BIT(NTY_COROUTINE_STATUS_FDEOF) | BIT(NTY_COROUTINE_STATUS_DETACH));
	// 将控制权交还给调度器
	nty_coroutine_yield(co);
}

#else

//new_ctx[%rdi]:即将运行协程的上下文寄存器列表; cur_ctx[%rsi]:正在运行协程的上下文寄存器列表
int _switch(nty_cpu_ctx *new_ctx, nty_cpu_ctx *cur_ctx);
//默认x86_64
__asm__ (
"    .text                                  \n"
"       .p2align 4,,15                                   \n"
".globl _switch                                          \n"
".globl __switch                                         \n"
"_switch:                                                \n"
"__switch:                                               \n"
"       movq %rsp, 0(%rsi)      # save stack_pointer     \n"
"       movq %rbp, 8(%rsi)      # save frame_pointer     \n"
"       movq (%rsp), %rax       # save insn_pointer      \n"
"       movq %rax, 16(%rsi)                              \n"
"       movq %rbx, 24(%rsi)     # save rbx,r12-r15       \n"
"       movq %r12, 32(%rsi)                              \n"
"       movq %r13, 40(%rsi)                              \n"
"       movq %r14, 48(%rsi)                              \n"
"       movq %r15, 56(%rsi)                              \n"
"       movq 56(%rdi), %r15                              \n"
"       movq 48(%rdi), %r14                              \n"
"       movq 40(%rdi), %r13     # restore rbx,r12-r15    \n"
"       movq 32(%rdi), %r12                              \n"
"       movq 24(%rdi), %rbx                              \n"
"       movq 8(%rdi), %rbp      # restore frame_pointer  \n"
"       movq 0(%rdi), %rsp      # restore stack_pointer  \n"
"       movq 16(%rdi), %rax     # restore insn_pointer   \n"
"       movq %rax, (%rsp)                                \n"
"       ret                                              \n"
);


static void _exec(void *lt) {
	nty_coroutine *co = (nty_coroutine*)lt;
	co->func(co->arg);
	co->status |= (BIT(NTY_COROUTINE_STATUS_EXITED) | BIT(NTY_COROUTINE_STATUS_FDEOF) | BIT(NTY_COROUTINE_STATUS_DETACH));
	nty_coroutine_yield(co);

}

static inline void nty_coroutine_madvise(nty_coroutine *co) {

	size_t current_stack = (co->stack + co->stack_size) - co->ctx.esp;
	assert(current_stack <= co->stack_size);

	if (current_stack < co->last_stack_size &&
		co->last_stack_size > co->sched->page_size) {
		size_t tmp = current_stack + (-current_stack & (co->sched->page_size - 1));
		assert(madvise(co->stack, co->stack_size-tmp, MADV_DONTNEED) == 0);
	}
	co->last_stack_size = current_stack;
}

#endif

// 创建调度器
extern int nty_schedule_create(int stack_size);


// 释放协程对象
void nty_coroutine_free(nty_coroutine *co) {
	// co 为空，则直接返回
	if (co == NULL) return ;
	// 已生成协程数减一，表示释放了一个协程对象
	co->sched->spawned_coroutines --;

	if (co->stack) {
		free(co->stack);
		co->stack = NULL;
	}

	if (co) {
		free(co);
	}

}
// 初始化协程对象
static void nty_coroutine_init(nty_coroutine *co) {

#ifdef _USE_UCONTEXT
	// 获取当前协程的上下文
	getcontext(&co->ctx);	
	// 设置协程的栈起始地址。将调度器的栈 co->sched->stack 的地址赋值给协程的上下文的 uc_stack.ss_sp 字段，表示协程将使用调度器的栈来执行。
	co->ctx.uc_stack.ss_sp = co->sched->stack;
	// 设置协程的栈大小
	co->ctx.uc_stack.ss_size = co->sched->stack_size;
	// 设置协程的返回点。当执行完当前上下文时，程序将会切换回调度器的上下文 co->sched->ctx。
	co->ctx.uc_link = &co->sched->ctx;
	// 创建协程的上下文。使用 makecontext 函数将协程的上下文 co->ctx 和执行入口函数 _exec 关联起来，并传递协程的指针 co 作为参数给 _exec 函数。
	makecontext(&co->ctx, (void (*)(void)) _exec, 1, (void*)co);
#else
	void **stack = (void **)(co->stack + co->stack_size);

	stack[-3] = NULL;
	stack[-2] = (void *)co;

	co->ctx.esp = (void*)stack - (4 * sizeof(void*));
	co->ctx.ebp = (void*)stack - (3 * sizeof(void*));
	co->ctx.eip = (void*)_exec;
#endif
	// // 将协程对象的状态设置为就绪
	co->status = BIT(NTY_COROUTINE_STATUS_READY);
	
}

// 切换到另一个协程对象执行
void nty_coroutine_yield(nty_coroutine *co) {
#ifdef _USE_UCONTEXT
	// 如果协程对象的状态不是已退出，则保存当前协程的栈状态
	if ((co->status & BIT(NTY_COROUTINE_STATUS_EXITED)) == 0) {
		_save_stack(co);
	}
	// 将当前协程的上下文切换到另一个协程对象的上下文，实现协程的切换
	swapcontext(&co->ctx, &co->sched->ctx);
#else
	_switch(&co->sched->ctx, &co->ctx);
#endif
}

// 恢复协程对象的执行
int nty_coroutine_resume(nty_coroutine *co) {
	// 如果协程对象的状态为新建状态，则进行初始化操作。否则，加载协程对象的栈
	if (co->status & BIT(NTY_COROUTINE_STATUS_NEW)) {
		nty_coroutine_init(co);
	} 
#ifdef _USE_UCONTEXT	
	else {
		_load_stack(co);
	}
#endif
	// 获取当前调度器对象
	nty_schedule *sched = nty_coroutine_get_sched();
	// 将当前协程对象 co 的指针赋值给调度器结构体中的 curr_thread 成员变量。这个成员变量用于保存当前协程所关联的协程对象。
	sched->curr_thread = co;
#ifdef _USE_UCONTEXT
	// 将当前调度器的上下文切换到协程对象的上下文，实现协程的恢复执行
	swapcontext(&sched->ctx, &co->ctx);
#else
	_switch(&co->ctx, &co->sched->ctx);
	nty_coroutine_madvise(co);
#endif
	// 将调度器的当前线程指针设置为 NULL，表示当前线程不再参与后续的操作	
	sched->curr_thread = NULL;

	if (co->status & BIT(NTY_COROUTINE_STATUS_EXITED)) {
		if (co->status & BIT(NTY_COROUTINE_STATUS_DETACH)) {
			nty_coroutine_free(co);
		}
		return -1;
	} 
	return 0;
}


// 将协程对象置入睡眠队列中，并让出执行权
void nty_coroutine_sleep(uint64_t msecs) {
	nty_coroutine *co = nty_coroutine_get_sched()->curr_thread;

	if (msecs == 0) {	// 若睡眠的时间 msecs = 0 ，将 co 尾插到就绪队列，让出执行权
		TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);
		nty_coroutine_yield(co);
	} else {	// msecs 不等于 0，表示需要延迟执行。加入到调度器的睡眠队列中，并设置睡眠的时间
		nty_schedule_sched_sleepdown(co, msecs);
	}
}


// 释放调度对象的key
static void nty_coroutine_sched_key_destructor(void *data) {
	free(data);
}

// 创建调度键
static void nty_coroutine_sched_key_creator(void) {
	assert(pthread_key_create(&global_sched_key, nty_coroutine_sched_key_destructor) == 0);
	assert(pthread_setspecific(global_sched_key, NULL) == 0);
}


// 创建协程
int nty_coroutine_create(nty_coroutine **new_co, proc_coroutine func, void *arg) {

	assert(pthread_once(&sched_key_once, nty_coroutine_sched_key_creator) == 0);

	// 获取调度器，若失败，创建调度器
	nty_schedule *sched = nty_coroutine_get_sched();
	if (sched == NULL) {
		nty_schedule_create(0);
		
		sched = nty_coroutine_get_sched();
		if (sched == NULL) {
			printf("Failed to create scheduler\n");
			return -1;
		}
	}
	// 给协程分配空间
	nty_coroutine *co = calloc(1, sizeof(nty_coroutine));
	if (co == NULL) {
		printf("Failed to allocate memory for new coroutine\n");
		return -2;
	}

#ifdef _USE_UCONTEXT
	// 初始化协程的栈
	co->stack = NULL;
	co->stack_size = 0;
#else
	int ret = posix_memalign(&co->stack, getpagesize(), sched->stack_size);
	if (ret) {
		printf("Failed to allocate stack for new coroutine\n");
		free(co);
		return -3;
	}
	co->stack_size = sched->stack_size;
#endif
	co->sched = sched;
	co->status = BIT(NTY_COROUTINE_STATUS_NEW); // 新建状态
	co->id = sched->spawned_coroutines ++;		// 协程数量 + 1
	co->func = func;
	
	co->fd = -1;
	co->events = 0;
	co->arg = arg;
	co->birth = nty_coroutine_usec_now();
	*new_co = co;
	// 新创建的协程尾插入调度器的就绪队列
	TAILQ_INSERT_TAIL(&co->sched->ready, co, ready_next);

	return 0;
}




