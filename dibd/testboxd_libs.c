/******************************************************************************
 * Copyright (c) Ericsson AB 2012 All rights reserved.
 *
 * The information in this document is the property of Ericsson.
 *
 * Except as specifically authorized in writing by Ericsson, the
 * receiver of this document shall keep the information contained
 * herein confidential and shall protect the same in whole or in
 * part from disclosure and dissemination to third parties.
 *
 * Disclosure and dissemination to the receivers employees shall
 * only be made on a strict need to know basis.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h> /* getenv... */
#include <dlfcn.h>
#include <pthread.h>
#include <errno.h>
#include <libgen.h> /* basename... */
#include "testboxd_libs.h"
#include "testboxd_utilities.h"
#include "testboxd_types.h"
#include "libtestbox_log.h"
#include "libtestbox_verb.h"
#include "libtestbox_types.h"
#include "libtestbox_exec.h"
#include "libtestbox_utils.h"
#include "libtestbox_groupdb.h"
#include "libtestbox_suitedb.h"
#include "libtestbox_casedb.h"

static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* linked list with loaded test libraries */
static tb_test_lib_t * volatile libs = NULL;

char * create_path_from_env(char *orig_path) {
	char *envstart = NULL;
	char *envend = NULL;
	char *envpath = NULL;
	char envsubstr[TB_STR_LEN];
	char envvar[TB_STR_LEN];

	if ((envstart = strstr(orig_path, "$(")) != NULL) { //Check for env
		if ((envend = strstr(envstart, ")")) != NULL) { //variable e.g. $(MYDIR)

			strncpy(envsubstr, envstart, envend - envstart + 1); //$(MYDIR)
			envsubstr[envend - envstart + 1] = '\0';

			strncpy(envvar, &envstart[2], envend - envstart - 2); //MYDIR
			envvar[envend - envstart - 2] = '\0';

			if ((envpath = getenv(envvar)) != NULL) {
				return replace_string(orig_path, envsubstr, envpath);
			}
			else {
				TB_LOG(TB_LOG_ERR, "Unknown env variable? '%s'\n", envvar);
			}
		} else {
			TB_LOG(TB_LOG_WARN, "Incomplete env variable? '%s'\n", orig_path);
		}
	}
	return NULL;
}

/*
 * Find the path config file on file system and then load all libraries
 * matching the paths within it.
 */
void load_libraries(void) {
	char test_libs_conf_path[TB_MAX_PATH_LEN] = "";
	int status = -1;

	if (testbox_cfg_dir) {
		strncpy(test_libs_conf_path, testbox_cfg_dir, TB_MAX_PATH_LEN - 1);
		strcat(test_libs_conf_path, "/");
		strncat(test_libs_conf_path, basename(TEST_LIBS_CONF), TB_MAX_PATH_LEN - 1);
	} else {
		const char *testboxdir = getenv("TESTBOXDIR");

		strncpy(test_libs_conf_path, testboxdir, TB_MAX_PATH_LEN - 1);
		strncat(test_libs_conf_path, TEST_LIBS_CONF, TB_MAX_PATH_LEN - 1);
	}
	TB_LOG(TB_LOG_DBG, "test_libs_conf_path = [%s]", test_libs_conf_path);
	/* There is only one config file per arch */
	status = find_libs_from_conf_file(test_libs_conf_path);
	if (status != TB_OK) {
		TB_LOG(TB_LOG_ERR, "Error finding/loading libraries using path [%s]", test_libs_conf_path);
	}
}

