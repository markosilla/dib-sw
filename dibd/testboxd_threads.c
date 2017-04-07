#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strerror... */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>

#include "testboxd_threads.h"
#include "testboxd_utilities.h"
#include "testboxd_types.h"
#include "testboxd_print.h"
#include "libtestbox_log.h"
#include "libtestbox_types.h"
#include "libtestbox_verb.h"
#include "libtestbox_xmlfilegen.h"
#include "libtestbox_time.h"
#include "libtestbox_socket.h"
#include "libtestbox_statusdb.h"

#define TESTBOX_FAIL BOLDRED "TESTBOX_FAIL" RESET_COLOR
#define TEST_THREAD_COUNT	100

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_wait = PTHREAD_COND_INITIALIZER;
static int test_thread_cnt;

/*---------------------------------------------------------------------------*/
thread_args_t *creat_default_thread_args(void) {
	thread_args_t *args = calloc(1, sizeof(thread_args_t));

	if (NULL == args) {
		TB_LOG(TB_LOG_ERR, "%s", strerror(errno));
	} else {
		args->verbose_level = get_verbose_level();
		if (args->verbose_level == TB_VERB_OFF ||
				args->verbose_level == TB_VERB_OFF_LOG){
			args->wait_for_response = 0;
		}
		else{
			args->wait_for_response = 1;
		}

		args->loops = 1;
		args->argc = 0;
		args->execution_time = 0;
		args->execution_dly_time = 0;
	}
	return args;
}

/*---------------------------------------------------------------------------*/
thread_return_t *create_default_thread_return(void) {
	thread_return_t *ret = calloc(1, sizeof(thread_return_t));

	if (NULL == ret) {
		TB_LOG(TB_LOG_ERR, "%s", strerror(errno));

		return NULL;
	}

	ret->status = TB_ERROR;

	return ret;
}

/*---------------------------------------------------------------------------*/
thread_return_t* finalize_thread(int result, thread_args_t *data,
		thread_return_t* thread_return)
{
	/* If calling thread expects response, send thread_return, else NULL*/
	if (data->wait_for_response)
	{
		thread_return = create_default_thread_return();

		thread_return->status = result;
		TB_LOG(TB_LOG_DBG, "%s %s\n", data->node_name,
				result != TB_OK ? "failed" : "passed");
		/* remove thread from client list */
		tb_socket_client_remove(pthread_self());
	}
	/* Free arg data */
	free(data);
	return thread_return;
}

/*---------------------------------------------------------------------------*/
int exec_test_thread(int wait, void *(*testFunc)(void *), thread_args_t *thread_args, const char *name, char *resp) {
	int status = TB_OK;
	pthread_t thread;
	thread_return_t *response;
	struct timespec ts;

		pthread_mutex_lock(&lock);

		while (test_thread_cnt > TEST_THREAD_COUNT-1)
		{
			pthread_cond_wait(&cond_wait, &lock);
			TB_LOG(TB_LOG_DBG, "More than 100 tests in parallel. Waiting in queue!\n");
		}

		test_thread_cnt++;
		pthread_mutex_unlock(&lock);

		if (pthread_create(&thread, NULL, testFunc, thread_args) != 0) {
			TB_LOG(TB_LOG_ERR, "Thread creation failed\n");
			free(thread_args);
			status = TB_ERROR;
			goto exit;
		}

		if (wait) {
			clock_gettime(CLOCK_REALTIME, &ts);
			if (thread_args->max_timeout < 480)
				ts.tv_sec += 480;
			else
				ts.tv_sec += thread_args->max_timeout;

			if (pthread_timedjoin_np(thread, (void *) (&response), &ts) != 0) {
				tb_status_group_nod_t* status_group = NULL;
				TB_LOG(TB_LOG_ERR, "Cancelling hanging thread, error %s, (%d)\n", strerror(errno), errno);
				tb_status_lock_mutex();
				status_group = tb_status_lookup_group_node_by_thread(thread);
				if (status_group) {
					pthread_cancel(thread);
				}
				tb_status_release_mutex();
				pthread_join(thread, (void *) (&response));
			}

			if (response == PTHREAD_CANCELED) {
				TB_LOG(TB_LOG_ERR, "Thread %s cancelled\n", name);
				sprintf(resp, "%s :: %s", name, TESTBOX_FAIL);
				if (thread_args->gen_xml_file) {
					suite_end_message_handler();
				}
				return TB_ERROR;
			}
			sprintf(resp, "%s :: %s", name,
					(response->status) ? TESTBOX_FAIL : "TESTBOX_PASS");

			TB_LOG(TB_LOG_DBG, "%s\n", resp);

			if (response->status != TB_OK)
				status = TB_ERROR;

			free(response);
			return status;
		} else {
			pthread_detach(thread);
		}

exit:
	sprintf(resp, "%s", (status) ? "TESTBOX_ERROR" : "TESTBOX_OK");
		return status;
}

