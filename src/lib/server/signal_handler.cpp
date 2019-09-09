// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com


#include "signal_handler.h"
#include "helper/debug_helper.h"
#include "server/serverapp.h"

namespace KBEngine{
KBE_SINGLETON_INIT(SignalHandlers);

const int SIGMIN = 1;
const int SIGMAX = SIGSYS;

const char * SIGNAL_NAMES[] = 
{
	NULL,
	"SIGHUP",		//SIGHUP 信号在用户终端连接(正常或非正常)结束时发出, 通常是在终端的控制进程结束时, 通知同一session内的各个作业, 这时它们与控制终端不再关联. 系统对SIGHUP信号的默认处理是终止收到该信号的进程。所以若程序中没有捕捉该信号，当收到该信号时，进程就会退出.SIGHUP 1 终端挂起或控制进程终止。当用户退出Shell时，由该进程启动的所有进程都会收到这个信号，默认动作为终止进程
	"SIGINT",		//在键盘按下<Ctrl+C>组合键后产生，默认动作为终止进程.外部中断，通常为用户所发动,来自键盘的中断信号 ( ctrl + c ) .键盘中断。当用户按下组合键时，用户终端向正在运行中的由该终端启动的程序发出此信号。默认动作为终止进程
	"SIGQUIT",		//在键盘按下<Ctrl+\>组合键后产生，默认动作为终止进程.当用户按下或组合键时，用户终端向正在运行中的由该终端启动的程序发出此信号。默认动作为退出程序
	"SIGILL",		//非法程序映像，例如非法指令
	"SIGTRAP",		//在大多数系统，gdb对使用fork创建的进程没有进行特别的支持。当父进程使用fork创建子进程，gdb仍然只会调试父进程，而子进程没有得到控制和调试。这个时候，如果你在子进程执行到的代码中设置了断点，那么当子进程执行到这个断点的时候，会产生一个SIGTRAP的信号，如果没有对此信号进行捕捉处理，就会按默认的处理方式处理——终止进程
	"SIGABRT",		//异常终止条件，例如 abort() 所起始的
	"SIGBUS",		//意味着指针所相应的地址是有效地址，但总线不能正常使用该指针。一般是未对齐的数据訪问所致。 
	"SIGFPE",		//发生致命的运算错误时发出。不仅包括浮点运算错误，还包括溢出及除数为0等所有的算法错误。默认动作为终止进程并产生core文件
	"SIGKILL",		//无条件终止进程。本信号不能被忽略、处理和阻塞。默认动作为终止进程。它向系统管理员提供了一种可以杀死任何进程的方法；kill 命令默认也使用 SIGTERM 信号。进程接收到该信号会立即终止，不进行清理和暂存工作。
	"SIGUSR1",
	"SIGSEGV",		//非法内存访问（段错误）
	"SIGUSR2",
	"SIGPIPE",
	"SIGALRM",		//定时器超时，超时的时间由系统调用alarm设置。默认动作为终止进程
	"SIGTERM",		//程序结束信号，可以由 kill 命令产生。与SIGKILL不同的是，SIGTERM 信号可以被阻塞和终止，以便程序在退出前可以保存工作或清理临时文件等
	"SIGSTKFLT",
	"SIGCHLD",		//子进程结束时，父进程会收到这个信号。默认动作为忽略该信号；
	"SIGCONT",
	"SIGSTOP",		//来自键盘 ( ctrl + z ) 或调试程序的停止执行信号
	"SIGTSTP",
	"SIGTTIN",
	"SIGTTOU",
	"SIGURG",
	"SIGXCPU",
	"SIGXFSZ",
	"SIGVTALRM",
	"SIGPROF",
	"SIGWINCH",
	"SIGIO",
	"SIGPWR",
	"SIGSYS"
};

SignalHandlers g_signalHandlers;

std::string SIGNAL2NAMES(int signum)
{
	if (signum >= SIGMIN && signum <= SIGMAX)
	{
		return SIGNAL_NAMES[signum];
	}

	return fmt::format("unknown({})", signum);
}

void signalHandler(int signum)
{
	printf("SignalHandlers: receive sigNum %d.\n", signum);
	g_signalHandlers.onSignalled(signum);
};

//-------------------------------------------------------------------------------------
SignalHandlers::SignalHandlers():
singnalHandlerMap_(),
papp_(NULL),
rpos_(0),
wpos_(0)
{
}

//-------------------------------------------------------------------------------------
SignalHandlers::~SignalHandlers()
{
}

//-------------------------------------------------------------------------------------
void SignalHandlers::attachApp(ServerApp* app)
{ 
	papp_ = app; 
	app->dispatcher().addTask(this);
}

//-------------------------------------------------------------------------------------	
bool SignalHandlers::ignoreSignal(int sigNum)
{
#if KBE_PLATFORM != PLATFORM_WIN32
	if (signal(sigNum, SIG_IGN) == SIG_ERR)
		return false;
#endif

	return true;
}

//-------------------------------------------------------------------------------------	
SignalHandler* SignalHandlers::addSignal(int sigNum, 
	SignalHandler* pSignalHandler, int flags)
{
	// 允许被重置
	// SignalHandlerMap::iterator iter = singnalHandlerMap_.find(sigNum);
	// KBE_ASSERT(iter == singnalHandlerMap_.end());

	singnalHandlerMap_[sigNum] = pSignalHandler;

#if KBE_PLATFORM != PLATFORM_WIN32
	struct sigaction action;
	action.sa_handler = &signalHandler;
	sigfillset( &(action.sa_mask) );

	if (flags & SA_SIGINFO)
	{
		ERROR_MSG( "ServerApp::installSingnal: "
				"SA_SIGINFO is not supported, ignoring\n" );
		flags &= ~SA_SIGINFO;
	}

	action.sa_flags = flags;

	::sigaction( sigNum, &action, NULL );
#endif

	return pSignalHandler;
}
	
//-------------------------------------------------------------------------------------	
SignalHandler* SignalHandlers::delSignal(int sigNum)
{
	SignalHandlerMap::iterator iter = singnalHandlerMap_.find(sigNum);
	KBE_ASSERT(iter != singnalHandlerMap_.end());
	SignalHandler* pSignalHandler = iter->second;
	singnalHandlerMap_.erase(iter);
	return pSignalHandler;
}
	
//-------------------------------------------------------------------------------------	
void SignalHandlers::clear()
{
	singnalHandlerMap_.clear();
}

//-------------------------------------------------------------------------------------	
void SignalHandlers::onSignalled(int sigNum)
{
	// 不要分配内存
	KBE_ASSERT(wpos_ != 0XFF);
	signalledArray_[wpos_++] = sigNum;
}

//-------------------------------------------------------------------------------------	
bool SignalHandlers::process()
{
	if (wpos_ == 0)
		return true;

	DEBUG_MSG(fmt::format("SignalHandlers::process: rpos={}, wpos={}.\n", rpos_, wpos_));

#if KBE_PLATFORM != PLATFORM_WIN32
	/* 如果信号有瞬时超过255触发需求，可以打开注释，将会屏蔽所有信号等执行完毕之后再执行期间触发的信号，将signalledArray_改为信号集类型
	if (wpos_ == 1 && signalledArray_[0] == SIGALRM)
		return true;

	sigset_t mask, old_mask;
	sigemptyset(&mask);
	sigemptyset(&old_mask);

	sigfillset(&mask);

	// 屏蔽信号
	sigprocmask(SIG_BLOCK, &mask, &old_mask);
	*/
#endif

	while (rpos_ < wpos_)
	{
		int sigNum = signalledArray_[rpos_++];

#if KBE_PLATFORM != PLATFORM_WIN32
		//if (SIGALRM == sigNum)
		//	continue;
#endif

		SignalHandlerMap::iterator iter1 = singnalHandlerMap_.find(sigNum);
		if (iter1 == singnalHandlerMap_.end())
		{
			DEBUG_MSG(fmt::format("SignalHandlers::process: sigNum {} unhandled, singnalHandlerMap({}).\n",
				SIGNAL2NAMES(sigNum), singnalHandlerMap_.size()));

			continue;
		}

		DEBUG_MSG(fmt::format("SignalHandlers::process: sigNum {} handle. singnalHandlerMap({})\n", SIGNAL2NAMES(sigNum), singnalHandlerMap_.size()));
		iter1->second->onSignalled(sigNum);
	}

	rpos_ = 0;
	wpos_ = 0;

#if KBE_PLATFORM != PLATFORM_WIN32
	// 恢复屏蔽
	/*
	sigprocmask(SIG_SETMASK, &old_mask, NULL);

	addSignal(SIGALRM, NULL);

	// Get the current signal mask
	sigprocmask(0, NULL, &mask);

	// Unblock SIGALRM
	sigdelset(&mask, SIGALRM);

	// Wait with this mask
	ualarm(1, 0);

	// 让期间错过的信号重新触发
	sigsuspend(&mask);

	delSignal(SIGALRM);
	*/
#endif

	return true;
}

//-------------------------------------------------------------------------------------		
}