/*---------------------------------------------------------------------------
void load_predefined_suites(void) {
	char testbox_predef_suite_path[TB_MAX_PATH_LEN] = "";

	if (testbox_cfg_dir) {
                strncpy(testbox_predef_suite_path, testbox_cfg_dir, TB_MAX_PATH_LEN - 1);
		strcat(testbox_predef_suite_path, "/");
                strncat(testbox_predef_suite_path, basename(PRE_DEFINED_SUITE_CONF), TB_MAX_PATH_LEN - 1);
        } else {
		const char *testboxdir = getenv("TESTBOXDIR");

		strncpy(testbox_predef_suite_path, testboxdir, TB_MAX_PATH_LEN - 1);
		strncat(testbox_predef_suite_path, PRE_DEFINED_SUITE_CONF, TB_MAX_PATH_LEN - 1);
	}
	TB_LOG(TB_LOG_DBG, "predefined suites conf path = [%s]", testbox_predef_suite_path);
	if (add_group_from_file(testbox_predef_suite_path)) {
		TB_LOG(TB_LOG_ERR, "Error loading predefined suites from [%s]", testbox_predef_suite_path);
	} else {
		TB_LOG(TB_LOG_DBG, "Predefined suites loaded from [%s]", testbox_predef_suite_path);
	}

}

---------------------------------------------------------------------------*/
void load_group_from_file(void) {
	char testbox_predef_suite_path[TB_MAX_PATH_LEN] = "";

	if (testbox_cfg_dir) {
                strncpy(testbox_predef_suite_path, testbox_cfg_dir, TB_MAX_PATH_LEN - 1);
		strcat(testbox_predef_suite_path, "/");
                strncat(testbox_predef_suite_path, basename(GROUP_FROM_FILE_CONF), TB_MAX_PATH_LEN - 1);
        } else {
		const char *testboxdir = getenv("TESTBOXDIR");

		strncpy(testbox_predef_suite_path, testboxdir, TB_MAX_PATH_LEN - 1);
		strncat(testbox_predef_suite_path, GROUP_FROM_FILE_CONF, TB_MAX_PATH_LEN - 1);
	}
	TB_LOG(TB_LOG_DBG, "group from file conf path = [%s]", testbox_predef_suite_path);
	if (find_groups_from_conf_file(testbox_predef_suite_path)) {
		TB_LOG(TB_LOG_ERR, "Error loading group from file [%s]", testbox_predef_suite_path);
	} else {
		TB_LOG(TB_LOG_DBG, "Group from file loaded from [%s]", testbox_predef_suite_path);
	}

}

/*---------------------------------------------------------------------------*/
/* Find groups conf from path in conf file */
int find_groups_from_conf_file(const char *filename) {
	int status = TB_OK;
	char lib_path[TB_MAX_PATH_LEN];
	FILE *file;
	char *path;

	file = fopen(filename, "r");
	if (file != NULL) {
		while (fgets(lib_path, sizeof(lib_path), file) != NULL) {
			strip_newline(lib_path, strlen(lib_path));

			if ((strncmp("#", lib_path, 1) == 0) || (strlen(lib_path) == 0)) {
				//do nothing with this line
			} else {

				if((path = create_path_from_env(lib_path)) == NULL) {
					TB_LOG(TB_LOG_DBG, "path does not use env variable '%s'\n", path);
					path = lib_path; //Default if no env varibles are used
				}

                if (add_group_from_file(path)) {
                  TB_LOG(TB_LOG_ERR, "Error loading group from file[%s]", path);
                } else {
                  TB_LOG(TB_LOG_DBG, "Loaded group from file [%s]", path);
                }

				if (status) {
					TB_LOG(TB_LOG_ERR, "Failed load group from file: %s\n", path);
				} else {
					TB_LOG(TB_LOG_DBG, "Loaded group from file: %s\n", path);
				}

				if (path != lib_path) {
					free(path);
				}
			}
		}
	} else {
		TB_LOG(TB_LOG_ERR, "Could not open [%s]\n", filename);
		status = TB_ERROR;
		goto exit;
	}

	fclose(file);
	exit: return status;
}