/*---------------------------------------------------------------------------*/
int tb_threads_kill_thread(const int index) {
	tb_status_lock_mutex();
	tb_status_group_nod_t *gr = tb_status_lookup_group_node_by_idx(index);
	if (gr->thread != (pthread_t)-1) {
		if ((pthread_cancel(gr->thread)) != TB_OK) {
			tb_status_release_mutex();
			return TB_ERROR;
		}
	}
	tb_status_release_mutex();
	return TB_OK;
}

static void handler_thread_cleanup(void *arg) {
	pthread_mutex_lock(&lock);
	test_thread_cnt--;
	pthread_cond_signal(&cond_wait);
	pthread_mutex_unlock(&lock);
}

static void status_group_cleanup(void *arg) {
	tb_status_group_nod_t* status_group = (tb_status_group_nod_t*)arg;
	tb_status_lock_mutex();
	status_group->thread = (pthread_t)-1;
	if (status_group->action != tb_test_done && status_group->action != tb_test_stopped) {
		status_group->action = tb_test_stopped;
		status_group->endTimeStamp = getTimeStamp();
	}
	tb_status_release_mutex();
}

/*---------------------------------------------------------------------------*/
void *tb_thread_handler_case(void *arg) {
	thread_args_t *data = (thread_args_t *) arg;
	int result = -1;
	int executions = 0;
	struct timespec ts;

	tb_testcase_nod_t* test = NULL;
	tb_status_group_nod_t* status_group = NULL;
	tb_status_suite_nod_t* status_suite = NULL;
	tb_status_testcase_nod_t* status_tc = NULL;
	thread_return_t *thread_return = NULL;

	pthread_cleanup_push(handler_thread_cleanup, NULL);

	if (data->wait_for_response)
		tb_socket_client_add(data->socket);

	test = tb_case_lookupnode(data->node_name);
	if (NULL == test) {
		TB_LOG(TB_LOG_ERR, "Test case [%s] not found, aborting\n",
			data->node_name);
		goto wrapup_thread;
	}

	status_group = tb_status_add_group_node(data->node_name, 1, data->loops);
	if (status_group == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status node failed!");
		goto wrapup_thread;
	}

	pthread_cleanup_push(status_group_cleanup, (void *)status_group);

	status_suite = tb_status_add_suite_bucket(data->node_name, status_group);
	if (status_suite == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status suite bucket failed!");
		goto wrapup_thread;
	}

	status_tc = tb_status_add_testcase_bucket(data->node_name, status_suite);
	if (status_tc == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status testcase bucket failed!");
		goto wrapup_thread;
	}


	status_group->verbose_level = data->verbose_level;
	status_group->action = tb_test_running;
	status_group->startTimeStamp = getTimeStamp();
	if (data->gen_xml_file) {
		suite_start_message_handler(data->node_name);
	}

	result = 0;

	if (data->execution_time != 0) {
		clock_gettime(CLOCK_REALTIME, &ts);
		data->execution_time += ts.tv_sec;
	}
	for (executions = 0; executions < data->loops; executions++) {
		if (tb_exec_run_test(test, data, status_group, status_tc) != 0) {
			result = -1;
		}

		if ((result != 0) && (data->stop_on_fail == 1)) {
			status_group->action = tb_test_stopped;
			goto exit;
		}

		clock_gettime(CLOCK_REALTIME, &ts);
		if (data->execution_time != 0 && (ts.tv_sec > data->execution_time))
		{
			status_group->action = tb_test_done;
			goto exit;
		}
		sleep(data->execution_dly_time);
	}

	if (data->gen_xml_file) {
		suite_end_message_handler();
	}
	status_group->action = tb_test_done;

exit:
	status_group->endTimeStamp = getTimeStamp();
wrapup_thread:
	/* If calling thread expects response, send thread_return, else NULL*/
	thread_return = finalize_thread(result, data, thread_return);
	/* pthread_cleanup_pop must be in pairs with pthread_cleanup_push */
	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	return (void*) thread_return;
}

