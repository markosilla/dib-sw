#include <stdio.h>
#include <string.h>
#include "testboxd_print.h"
#include "libtestbox_types.h"
#include "libtestbox_statusdb.h"
#include "libtestbox_time.h"
#include "libtestbox_casedb.h"
#include "libtestbox_suitedb.h"
#include "libtestbox_groupdb.h"
#include "libtestbox_verb.h"
#include "libtestbox_log.h"

/* Global/Static Definitions
 *****************************************************************************/

/* Private function forward declarations
 *****************************************************************************/
static char * tb_print_actionstring(const tb_test_action_t act);
static void tb_print_timestamp(struct timeval start, struct timeval end, const char *color, int diffOnly);

/* Public functions
 *****************************************************************************/
void tb_print_copyright_info(void) {
	TB_PRINTF(
		"RCS TestBox - The Target Test Framework\n"
		"Copyright (C) Ericsson AB 2013 All rights reserved.\n"
		"-------------------------------------------------------------------------------\n");
}

/*---------------------------------------------------------------------------*/
void tb_print_status(void) {
	int i, j = 0, k;

	TB_PRINTF(BOLD "RCS TestBox -- Status information\n" RESET_COLOR);
	TB_PRINTF(BOLD "-------------------------------------------------------------------------------\n" RESET_COLOR);
	TB_PRINTF(BOLD "%-6s%-25s%-22s%-20s%s\n" RESET_COLOR, "Index", "Group", "Start time", "Duration", "Action");
	TB_PRINTF(BOLD "%2s%-6s%-25s\n" RESET_COLOR, "", "Index", "Suite");
	TB_PRINTF(BOLD "%4s%-6s%-29s%10s%10s%10s%10s\n" RESET_COLOR, "", "Index", "Case", "Runs", "Total", "Pass", "Fail");
	TB_PRINTF(BOLD"-------------------------------------------------------------------------------\n" RESET_COLOR);

	tb_status_lock_mutex();

	for (i = 0; i < tb_status_getsize_group_reg(); i++) {
		tb_status_group_nod_t* sts_gr = tb_status_lookup_group_node_by_idx(i);

		TB_PRINTF(GREEN"%-6d%-25s" RESET_COLOR, i, sts_gr->name);
		tb_print_timestamp(sts_gr->startTimeStamp, sts_gr->endTimeStamp, BLUE, 0);
		TB_PRINTF("%12s\n", tb_print_actionstring(sts_gr->action));

		for (j = 0; j < tb_status_getsize_suite_node(sts_gr); j++) {
			tb_status_suite_nod_t* sts_su = tb_status_lookup_suite_bucket_by_idx(sts_gr, j);

			TB_PRINTF(GREEN"%2s%-6d%-23s\n" RESET_COLOR, "", j, sts_su->name);

			for (k = 0; k < tb_status_getsize_testcase_node(sts_su); k++) {
				tb_status_testcase_nod_t* sts_tc = tb_status_lookup_testcase_bucket_by_idx(sts_su, k);
				TB_PRINTF("%4s%-6d%-29s%10d%10d"GREEN"%10d"RED"%10d"RESET_COLOR"\n", "", k, sts_tc->name, sts_tc->executions,
						sts_gr->loops, sts_tc->pass, sts_tc->fail);
			}
		}

		TB_PRINTF("\n");
	}

	tb_status_release_mutex();

	TB_PRINTF(BOLD"-------------------------------------------------------------------------------\n" RESET_COLOR);
}