/*---------------------------------------------------------------------------*/
/* Find shared libs from path in conf file */
int find_libs_from_conf_file(const char *filename) {
	int status = TB_OK;
	char lib_path[TB_MAX_PATH_LEN];
	FILE *file;
	char *path;

	file = fopen(filename, "r");
	if (file != NULL) {
		while (fgets(lib_path, sizeof(lib_path), file) != NULL) {
			strip_newline(lib_path, strlen(lib_path));

			if ((strncmp("#", lib_path, 1) == 0) || (strlen(lib_path) == 0)) {
				//do nothing with this line
			} else {

				if((path = create_path_from_env(lib_path)) == NULL) {
					TB_LOG(TB_LOG_DBG, "path does not use env variable '%s'\n", path);
					path = lib_path; //Default if no env varibles are used
				}

				status = load_testcase_from_lib(path);

				if (status) {
					TB_LOG(TB_LOG_ERR, "Failed load library: %s\n", path);
				} else {
					TB_LOG(TB_LOG_DBG, "Loaded library: %s\n", path);
				}

				if (path != lib_path) {
					free(path);
				}
			}
		}
	} else {
		TB_LOG(TB_LOG_ERR, "Could not open [%s]\n", filename);
		status = TB_ERROR;
		goto exit;
	}

	fclose(file);
	exit: return status;
}

/*
 * Load suites and test cases from lib
 */
int load_testcase_from_lib(const char *lib) {
	int status = TB_OK;
	const char *error;
	testbox_testgroups_t *imported_lib_group = NULL;
	tb_testcase_nod_t testcase_info;
	tb_testcase_nod_t* tc_test = NULL;
	tb_testsuite_nod_t *suite_info = NULL;
	tb_testsuite_nod_t *su_test = NULL;
	tb_testgroup_nod_t *group_info = NULL;
	tb_test_lib_t *curr_lib = NULL;
	int i = 0, j = 0, k = 0;

	curr_lib = (tb_test_lib_t*) calloc(1, sizeof(tb_test_lib_t));
	if (NULL == curr_lib) {
		TB_LOG(TB_LOG_ERR, "calloc failed: (%s)", strerror(errno));
		status = TB_ERROR;
		goto exit;
	}

	/* Load library */
	curr_lib->lib_handle = dlopen(lib, RTLD_NOW | RTLD_GLOBAL);
	if (!curr_lib->lib_handle) {
		TB_LOG(TB_LOG_ERR, "Couldn't open lib: %s\nFailed with error: %s\n", lib, dlerror());
		status = TB_ERROR;
		free(curr_lib);
		goto exit;
	} else {
		strncpy(curr_lib->lib_name, lib, TB_MAX_PATH_LEN - 1);
		/* Lock libs */
		pthread_mutex_lock(&lock);
		/* Add lib to libs */
		curr_lib->next_lib = libs;
		libs = curr_lib;
		/* Unlock libs */
		pthread_mutex_unlock(&lock);
	}

	/* Clear any existing error */
	dlerror();
	/* Get symbol */
	imported_lib_group = (testbox_testgroups_t*) dlsym(curr_lib->lib_handle, "lib_group");
	if ((error = dlerror())) {
		TB_LOG(TB_LOG_ERR, "Couldn't find symbol lib_group in lib: %s\nFailed with error: %s\n", curr_lib->lib_name, error);
		status = TB_ERROR;
		dlclose(curr_lib->lib_handle);
		/* Lock libs */
		pthread_mutex_lock(&lock);
		/* Remove lib from libs */
		libs = curr_lib->next_lib;
		/* Unlock libs */
		pthread_mutex_unlock(&lock);
		free(curr_lib);
		goto exit;
	}

	for (k = 0; k < imported_lib_group->number_of_groups; k++) {
		testbox_testgroup_t *group = &imported_lib_group->groups[k];

		TB_LOG(TB_LOG_DBG, "Loading lib group: '%s'\n", group->group_name);

		/* Warn if group exist */
		group_info = tb_group_lookupnode(group->group_name);
		if (group_info != NULL) {
			TB_LOG(TB_LOG_WARN, "Group %s already exists!", group->group_name);
		}
		group_info = tb_group_addnode(group->group_name);

		for (i = 0; i < group->number_of_suites; i++) {
			testbox_testsuite_t *suite = &group->suites[i];

			TB_LOG(TB_LOG_DBG, "Adding lib suite: '%s'\n", suite->suite_name);

			/* Warn if suite exist */
			suite_info = tb_suite_lookupnode(suite->suite_name);
			if (suite_info != NULL) {
				TB_LOG(TB_LOG_WARN, "Suite %s already exists!", suite->suite_name);
			}
			suite_info = tb_suite_addnode(suite->suite_name);
			strncpy(suite_info->descr, suite->suite_descr, TB_STR_LEN - 1);

			for (j = 0; j < suite->number_of_tests; j++) {
				testbox_testcase_t *tc = &suite->testcases[j];

				if (tc->valid_board & tb_getboardtype()) {
					TB_LOG(TB_LOG_DBG, "Adding tc: '%s'\n", tc->test_name);

					strncpy(testcase_info.name, tc->test_name, TB_STR_LEN - 1);
					strncpy(testcase_info.description, tc->test_description,
							TB_STR_LEN - 1);

					testcase_info.valid_board = tc->valid_board;
					testcase_info.is_extended = tc->is_extended;
					testcase_info.function = tc->function;
					testcase_info.is_executable = TB_FALSE;
					testcase_info.argc = 1;
					strncpy(testcase_info.args[0], testcase_info.name,
							TB_STR_LEN - 1);

					tc_test = tb_suite_addtest(suite_info, testcase_info);
					if (NULL == tc_test) {
						TB_LOG(TB_LOG_ERR, "%s was NOT added to %s",
								testcase_info.name, suite_info->name);
						status++;
					}
					if (tb_case_lookupnode(tc->test_name) == NULL) {
						tc_test = tb_case_addnode(testcase_info);
						if (NULL == tc_test) {
							TB_LOG(TB_LOG_ERR, "%s was NOT added to case registry", testcase_info.name);
							status++;
						}
					} else {
						TB_LOG(TB_LOG_DBG, "[%s] already exists in registry\n", tc->test_name);
					}
				} else {
					TB_LOG(TB_LOG_DBG,
							"tc [%s] with valid_board [%d] is not valid for current board [%d]\n",
							tc->test_name, tc->valid_board, tb_getboardtype());
				}
			}
			su_test = tb_group_addsuite(group_info, suite_info);
			if (NULL == su_test) {
				TB_LOG(TB_LOG_ERR, "%s was NOT added to %s", suite_info->name, group_info->name);
				status++;
			}
		}
	}

exit: if (status) {
		return TB_ERROR;
	} else {
		return TB_OK;
	}
}