/*---------------------------------------------------------------------------*/
void *tb_thread_handler_suite(void *arg) {
	thread_args_t *data = (thread_args_t *) arg;
	int result = -1;

	tb_testsuite_nod_t *suite = NULL;
	tb_status_group_nod_t *status_group = NULL;
	tb_status_suite_nod_t *status_suite = NULL;
	thread_return_t *thread_return = NULL;

	pthread_cleanup_push(handler_thread_cleanup, NULL);

	if (data->wait_for_response)
		tb_socket_client_add(data->socket);

	suite = tb_suite_lookupnode(data->node_name);
	if (suite == NULL) {
		TB_LOG(TB_LOG_ERR, "Test suite [%s] not found, aborting\n", data->node_name);
		goto wrapup_thread;
	}

	status_group = tb_status_add_group_node(data->node_name, 1, data->loops);
	if (status_group == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status node failed!");
		goto wrapup_thread;
	}

	pthread_cleanup_push(status_group_cleanup, (void *)status_group);

	status_suite = tb_status_add_suite_bucket(data->node_name, status_group);
	if (status_suite == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status bucket failed!");
		goto wrapup_thread;
	}

	result = tb_status_add_n_testcase_buckets(suite, status_suite);
	if (result) {
		TB_LOG(TB_LOG_ERR, "Allocating status buckets failed!");
		goto wrapup_thread;
	}

	status_group->verbose_level = data->verbose_level;
	status_group->action = tb_test_running;
	status_group->startTimeStamp = getTimeStamp();

	result = tb_exec_run_tests_in_suite(suite, data, status_group, status_suite);

	status_group->endTimeStamp = getTimeStamp();

	if (tb_test_stopped != status_group->action) {
		status_group->action = tb_test_done;
	}

wrapup_thread:
	/* If calling thread expects response, send thread_return, else NULL*/
	thread_return = finalize_thread(result, data, thread_return);
	/* pthread_cleanup_pop must be in pairs with pthread_cleanup_push */
	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	return (void*) thread_return;
}

