
#ifndef TESTBOXD_ARGS_H_
#define TESTBOXD_ARGS_H_

int cmd_parse(int argc, char **argv, int client_sock_fd, char *resp);
int startup_args(int argc, char *argv[]);

#endif /* TESTBOXD_ARGS_H_ */