/*
 * Unload library
 */
int unload_lib(const char *lib) {

	/* Define some variables to be used */
	int status = TB_OK;
	const char *error;
	testbox_testgroups_t *imported_ulib_group = NULL;
	tb_test_lib_t *curr_ulib = NULL;
	tb_test_lib_t *tempnode = NULL;
	tb_test_lib_t *tempnode2 = NULL;
	int i = 0, j = 0, k = 0;

	/* Allocate some memory */
	curr_ulib = (tb_test_lib_t*) calloc(1, sizeof(tb_test_lib_t));
	if (NULL == curr_ulib) {
		TB_LOG(TB_LOG_ERR, "calloc failed: (%s)", strerror(errno));
		status = TB_ERROR;
		goto exit;
	}

	/* Get the handle of the library if the library is already loaded */
	curr_ulib->lib_handle = dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
	if (curr_ulib->lib_handle == NULL) {
		TB_PRINTF("Library not loaded or wrong: %s\n", lib);
		TB_LOG(TB_LOG_ERR, "Library not loaded or wrong: %s\nFailed with error: %s\n", lib, dlerror());
		status = TB_ERROR;
		free(curr_ulib);
		goto exit;
	 }
	else  {

		/* Clear any existing error */
		dlerror();

		/* Starting to unload groups, suits and cases */
		TB_PRINTF("Unloading library: %s\n", lib);

		imported_ulib_group = (testbox_testgroups_t*) dlsym(curr_ulib->lib_handle, "lib_group");
		if ((error = dlerror())) {
			TB_LOG(TB_LOG_ERR, "Couldn't find symbol lib_group in lib: %s\nFailed with error: %s\n", curr_ulib->lib_name, error);
			status = TB_ERROR;
			dlclose(curr_ulib->lib_handle);
			free(curr_ulib);
			goto exit;
		}
		else  {
			for (k = 0; k < imported_ulib_group->number_of_groups; k++) {
					testbox_testgroup_t *ugroup = &imported_ulib_group->groups[k];

					TB_PRINTF("  Unloading group: '%s'\n", ugroup->group_name);

					for (i = 0; i < ugroup->number_of_suites; i++) {
						testbox_testsuite_t *usuite = &ugroup->suites[i];

						TB_PRINTF("    Unloading suite: '%s'\n", usuite->suite_name);

						for (j = 0; j < usuite->number_of_tests; j++) {
							testbox_testcase_t *utc = &usuite->testcases[j];

							TB_PRINTF("      Unloading case: '%s'\n", utc->test_name);

							tb_case_deletenode_by_name(utc->test_name);
						}
					}
					tb_group_deletenode_by_name(ugroup->group_name);
			}

			/* Lock libs */
			pthread_mutex_lock(&lock);
			/* Remove the library node also from the libs linked list */
			tempnode = libs;

			/* When node to be deleted is the head node */
			if(libs->lib_handle == curr_ulib->lib_handle)
			{
				if(libs->next_lib == NULL)
			    {
					libs = NULL;
					/* Unlock libs */
					pthread_mutex_unlock(&lock);

					TB_PRINTF("This was the last library!\n");

					/* Close the library */
					dlclose(curr_ulib->lib_handle); /* Close the library, as it was opened in this function again */
					dlclose(curr_ulib->lib_handle); /* Close the library that was originally wanted to be closed */

					/* Free memory allocated in this function */
				    free(curr_ulib);

				    /* Free memory allocated in load_testcase_from_lib function*/
				    free(tempnode);

					status = TB_OK;

			        goto exit;
			     }

				/* Move the libs pointer in linked list to second node */
			    libs = libs->next_lib;
				/* Unlock libs */
				pthread_mutex_unlock(&lock);
			    /* Close the library */
				dlclose(curr_ulib->lib_handle); /* Close the library, as it was opened in this function again */
				dlclose(curr_ulib->lib_handle); /* Close the library that was originally wanted to be closed */

				/* Free memory allocated in this function */
			    free(curr_ulib);

			    /* Free memory allocated in load_testcase_from_lib function */
			    free(tempnode);

		        status = TB_OK;
		        goto exit;
			}

			/* When not first node, follow the normal deletion process */
			/* Find the previous node */
			while(tempnode->next_lib != NULL && tempnode->next_lib->lib_handle != curr_ulib->lib_handle)
				tempnode = tempnode->next_lib;

			/* Check if node really exists in linked list */
			if(tempnode->next_lib == NULL){

				/* Unlock libs */
				pthread_mutex_unlock(&lock);
				TB_PRINTF("This libary is not present in the libs list!\n");

				/* Close the library */
				dlclose(curr_ulib->lib_handle); /* Close the library, as it was opened in this function again */
				dlclose(curr_ulib->lib_handle); /* Close the library that was originally wanted to be closed */

				/* Free memory allocated in load_testcase_from_lib function */
			    free(curr_ulib);

			    status = TB_ERROR;

			    goto exit;
			}

		    /* Get a pointer of loaded library, so the memory area can be freed after removing it from linked list */
			tempnode2 = tempnode->next_lib;

			/* Remove node from linked list */
			tempnode->next_lib = tempnode2->next_lib;

			/* Unlock libs */
			pthread_mutex_unlock(&lock);

			/* Close the library */
			dlclose(curr_ulib->lib_handle); /* Close the library, as it was opened in this function again */
			dlclose(curr_ulib->lib_handle); /* Close the library that was originally wanted to be closed */

			/* Free memory allocated in this function */
			free(curr_ulib);

			/* Free memory allocated in load_testcase_from_lib function */
			free(tempnode2);

			status = TB_OK;
		}

	 }

exit: if (status){
			return TB_ERROR;
		}
		else{
			return TB_OK;
		}
}