/*---------------------------------------------------------------------------*/
void *tb_thread_handler_group(void *arg) {
	thread_args_t *data = (thread_args_t *) arg;
	int result = -1;
	int s_cnt = 0;
	tb_status_group_nod_t *status_group = NULL;
	tb_status_suite_nod_t *status_suite = NULL;
	tb_testgroup_nod_t *group = NULL;
	tb_testsuite_nod_t* suite = NULL;
	thread_return_t *thread_return = NULL;

	pthread_cleanup_push(handler_thread_cleanup, NULL);

	if (data->wait_for_response)
		tb_socket_client_add(data->socket);

	group = tb_group_lookupnode(data->node_name);
	if (group == NULL) {
		TB_LOG(TB_LOG_ERR, "Test group [%s] not found, aborting\n", data->node_name);
		goto wrapup_thread;
	}

	status_group = tb_status_add_group_node(group->name, group->size, data->loops);
	if (status_group == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status node failed!");
		goto wrapup_thread;
	}

	pthread_cleanup_push(status_group_cleanup, (void *)status_group);

	while ((suite = tb_group_lookupsuite(group, s_cnt)) != NULL) {
		status_suite = tb_status_add_suite_bucket(suite->name, status_group);
		if (status_suite == NULL) {
			TB_LOG(TB_LOG_ERR, "Allocating status bucket failed!");
			goto wrapup_thread;
		}

		result = tb_status_add_n_testcase_buckets(suite, status_suite);
		if (result) {
			TB_LOG(TB_LOG_ERR, "Allocating status buckets failed!");
			goto wrapup_thread;
		}
		s_cnt++;
	}

	result = tb_exec_run_tests_in_group(group, data, status_group);

wrapup_thread:
	/* If calling thread expects response, send thread_return, else NULL*/
	thread_return = finalize_thread(result, data, thread_return);
	/* pthread_cleanup_pop must be in pairs with pthread_cleanup_push */
	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	return (void*) thread_return;
}

/*---------------------------------------------------------------------------*/
void *tb_thread_handler_executable(void *arg) {
	thread_args_t *data = (thread_args_t *) arg;
	int i, result = -1;
	int executions = 0;
	struct timespec ts;
	char command[TB_MAX_ARGS * TB_STR_LEN] = {0};
	tb_status_group_nod_t* status_group = NULL;
	tb_status_suite_nod_t* status_suite = NULL;
	tb_status_testcase_nod_t* status_tc = NULL;
	thread_return_t *thread_return = NULL;

	pthread_cleanup_push(handler_thread_cleanup, NULL);

	strcat(command, data->argv[0]);
	for (i = 1; i < data->argc; i++) {
		strcat(command, " ");
		strcat(command, data->argv[i]);
	}

	if (data->wait_for_response)
		tb_socket_client_add(data->socket);

	status_group = tb_status_add_group_node(data->node_name, 1, data->loops);
	if (status_group == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status node failed!");
		goto wrapup_thread;
	}

	pthread_cleanup_push(status_group_cleanup, (void *)status_group);

	status_suite = tb_status_add_suite_bucket(data->node_name, status_group);
	if (status_suite == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status bucket failed!");
		goto wrapup_thread;
	}

	status_tc = tb_status_add_testcase_bucket(data->node_name, status_suite);
	if (status_tc == NULL) {
		TB_LOG(TB_LOG_ERR, "Allocating status bucket failed!");
		goto wrapup_thread;
	}

	status_group->verbose_level = data->verbose_level;
	status_group->action = tb_test_running;
	status_group->startTimeStamp = getTimeStamp();

	result = 0;

	if (data->gen_xml_file) {
		suite_start_message_handler(data->node_name);
	}

	if (data->execution_time != 0) {
		clock_gettime(CLOCK_REALTIME, &ts);
		data->execution_time += ts.tv_sec;
	}

	for (executions = 0; executions < data->loops; executions++) {

		if (tb_exec_run_executable(command, status_tc) != 0) {
			result = -1;
		}

		if ((result != 0) && (data->stop_on_fail == 1)) {
			status_group->action = tb_test_stopped;
			goto exit;
		}
		clock_gettime(CLOCK_REALTIME, &ts);
		if (data->execution_time != 0 && (ts.tv_sec > data->execution_time))
		{
			status_group->action = tb_test_done;
			goto exit;
		}
	}
	if (data->gen_xml_file) {
		suite_end_message_handler();
	}

	status_group->action = tb_test_done;

exit:
	status_group->endTimeStamp = getTimeStamp();
wrapup_thread:
	/* If calling thread expects response, send thread_return, else NULL*/
	thread_return = finalize_thread(result, data, thread_return);
	/* pthread_cleanup_pop must be in pairs with pthread_cleanup_push */
	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	return (void*) thread_return;
}
