#include <stdio.h>
#include <stdlib.h>		/* exit, EXIT_FAILURE etc */
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include "testboxd_libs.h"
#include "testboxd_threads.h"
#include "testboxd_utilities.h"
#include "testboxd_sockets.h"
#include "testboxd_args.h"
#include "libtestbox_log.h"
#include "libtestbox_verb.h"
#include "libtestbox_casedb.h"
#include "libtestbox_suitedb.h"
#include "libtestbox_statusdb.h"


static void __sig_handler(int sig);
static void __init_sig_handler(void);
static void __daemonize(char *argv[]);
static void __daemon_shutdown(void);
static void __null_fds(void);

static void __null_fds(void) {
	struct fp {
		FILE *file;
		const char *mode;
	} files[] = {
			{ .file = stdin, .mode = "r" },
			{ .file = stdout, .mode = "w" },
			{ .file = stderr, .mode = "w" }
	};
	int i;

	for (i = 0; i < 3; i++) {
		FILE *f = freopen("/dev/null", files[i].mode, files[i].file);
		if (f == NULL ) {
			syslog(LOG_ERR, "(%s) FATAL: freopen: %d\n", __func__, i);
		}
	}
}

static void __sig_handler(int sig)
{
	switch(sig) {
	case SIGINT:
	case SIGTERM:
		__daemon_shutdown();
		exit(EXIT_SUCCESS);
		break;
	case SIGALRM:
		break;
	case SIGUSR1:
	case SIGKILL:
		__daemon_shutdown();
		exit(EXIT_SUCCESS);
		break;
	case SIGCHLD:
		break;
	}
}

static void __init_sig_handler(void)
{
  	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, __sig_handler);
	signal(SIGALRM, __sig_handler);
	signal(SIGCHLD, __sig_handler);
	signal(SIGTERM, __sig_handler);
	signal(SIGINT, SIG_IGN);
	signal(SIGKILL, __sig_handler);
	signal(SIGQUIT, SIG_IGN);
}

static void __daemonize(char *argv[]) {
	pid_t pid, sid;

	/* fork off the parent process */
	if (1 == getppid()) {
		/* already a daemon */
		exit(EXIT_SUCCESS);
	}

	__init_sig_handler();

	pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "(%s) FATAL: fork failed!\n", __func__);
		exit(EXIT_FAILURE);
	}
	/* we need to exit the parent process */
	if (pid > 0) {
		syslog(LOG_INFO, "(%s) %s running as daemon!\n", __func__, argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* Change the file mode mask */
	umask(0022);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		syslog(LOG_ERR, "(%s) FATAL: setsid failed!\n", __func__);
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory */
	if ((chdir("/")) < 0) {
		syslog(LOG_ERR, "(%s) FATAL: chdir failed!\n", __func__);
		exit(EXIT_FAILURE);
	}

	__null_fds();
}

static void __daemon_shutdown(void) {
	/* do clean up */
}

/**
 * TMP thing - to find out and log if we have more
 * cases of unexpected signals received
 * racy - but should be enough to find out if it still happens.
 */
pid_t sigusr_pid = 0;
int   sigusr_sig = 0;
pid_t sigpipe_sig = 0;
int   sigpipe_pid = 0;

static void sigusr_handler(int s, siginfo_t *info, void *ucontext)
{
        (void)ucontext;

	sigusr_sig++;
	sigusr_pid = info->si_pid;
}

static void sigpipe_handler(int s, siginfo_t *info, void *ucontext)
{
        (void)ucontext;

	sigpipe_sig++;
	sigpipe_pid = info->si_pid;
}

static int no_daemon_signal_setup(void)
{
	struct sigaction sausr1;
	struct sigaction sausr2;
	struct sigaction sigpipe;
	int ret = 0;

	sausr1.sa_sigaction = sigusr_handler;
	sigfillset(&sausr1.sa_mask);
	sausr1.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &sausr1, NULL) == -1) {
		ret = errno;
		TB_LOG(TB_LOG_ERR, "%s : SIGUSR1 failed with %d", __func__, errno);
		return -ret;
	}
	sausr2.sa_sigaction = sigusr_handler;
	sigfillset(&sausr2.sa_mask);
	sausr2.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR2, &sausr2, NULL) == -1) {
		ret = errno;
		TB_LOG(TB_LOG_ERR, "%s : SIGUSR2 failed with %d", __func__, errno);
		return -ret;
	}
	sigpipe.sa_sigaction = sigpipe_handler;
	sigfillset(&sigpipe.sa_mask);
	sigpipe.sa_flags = SA_SIGINFO;
	if (sigaction(SIGPIPE, &sigpipe, NULL) == -1) {
		ret = errno;
		TB_LOG(TB_LOG_ERR, "%s : SIGPIPE failed with %d", __func__, errno);
		return -ret;
	}
	return ret;
}

int main(int argc, char *argv[]) {
	openlog("TB:", LOG_PID, LOG_DAEMON);
	logLevel(TB_LOG_INFO);

	startup_args(argc, argv);

	if (run_as_daemon)
		__daemonize(argv);
	else {
		int ret = no_daemon_signal_setup();
		if (ret)
			exit(ret);
	}

	load_libraries();

	/* load_predefined_suites(); */

	load_group_from_file();

	return tb_socket_handling();
}