/*
 * List all libraries currently loaded
 */
int list_libs() {
	tb_test_lib_t *curr_llib = NULL;
	/* Lock libs */
	pthread_mutex_lock(&lock);
	/* Point to last loaded library */
	curr_llib = libs;
	TB_PRINTF("Currently loaded libraries:\n");
	/* Print the library names */
	while (curr_llib != NULL){
		TB_PRINTF("%s\n", curr_llib->lib_name);
		curr_llib = curr_llib->next_lib;
	}
	/* Unlock libs */
	pthread_mutex_unlock(&lock);
	return TB_OK;
}

/*---------------------------------------------------------------------------*/
static int add_tc_from_file(const char *filename, tb_testsuite_nod_t *suite_info, FILE *file) {
	char line[TB_STR_LEN];
	char orig_path[TB_STR_LEN];
	char *tc_name = NULL;
	char *exec_name = NULL;
	char *args[32] = { 0 };
	tb_testcase_nod_t *tc = NULL;
	tb_testcase_nod_t testcase_info;
	tb_testtype_t t;
	int i, nr_of_arg = 0;

	while (fgets(line, sizeof(line), file) != NULL) {
		if (strncmp("#", line, 1) != 0) {
			if (strlen(line) > 1) {
				memset(&testcase_info, 0, sizeof(testcase_info));
				strip_newline(line, strlen(line));

				if ((tc_name = strstr(line, "DESCR(")) != NULL) {
					tc_name += strlen("DESCR("); *strrchr(tc_name, ')') = '\0';

					strncpy(suite_info->descr, tc_name, TB_STR_LEN-1);
				}
				else if ((tc_name = strstr(line, "TC(")) != NULL) {
					tc_name += strlen("TC("); *strrchr(tc_name, ')') = '\0';

					TB_LOG(TB_LOG_DBG, "Found test case '%s' in %s\n", tc_name, filename);

					// read any/possible variables
					i = 0;
					args[i++] = strtok(tc_name, " ");
					while ((args[i] = strtok(NULL, " ")) != NULL) {
						i++;
					}
					nr_of_arg = i;

					// fetch already registered test function and make a local copy
					tc = tb_case_lookupnode(args[0]);
					if (NULL != tc) {
						strncpy(testcase_info.name, tc->name, TB_STR_LEN - 1);
						strncpy(testcase_info.description, tc->description, TB_STR_LEN - 1);
						testcase_info.is_executable = TB_FALSE;
						testcase_info.valid_board = tc->valid_board;
						testcase_info.is_extended = tc->is_extended;
						testcase_info.function = tc->function;
						testcase_info.argc = nr_of_arg;
						for (i = 0; i < nr_of_arg; i++) {
							strncpy(testcase_info.args[i], args[i], TB_STR_LEN - 1);
						}
						// add test function with arguments to suite
						tc = tb_suite_addtest(suite_info, testcase_info);
						if (tc == NULL) {
							TB_LOG(TB_LOG_ERR, "%s was NOT added to %s", testcase_info.name, suite_info->name);
						}
					} else {
						TB_LOG(TB_LOG_ERR, "Could not find test function [%s], check spelling in file",
								args[0]);
					}
				}
				else if ((tc_name = strstr(line, "EXEC(")) != NULL) {
					tc_name += strlen("EXEC("); *strrchr(tc_name, ')') = '\0';

					TB_LOG(TB_LOG_DBG, "Found exec '%s' in %s\n", tc_name, filename);

					/* check if the path contains env var */
					strncpy(orig_path, tc_name, TB_STR_LEN - 1);
					if((tc_name = create_path_from_env(orig_path)) == NULL) {
						TB_LOG(TB_LOG_DBG, "path does not use env variable '%s'\n", orig_path);
						tc_name = orig_path; //Default if no env varibles are used
					}
					TB_LOG(TB_LOG_DBG, "path to exec: '%s'\n", tc_name);

					/* extract exec name */
					exec_name = strrchr(tc_name, '/');
					exec_name = strtok(exec_name, "/");

					// read any/possible variables
					i = 0;
					args[i++] = strtok(tc_name, " ");
					while ((args[i] = strtok(NULL, " ")) != NULL) {
						i++;
					}
					nr_of_arg = i;

					// check if executable file exist and can be run
					t = tb_exec_check_executable(args[0]);
					if (t == TB_EXEC) {
						strncpy(testcase_info.name, exec_name, TB_STR_LEN - 1);
						strncpy(testcase_info.description, "Executable file", TB_STR_LEN - 1);
						testcase_info.is_executable = TB_TRUE;
						testcase_info.valid_board = TB_BOARD_ALL;
						testcase_info.is_extended = TB_NOT_EXTENDED;
						testcase_info.argc = nr_of_arg;
						for (i = 0; i < nr_of_arg; i++) {
							strncpy(testcase_info.args[i], args[i], TB_STR_LEN - 1);
						}

						// add exec with arguments to suite
						tc = tb_suite_addtest(suite_info, testcase_info);
						if (tc == NULL) {
							TB_LOG(TB_LOG_ERR, "%s was NOT added to %s", testcase_info.name, suite_info->name);
						}
					} else {
						TB_LOG(TB_LOG_ERR, "Could not find executable file [%s], check spelling in file",
								args[0]);
					}
				}
				else if ((tc_name = strstr(line, "END_SUITE")) != NULL) {
					TB_LOG(TB_LOG_DBG, "Found [%s] in %s\n", tc_name, filename);
					return TB_OK;
				}
			}
		}
	}

	TB_LOG(TB_LOG_ERR, "Format error in %s??\n", filename);
	return TB_ERROR;
}

