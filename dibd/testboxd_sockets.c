#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset()... */
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "testboxd_sockets.h"
#include "testboxd_utilities.h"
#include "testboxd_args.h"
#include "testboxd_types.h"
#include "libtestbox_log.h"
#include "libtestbox_socket.h"

/* #define DEBUG */

typedef struct {
	int sock_id;
	int *sock_stat;
} thread_data;

/**
 * TMP thing - to find out and log if we have more
 * cases of unexpected signals received
 */
extern pid_t sigusr_pid;
extern int sigusr_sig;
extern pid_t sigpipe_sig;
extern int sigpipe_pid;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait = PTHREAD_COND_INITIALIZER;
static int connection_count = 0;

static void log_rx_signals(void)
{
	static int sigusr_sig_prev = 0;
	static int sigpipe_sig_prev = 0;

	if (sigusr_sig_prev != sigusr_sig) {
		sigusr_sig_prev = sigusr_sig;
		TB_LOG(TB_LOG_ERR, "Received unexpected signals (%d) : SIGUSR,"
				" sender %u", sigusr_sig_prev, sigusr_pid);
	}
	if (sigpipe_sig_prev != sigpipe_sig) {
		sigpipe_sig_prev = sigpipe_sig;
		TB_LOG(TB_LOG_ERR,
				"Received unexpected signals (%d) : SIGPIPE, "
				"sender %u", sigpipe_sig_prev, sigpipe_pid);
	}
}

void* tb_socket_th(void *ptr)
{
	int newsockfd;
	int n = 0;
	int pos = 0;
	int len = 0;
	int argc = 0;
	char **argv;
	tb_socket_msg_t message;

	thread_data *th_info = ptr;
	newsockfd = th_info->sock_id;

	tb_socket_client_add(newsockfd);

	memset(&message, 0, sizeof(tb_socket_msg_t));

	n = read(newsockfd, &message, sizeof(message));
	if (n < 0) {
		TB_LOG(TB_LOG_ERR, "ERROR reading from socket\n");
		goto drain;
	}

	alloc_2d_array(&argv, TB_MAX_ARGS, TB_STR_LEN);

	while (message.pBuf[pos] != 0) {
		len = strlen(&message.pBuf[pos]) + 1;
		memcpy(argv[argc], &message.pBuf[pos], len);
		pos += len;
		argc++;
		TB_LOG(TB_LOG_DBG,
			"testboxd len: %d, pos: %d, argv: %s, nof args: %d\n",
			len, pos, argv[argc - 1], argc);
	}

	th_info->sock_stat = (intptr_t) cmd_parse(argc, argv, newsockfd, message.pBuf);

	message.hdr.type = tb_sock_msg_done;
	if (strlen(message.pBuf) == 0) {
		snprintf(message.pBuf, TB_SOCK_MSG_MAX_LEN, "%s :: %s",
				"Empty response from TestBox", "TESTBOX_FAIL");
	}

	n = tb_socket_send_data(message.pBuf, strlen(message.pBuf) + 1,
			tb_sock_msg_done);
	if (n < 0) {
		TB_LOG(TB_LOG_ERR, "ERROR writing to socket %d. %s.", newsockfd,
				strerror(errno));
	}

	free_2d_array(argv, TB_MAX_ARGS);

	pthread_mutex_lock(&lock);
	connection_count--;
	pthread_cond_signal(&wait);
	pthread_mutex_unlock(&lock);

drain:
	tb_socket_client_remove(pthread_self());
	close(newsockfd);
	if (th_info)
		free(th_info);
	return NULL;
}

int tb_socket_handling(void)
{
	int sockfd = 0;
	int newsockfd = 0;
	int status = TB_ERROR;
	pthread_t th;
	thread_data *th_data = NULL;

	sockfd = tb_socket_setup();

	while (1) {
		log_rx_signals();

		newsockfd = tb_socket_accept(sockfd);

		pthread_mutex_lock(&lock);

		/*
		 * The connection limit is added because of the system
		 * limitation of having too many simultaneous socket file
		 * descriptors opened.
		 */
		while (connection_count > 100) {
			pthread_cond_wait(&wait, &lock);
#ifdef DEBUG
			TB_LOG(TB_LOG_DBG, "More than 100 connections at the "
					"same time. Waiting in queue!\n");
#endif
		}
		connection_count++;
#ifdef DEBUG
		TB_LOG(TB_LOG_DBG, "Testboxd connection count: %d\n",
				connection_count);
#endif
		pthread_mutex_unlock(&lock);

		th_data = calloc(1, sizeof(*th_data));
		if (!th_data) {
			TB_LOG(TB_LOG_ERR, "ERROR: memory alloc failed\n");
			status = -ENOMEM;
			goto exit;
		}

		th_data->sock_id = newsockfd;
		th_data->sock_stat = &status;

		if (pthread_create(&th, NULL, &tb_socket_th, th_data) != 0) {
			TB_LOG(TB_LOG_ERR, "Socket thread creation failed\n");
			free(th_data);
			status = TB_ERROR;
			goto exit;
		}
		pthread_detach(th);
	}

exit:
	close(sockfd);
	return status;
}
