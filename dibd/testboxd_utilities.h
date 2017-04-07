#ifndef TESTBOX_UTILITIES_H_
#define TESTBOX_UTILITIES_H_

/**
 *
 */
void alloc_2d_array(char*** memory, int rows, int cols);


/**
 *
 */
void free_2d_array(char** ptr, int rows);


/**
 *
 */
char* replace_string(char* source_str, char* search_str, char* replace_str);

/**
 *
 */
int time_str_to_sec(char* time_str, int* sec);

#endif /* TESTBOX_UTILITIES_H_ */