static int add_suite_from_file(const char *filename, tb_testgroup_nod_t *group_info, FILE *file) {
	char line[TB_STR_LEN];
	char *suite_name;
	tb_testsuite_nod_t *suite_info = NULL;

	while (fgets(line, sizeof(line), file) != NULL) {
		if (strncmp("#", line, 1) != 0) {
			if (strlen(line) > 1) {
				strip_newline(line, strlen(line));

				if ((suite_name = strstr(line, "SUITE(")) != NULL) {
					suite_name += strlen("SUITE("); *strrchr(suite_name, ')') = '\0';

					TB_LOG(TB_LOG_DBG, "Found suite '%s' in %s\n", suite_name, filename);

					if (strlen(suite_name) > 0) {
						suite_info = tb_suite_lookupnode(suite_name);
						if (suite_info != NULL) {
							TB_LOG(TB_LOG_WARN, "Suite %s already exists!", suite_name);
						}
						suite_info = tb_suite_addnode(suite_name);
					}

					suite_info = tb_group_addsuite(group_info, suite_info);
					if (NULL == suite_info) {
						TB_LOG(TB_LOG_ERR, "%s was NOT added to %s", suite_name, group_info->name);
						goto exit;
					}

					add_tc_from_file(filename, suite_info, file);
				}
				else if ((suite_name = strstr(line, "END_GROUP")) != NULL) {
					TB_LOG(TB_LOG_DBG, "Found [%s] in %s\n", suite_name, filename);
					return TB_OK;
				}
			}
		}
	}

	exit:
	TB_LOG(TB_LOG_ERR, "Format error in %s??\n", filename);
	return TB_ERROR;
}

