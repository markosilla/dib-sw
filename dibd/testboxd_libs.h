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
#ifndef TESTBOXD_LIBS_H_
#define TESTBOXD_LIBS_H_

#include "libtestbox_types.h"

#define PRE_DEFINED_SUITE_CONF	"/bin/predefined_suites.conf"
#define TEST_LIBS_CONF			"/bin/test_libs.conf"
#define GROUP_FROM_FILE_CONF    "/bin/group_from_file.conf"

typedef struct tb_test_lib
{
   char lib_name[TB_MAX_PATH_LEN];	/** The library name */
   void *lib_handle;				/** The library handle */
   struct tb_test_lib *next_lib;
} tb_test_lib_t;

extern char *testbox_cfg_dir;
extern int run_as_daemon;

void load_libraries(void);
void load_predefined_suites(void);
void load_group_from_file(void);
int find_libs_from_conf_file(const char *filename);
int find_groups_from_conf_file(const char *filename);
int load_testcase_from_lib(const char *lib);
int unload_lib(const char *lib);
int list_libs();

int add_group_from_file(const char *filename);

#endif /* TESTBOXD_LIBS_H_ */

