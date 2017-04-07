
#include <stdio.h>
#include <stdlib.h> /* strtol... */
#include <string.h> /* memcpy, strncpy */
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <testboxd_args.h>
#include <testboxd_threads.h>
#include <testboxd_libs.h>
#include <testboxd_sockets.h>
#include "testboxd_types.h"
#include "testboxd_print.h"
#include <libtestbox_log.h>
#include <libtestbox_verb.h>
#include "libtestbox_exec.h"
#include "libtestbox_xmlfilegen.h"
#include "libtestbox_socket.h"

char *testbox_cfg_dir = NULL;
int run_as_daemon = 1; /* default for old S99rcs-start */

/******************************************************************************
 * Local defines
 *****************************************************************************/
#define LOG_FILE "/var/log/testboxd.log"

/******************************************************************************
 * Private function forward declarations
 *****************************************************************************/
static void stepmode(const tb_testsuite_nod_t* suite);

/******************************************************************************
 * Local functions
 *****************************************************************************/
static void stepmode(const tb_testsuite_nod_t* suite) {
	int c = 0;
	int k = 0;
	int i = 0;
	int status = 0;
	static struct termios oldt;
	static struct termios newt;
	tb_testcase_nod_t *test = NULL;

	char *help = "\nUSAGE:\n"
			"key:"
			" a            Run current case and step to next case\n"
			"     b            Run current case and step to previous case\n"
			"     c            Run current case\n"
			"     n            Step to next case\n"
			"     p            Step to previous case\n"
			"     r            Run until end of suite\n"
			"     e            End stepping mode\n"
			"     h            Print this help\n"
			"\n";

	/*tcgetattr gets the parameters of the current terminal
	 STDIN_FILENO will tell tcgetattr that it should write the settings
	 of stdin to oldt*/
	tcgetattr(STDIN_FILENO, &oldt);
	/*now the settings will be copied*/
	newt = oldt;

	/*ICANON normally takes care that one line at a time will be processed
	 that means it will return if it sees a "\n" or an EOF or an EOL*/
	newt.c_lflag &= ~(ICANON);

	/* Turn off echo*/
	newt.c_lflag &= ~(ICANON | ECHO);

	/*Those new settings will be set to STDIN
	 TCSANOW tells tcsetattr to change attributes immediately. */
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	k = tb_suite_getsizenode(suite);
	test = tb_suite_lookupcase(suite, i);

	TB_PRINTF("Stepping in suite: [%s]\n", suite->name);
	TB_PRINTF("Number of testcases: [%d]\n", k);
	TB_PRINTF("%s", help);
	TB_PRINTF("Execute ->  %s  %s ?\n", suite->name, test->name);

	do {
		switch (c) {
		case 'a': {
			TB_LOG(TB_LOG_DBG, "Pressed a\n");
			status = tb_exec_test_function(test->argc, test->args, test->function);
			TB_PRINTF("%s :: %s\n", test->name, (status) ? "TESTBOX_FAIL" : "TESTBOX_PASS");
			i++;
			if (i >= k) {
				TB_PRINTF("End of suite.\n");
				i--;
			}
			break;
		}

		case 'b': {
			TB_LOG(TB_LOG_INFO, "Pressed b\n");
			status = tb_exec_test_function(test->argc, test->args, test->function);
			TB_PRINTF("%s :: %s\n", test->name, (status) ? "TESTBOX_FAIL" : "TESTBOX_PASS");
			i--;
			if (i <= -1) {
				TB_PRINTF("Beginning of suite.\n");
				i = 0;
			}
			break;
		}

		case 'c': {
			TB_LOG(TB_LOG_INFO, "Pressed c\n");
			status = tb_exec_test_function(test->argc, test->args, test->function);
			TB_PRINTF("%s :: %s\n", test->name, (status) ? "TESTBOX_FAIL" : "TESTBOX_PASS");
			break;
		}

		case 'n': {
			TB_LOG(TB_LOG_INFO, "Pressed n\n");
			i++;
			if (i >= k) {
				TB_PRINTF("End of suite.\n");
				i--;
			}
			break;
		}

		case 'p': {
			TB_LOG(TB_LOG_INFO, "Pressed p\n");
			i--;
			if (i <= -1) {
				TB_PRINTF("Beginning of suite.\n");
				i = 0;
			}
			break;
		}

		case 'r': {
			TB_LOG(TB_LOG_INFO, "Pressed r\n");
			while (i < k) {
				status = tb_exec_test_function(test->argc, test->args, test->function);
				TB_PRINTF("%s :: %s\n", test->name, (status) ? "TESTBOX_FAIL" : "TESTBOX_PASS");
				i++;
				if (i >= k) {
					TB_PRINTF("End of suite.\n");
					i--;
					break;
				}
				test = tb_suite_lookupcase(suite, i);
			}
			break;
		}

		case 'h': {
			TB_LOG(TB_LOG_INFO, "Pressed h\n");
			TB_PRINTF("%s", help);
			break;
		}

		default: {
			TB_LOG(TB_LOG_INFO, "Stepping, entered default case.");
			c = 0;
			break;
		}
		}
		if (c != 0) {
			test = tb_suite_lookupcase(suite, i);
			TB_PRINTF("--------------------------------------------------------------------------------\n");
			TB_PRINTF("Execute ->  %s  %s ?\n", suite->name, test->name);
		}
	} while ((c = getchar()) != 'e');

	TB_PRINTF("Quit stepping...\n");
	/* Restore the old settings */
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int cmd_parse(int argc, char **argv, int client_sock_fd, char *resp) {
	int status = TB_OK;
	char str[TB_STR_LEN] = { 0 };
	int i = 0;
	int result = TB_ERROR;
	int level = 0;
	int sec = 0;
	tb_testtype_t run = TB_UNKNOWN_TEST_TYPE;
	tb_testsuite_nod_t *s = NULL;
	tb_testcase_nod_t *c = NULL;
	FILE *fp;
	char log_buf[TB_STR_LEN_LOG] = { 0 };
	int n = 0;
	void * (*threadfunc)(void *) = NULL;
	thread_args_t *thread_args = NULL;

	if (argc == 1) {
		print_help();
		status = TB_OK;
		goto exit;
	}

	/* Parse cmd line for utils, do your stuff and return */
	while (i < argc) {
		if (strncmp("--clear", argv[i], TB_STR_LEN) == 0) {
			if (strncmp("all", argv[i + 1], TB_STR_LEN) == 0) {
				tb_status_deleteall();
				status = TB_OK;
			} else if ((atoi(argv[i + 1]) >= 0) && (atoi(argv[i + 1]) < tb_status_getsize_group_reg())) {
				tb_status_delete_group_node(atoi(argv[i + 1]));
				status = TB_OK;
			}
			goto exit;

		} else if (strcmp("--help", argv[i]) == 0) {
			print_help();
			status = TB_OK;
			goto exit;

		} else if (strcmp("--descr", argv[i]) == 0) {
			c = tb_case_lookupnode(argv[i + 1]);
			s = tb_suite_lookupnode(argv[i + 1]);
			if (c) {
				status = TB_OK;
				TB_PRINTF("CASE: %s\n", c->name);
				TB_PRINTF("DESCR: %s\n", c->description);
			} else if (s) {
				status = TB_OK;
				TB_PRINTF("SUITE: %s\n", s->name);
				TB_PRINTF("DESCR: %s\n", s->descr);
			} else {
				TB_LOG(TB_LOG_ERR, "Case/suite [%s] not found, aborting\n", argv[i + 1]);
				status = TB_ERROR;
			}
			goto exit;

		} else if (strcmp("--kill", argv[i]) == 0) {
			result = tb_threads_kill_thread(atoi(argv[i + 1]));
			if (result)
				status = TB_OK;
			goto exit;

		} else if (strcmp("--status", argv[i]) == 0) {
			tb_print_status();
			status = TB_OK;
			goto exit;

		} else if (strcmp("--list", argv[i]) == 0) {
			if (argc > i+1) {
				tb_print_suites_and_funcs(argv[i+1]);
			}
			else {
				tb_print_suites_and_funcs("all");
			}
			status = TB_OK;
			goto exit;

		} else if (strcmp("--examples", argv[i]) == 0) {
			tb_print_examples();
			status = TB_OK;
			goto exit;

		} else if (strcmp("--step", argv[i]) == 0) {
			if (argc == 3) {
				s = tb_suite_lookupnode(argv[i + 1]);
				if (NULL != s) {
					stepmode(s);
					status = TB_OK;
				} else {
					TB_LOG(TB_LOG_ERR, "Suite [%s] not found, aborting\n", argv[i + 1]);
					status = TB_ERROR;
				}
			} else {
				TB_PRINTF("Too few arguments.\n");
				TB_LOG(TB_LOG_ERR, "Too few arguments.\n");
				status = TB_ERROR;
			}
			goto exit;

		} else if (strcmp("--group_from_file", argv[i]) == 0) {
			result = add_group_from_file(argv[i + 1]);
			if (result) {
				TB_LOG(TB_LOG_ERR, "add_group_from_file failed\n");
				status = TB_ERROR;
			}
			goto exit;

		} else if (strcmp("--loadlib", argv[i]) == 0) {
			result = load_testcase_from_lib(argv[i + 1]);
			if (result) {
				TB_LOG(TB_LOG_ERR, "load_testcase_from_lib returned %d\n", result);
				status = TB_ERROR;
			}
			goto exit;
		} else if (strcmp("--unloadlib", argv[i]) == 0) {
			result = unload_lib(argv[i + 1]);
			if (result) {
				TB_LOG(TB_LOG_ERR, "unload_lib returned %d\n", result);
				status = TB_ERROR;
			}
			goto exit;
		} else if (strcmp("--listlib", argv[i]) == 0) {
			result = list_libs();
			if (result) {
				TB_LOG(TB_LOG_ERR, "list_libs returned %d\n", result);
				status = TB_ERROR;
			}
			goto exit;
		} else if (strcmp("--find", argv[i]) == 0) {
			if (argc > 2) {
				TB_PRINTF("\n");
				tb_case_find_funcs((argv[i + 1]));
				tb_suite_find_suite((argv[i + 1]));
				status = TB_OK;
			}
			else {
				TB_PRINTF("Too few arguments.\n");
				TB_LOG(TB_LOG_ERR, "Too few arguments.\n");
				status = TB_ERROR;
			}
			goto exit;

		} else if (strcmp("--log_level", argv[i]) == 0) {
			if (argc == 3) {
				logLevel(atoi(argv[i + 1]));
			}
			TB_PRINTF("Current log level is [%d]\n", logLevel(TB_GET_LOG_LEVEL));
			goto exit;

		} else if (strcmp("--log_read", argv[i]) == 0) {
			fp = fopen(LOG_FILE, "r");
			if (fp == NULL ) {
				TB_PRINTF("Log is empty\n");
				status = TB_OK;
			} else {
				while (fgets(log_buf, sizeof(log_buf), fp) != NULL ) {
					n = tb_socket_send_data(log_buf, sizeof(log_buf), tb_sock_msg_rsp);
					if (n < 0) {
						TB_LOG(TB_LOG_ERR, "error writing log file to socket\n");
						status = TB_ERROR;
					}
				}
				fclose(fp);
			}
			goto exit;

		} else if (strcmp("--log_clear", argv[i]) == 0) {
			system("> /var/log/testboxd.log");
			TB_PRINTF("Log cleared\n");
			status = TB_OK;
			goto exit;

		} else if (strcmp("--verbose_level", argv[i]) == 0) {
			if (argc == 3) {
				level = strtol(argv[i + 1], NULL, 10);
				if ((level >= TB_VERB_OFF) && (level <= TB_VERB_OFF_LOG)) {
					status = set_verbose_level(level);
				} else {
					TB_PRINTF("Input level is [%d]. Should be between %d and %d\n", level, TB_VERB_OFF,
							TB_VERB_OFF_LOG);
					status = TB_ERROR;
				}
			}
			TB_PRINTF("Current default verbose level is [%d]\n", get_verbose_level());
			goto exit;

		}  else if (strcmp("--xml_file", argv[i]) == 0) {
			if (strncmp("open", argv[i + 1], 4) == 0) {
				set_output_filename(NULL);
				initialize_result_file();
				status = TB_OK;
			}
			else if (strncmp("close", argv[i + 1], 5) == 0) {
				status = uninitialize_result_file(NULL);
			}
			else {
				TB_PRINTF("Invalid argument '%s'\n", argv[i + 1]);
				status = TB_ERROR;
			}
			goto exit;
		}
		i++;
	}

	/*
	 * OK, so no util, parse cmd line for options and set flags, the rest
	 * shall be forwarded to test function
	 */

	thread_args = creat_default_thread_args();
	/* save the client socket fd for printing */
	thread_args->socket = client_sock_fd;

	i = 1;
	while (i < argc) {
		TB_LOG(TB_LOG_DBG, "argv[%d]=[%s]\n", i, argv[i]);
		if (strcmp("--verbose", argv[i]) == 0) {  //Override default verbose level
			thread_args->verbose_level = strtol(argv[i + 1], NULL, 10);
			if (thread_args->verbose_level < TB_VERB_OFF || thread_args->verbose_level > TB_VERB_OFF_LOG) {
				TB_PRINTF("--verbose level [%d] is out of range\n", thread_args->verbose_level);
				status = TB_ERROR;
				goto exit;
			}
			if (thread_args->verbose_level == TB_VERB_OFF ||
				thread_args->verbose_level == TB_VERB_OFF_LOG) {
				thread_args->wait_for_response = 0;
			}
			else {
				thread_args->wait_for_response = 1;
			}
			TB_LOG(TB_LOG_DBG, "--verbose set to %d\n", thread_args->verbose_level);
			TB_LOG(TB_LOG_DBG, "--wait set to %d\n", thread_args->wait_for_response);
			i++; /* we do not want to parse the level */

		} else if (strcmp("--extend", argv[i]) == 0) {
			thread_args->extended = 1;
			TB_LOG(TB_LOG_DBG, "--extend set\n");

		} else if (strcmp("--extended", argv[i]) == 0) {
			thread_args->extended = 1;
			TB_LOG(TB_LOG_DBG, "--extended set\n");

		} else if (strcmp("--xml", argv[i]) == 0) {
			thread_args->gen_xml_file = 1;
			TB_LOG(TB_LOG_DBG, "--xml set\n");

		} else if (strcmp("--loop", argv[i]) == 0) {
			thread_args->loops = strtol(argv[i + 1], NULL, 10);
			if (thread_args->loops <= 0) {
				TB_PRINTF("--loop [%d] is out of range\n", thread_args->loops);
				status = TB_ERROR;
				goto exit;
			}
			TB_LOG(TB_LOG_DBG, "--loop set to %d\n", thread_args->loops);
			i++; /* we do not want to parse the loop number */

		} else if (strcmp("--stoponfail", argv[i]) == 0) {
			thread_args->stop_on_fail = 1;
			TB_LOG(TB_LOG_DBG, "--stoponfail set\n");

		} else if (strcmp("--wait", argv[i]) == 0) {
			thread_args->wait_for_response = 1;
			if (thread_args->verbose_level == TB_VERB_OFF ||
				thread_args->verbose_level == TB_VERB_OFF_LOG) {
				thread_args->verbose_level = TB_VERB_RESP; //response to user
			}
			TB_LOG(TB_LOG_DBG, "--wait set\n");
			TB_LOG(TB_LOG_DBG, "--verbose set to %d\n", thread_args->verbose_level);

		} else if (strcmp("--maxtmo", argv[i]) == 0) {
			thread_args->max_timeout = strtol(argv[i + 1], NULL, 10);
			if (thread_args->max_timeout <= 0) {
				TB_PRINTF("--maxtmo [%d] is out of range\n", thread_args->max_timeout);
				status = TB_ERROR;
				goto exit;
			}
			TB_LOG(TB_LOG_DBG, "--maxtmo set to %d\n", thread_args->max_timeout);
		} else if (strcmp("--exe_time", argv[i]) == 0) {
			if(time_str_to_sec(argv[i + 1], &sec)) {
				TB_PRINTF("usage: --exe_time 00:00:00:00\n");
				status = TB_ERROR;
				goto exit;
			}
			thread_args->execution_time = sec;
			thread_args->loops = 0x7fffffff;
			if (thread_args->execution_time <= 0) {
				TB_PRINTF("--exe_time [%d] is out of range\n", thread_args->execution_time);
				status = TB_ERROR;
				goto exit;
			}
			TB_LOG(TB_LOG_DBG, "--execution_time set to %d second(s)\n", thread_args->execution_time);
			i++; /* we do not want to parse the time_str number */
		} else if (strcmp("--exe_dly_time", argv[i]) == 0) {
			if(time_str_to_sec(argv[i + 1], &sec)) {
				TB_PRINTF("usage: --exe_dly_time 00:00:00:00\n");
				status = TB_ERROR;
				goto exit;
			}
			thread_args->execution_dly_time = sec;
			if (thread_args->execution_dly_time <= 0) {
				TB_PRINTF("--exe_dly_time [%d] is out of range\n", thread_args->execution_dly_time);
				status = TB_ERROR;
				goto exit;
			}
			TB_LOG(TB_LOG_DBG, "--execution_dly_time set to %d second(s)\n", thread_args->execution_dly_time);
			i++; /* we do not want to parse the time_str number */
		} else if ((threadfunc == NULL) && ((run = tb_exec_find_group_suite_or_case(argv[i])) != TB_UNKNOWN_TEST_TYPE)) {
			strncpy(thread_args->node_name, argv[i], TB_STR_LEN - 1);
			strncpy(thread_args->argv[0], argv[i], TB_STR_LEN - 1);
			strncpy(str, argv[i], TB_STR_LEN - 1);
			thread_args->argc++;
			if (run == TB_GROUP) {
				threadfunc = tb_thread_handler_group;
			} else if (run == TB_SUITE) {
				threadfunc = tb_thread_handler_suite;
			} else if (run == TB_TESTCASE) {
				threadfunc = tb_thread_handler_case;
			}
		} else if ((threadfunc == NULL) && ((run = tb_exec_check_executable(argv[i])) != TB_UNKNOWN_TEST_TYPE)) {
			strncpy(thread_args->node_name, argv[i], TB_STR_LEN - 1);
			strncpy(thread_args->argv[0], argv[i], TB_STR_LEN - 1);
			strncpy(str, argv[i], TB_STR_LEN - 1);
			thread_args->argc++;
			if (run == TB_EXEC) {
				threadfunc = tb_thread_handler_executable;
			}
		} else { /* this is a option to the testcase */
			strncpy(thread_args->argv[thread_args->argc], argv[i], TB_STR_LEN - 1);
			thread_args->argc++;
		}
		i++;
	}

	/* If verbose_mode 3 and exe_time or loops is set to something from the
	 * user, make sure exe_dly_time is set to a value higher than 0 (since
	 * logging to disk is so slow). */
	if (thread_args->verbose_level == TB_VERB_OFF_LOG &&
			thread_args->execution_dly_time == 0 &&
			(thread_args->execution_time != 0 ||
			thread_args->loops != 1)){
		TB_PRINTF("The --exe_dly_time is not allowed to be 0 in "
				"verbose level %d, aborting\n",
				TB_VERB_OFF_LOG);
		status = TB_ERROR;
		goto exit;
	}

#if(0)
	/* for debug */
	for (int d = 0; d <= thread_args->argc; d++) {
		TB_LOG(TB_LOG_DBG, "thread_args->argv[%d]=%s\n", d, thread_args->argv[d]);
	}
#endif

	/* basic check that we do not run if do not have any valid group/suite/test */
	if (NULL == threadfunc) {
		TB_PRINTF("Valid executable, group, suite or case not found in input string, aborting\n");
		status = TB_ERROR;
		goto exit;
	}

	return exec_test_thread(thread_args->wait_for_response, threadfunc, thread_args, str, resp);

exit:

	if (thread_args)
		free(thread_args);
	sprintf(resp, "%s", (status) ? "TESTBOX_ERROR" : "TESTBOX_OK");
	return status;
}

int startup_args(int argc, char *argv[]) {
	int i = 0;

	for (i = 0; i < argc; i++) {
		if (strcmp("--socket_type", argv[i]) == 0) {
			if (strcmp("local", argv[i + 1]) == 0) {
				tb_socket_set(AF_UNIX);
				TB_LOG(TB_LOG_DBG, "Using --socket_type AF_UNIX\n");
			} else if (strcmp("inet", argv[i + 1]) == 0) {
				tb_socket_set(AF_INET);
				TB_LOG(TB_LOG_DBG, "Using --socket_type AF_INET\n");
			} else {
				TB_LOG(TB_LOG_DBG, "Invalid parameter %s=%s\n", argv[i], argv[i + 1]);
				return -1;
			}

			i++;
		} else if (strcmp("--log_level", argv[i]) == 0) {
			if (logLevel(atoi(argv[i + 1])) == -1) {
				TB_LOG(TB_LOG_ERR, "Invalid parameter %s=%s\n", argv[i], argv[i + 1]);
			} else {
				TB_LOG(TB_LOG_DBG, "Setting --log_level to %d\n", atoi(argv[i + 1]));
			}

			i++;
		} else if (strcmp("--config", argv[i]) == 0) {
			if (argc <= (i+1)) {
				TB_LOG(TB_LOG_ERR, "Invalid parameter %s : argument required\n", argv[i]);
			} else {
				char *tmp = strdup(argv[i + 1]);
				struct stat file_stat;

				if (access(tmp, X_OK))
					goto bad_arg;
				if (stat(tmp, &file_stat))
					goto bad_arg;
				if (!S_ISDIR(file_stat.st_mode))
					goto bad_arg;

				i++;
				testbox_cfg_dir = tmp;
				TB_LOG(TB_LOG_DBG, "Setting testbox config dir to %s\n", testbox_cfg_dir);
				continue;
			bad_arg:
				free(tmp);
				TB_LOG(TB_LOG_ERR, "Invalid parameter %s=%s\n", argv[i], argv[i + 1]);
			}
		}  else if (strcmp("--inhibit_daemonize", argv[i]) == 0) {
			run_as_daemon = 0;
		} else {
			//Do nothing
		}
	}
	return 0;
}