/*---------------------------------------------------------------------------*/
static void tb_print_usage(void) {
	TB_PRINTF(
			"USAGE:\n"
					"testbox <utility/function> [options]\n\n"
					"DESCRIPTION:\n"
					"TestBox is the RCS Execution Environment target test framework, used for internal\n"
					"SW unit testing as well as for HW test and verification, production test and repair.\n"
					"\"OPTIONS\" can be added together with --all, <suite name> or <case/function name>\n\n"
					"UTILITIES:\n"
					"  --clear <index> | all      Removes the chosen status database entry. Using 'all' will clear the complete database\n"
					"  --descr <suite/case name>  Print description of test case or suite\n"
					"  --examples                 Prints different command examples in TestBox\n"
					"  --find                     Find a test suite or test case in register\n"
					"  --help                     Print this help\n"
					"  --list [what]              Print suite(s) and test case(s).\n"
					"                             [what] = groups, Groups, suites and case(s).\n"
					"                             [what] = funcs, Functions only.\n"
					"  --loadlib <full path>      Load suites and test cases from library\n"
					"  --unloadlib <full path>    Unload library and its suites and test cases\n"
					"  --listlib                  Lists all currently loaded libraries\n"
					"  --log_clear                Clear log from test case(s)\n"
					"  --log_level                Set log level from test case(s)\n"
					"  --log_read                 Read log from test case(s)\n"
					"  --kill <status index>      Kills the thread running the indexed suite or test case\n"
					"  --status                   Print status of executed/executing suite(s) and test case(s)\n"
					"  --step <suite name>        Steps through case(s) within a suite.\n"
					"  --group_from_file [file]   Add group from file, see example file\n"
					"  --verbose_level [level]    Sets the default verbose level in TestBox\n"
					"                             Level 0: Respons and printf printing is off\n"
					"                                   1: Respons printing is on\n"
					"                                   2: Response and printf printing is on\n"
					"                             If level is omitted TestBox will return current level\n"
					"  --xml_file <open | close>  Open and close the xml result file. This is for automated test only\n"
					"\n"
					"  <group name>               Run suites within <group name>\n"
					"  <suite name>               Run tests within <suite name>\n"
					"  <case name>                Run test case named <case name>\n"
					"  <executable file>          Run program. Full path to file is needed.\n"
					"                             The executable file has to return 0 on success.\n"
					"\n"
					"OPTIONS:\n"
					"  --extend                   Run all basic and extended tests\n"
					"  --loop no_loops            Loop the suite or test case <no_of_loops>\n"
					"  --stoponfail               Stop On Fail\n"
					"  --verbose <level>          Overrides the default verbose level (0, 1 or 2) for this run\n"
					"  --wait                     Wait for response\n"
					"  --xml                      Save result in XML file. Note, XML has to be opened before and closed after\n"
					"                             running all the suites that shall be visible in result file, see --xml_file\n"
					"  --maxtmo                   Max timeout for the test expected in seconds\n"
                                        "                             Default and lowest timeout is 480 seconds \n"
					"  --exe_time                 Run group, suite or test case for a certain time <dd:hh:mm:ss>\n"
                                        "\n"
					"STARTUP OPTIONS:\n"
					"  --log_level                Set log level 0 (emergency) to 7 (debug)\n"
					"  --socket_type              Socket type can be local or inet\n"
					"\n");

}

/*---------------------------------------------------------------------------*/
void tb_print_suites_and_funcs(const char *what) {
	tb_print_copyright_info();

	if (strcmp("suites", what) == 0) {
		tb_group_printgroups(TB_TRUE);
	}
	else if (strcmp("groups", what) == 0) {
		tb_group_printgroups(TB_FALSE);
	}
	else { //all
		tb_case_printfuncs();
	}
}

/*---------------------------------------------------------------------------*/
void print_help(void) {
	tb_print_copyright_info();
	tb_print_usage();
	TB_PRINTF("\n");
}

void tb_print_examples(void) {
	TB_PRINTF(
	"EXAMPLES:\n"
	"  To run all test cases from suite foo:\n"
	"  testbox foo\n"
	"\n"
	"  To run test case bar:\n"
	"  testbox bar\n"
	"\n"
	"  To loop a suite 100 times and also run extended test cases within foo:\n"
	"  testbox foo --extend --loop 100\n"
	"\n"
	"  To loop a suite 100 times, run extended cases, wait for response/prompt and stop on first fail:\n"
	"  testbox foo --extend --loop 100 --wait --stoponfail\n");
}

/* Private functions
 *****************************************************************************/
static char * tb_print_actionstring(const tb_test_action_t act) {
	switch (act) {
	case tb_test_idle:
	{
		return "IDLE";
	}
	case tb_test_running:
	{
		return "RUNNING";
	}
	case tb_test_done:
	{
		return "DONE";
	}
	case tb_test_stopped:
	{
		return "STOPPED";
	}
	default:
	{
		return "UNKNOWN";
	}
	}
}

/*---------------------------------------------------------------------------*/
static void tb_print_timestamp(struct timeval start, struct timeval end, const char *color, int diffOnly) {
	TB_PRINTF(color);

	if (!diffOnly) {
		printTimeStamp(start, 1, 1, 0);
		TB_PRINTF("%-3s", "");
	}

	printTimeStampDiff(start, end);

	TB_PRINTF(RESET_COLOR);
}



