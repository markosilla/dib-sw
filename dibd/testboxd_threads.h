
#ifndef TESTBOXD_THREADS_H_
#define TESTBOXD_THREADS_H_

#include "testboxd_utilities.h"
#include "libtestbox_exec.h"

typedef struct {
	int status;
} thread_return_t;

thread_args_t *creat_default_thread_args();
int exec_test_thread(int wait, void *(*testFunc) (void *), thread_args_t *thread_args, const char *name, char *resp);
int tb_threads_kill_thread(const int index);
void *tb_thread_handler_group(void *arg);
void *tb_thread_handler_suite(void *arg);
void *tb_thread_handler_case(void *arg);
void *tb_thread_handler_executable(void *arg);

#endif /* TESTBOXD_THREADS_H_ */