/*Add group from file*/
int add_group_from_file(const char *filename) {
	int status = TB_OK;
	char line[TB_STR_LEN];
	FILE *file = fopen(filename, "r");
	tb_testgroup_nod_t *group_info = NULL;
	char *group_name;

	if (file != NULL) {
		while (fgets(line, sizeof(line), file) != NULL) {
			if (strncmp("#", line, 1) != 0) {
				if (strlen(line) > 1) {
					strip_newline(line, strlen(line));

					if ((group_name = strstr(line, "GROUP(")) != NULL) {
						group_name += strlen("GROUP("); *strrchr(group_name, ')') = '\0';

						TB_LOG(TB_LOG_DBG, "Found group '%s' in %s\n", group_name, filename);

						if (strlen(group_name) > 0) {
							group_info = tb_group_lookupnode(group_name);
							if (group_info != NULL) {
								TB_LOG(TB_LOG_WARN, "Group %s already exists!", group_name);
							}
							group_info = tb_group_addnode(group_name);

							add_suite_from_file(filename, group_info, file);
						}
					}
				}
			}
		}
	}
	else {
		TB_LOG(TB_LOG_ERR, "Couldn't open [%s]\n", filename);
		status = TB_ERROR;
		goto exit;
	}
	fclose(file);

	exit:
	return status;
}

