#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <errno.h>

#include <libtestbox_socket.h>

#define LOCALHOST "localhost"

static int socket_type = AF_UNIX;
static char ip_addr[128];

static void error(const char *msg)
{
    perror(msg);
    exit(0);
}

static void ip_addr_set(const char *new_ip)
{
	strncpy(ip_addr, new_ip, 128);
}

static int socket_get(void)
{
	return socket_type;
}

static void socket_set(const char *type)
{
	if (strcmp("local", type) == 0)
	{
		socket_type = AF_UNIX;
	}
	else if (strcmp("inet", type) == 0)
	{
		socket_type = AF_INET;
	}
	else
	{
		fprintf(stderr, "Invalid socket type (%s)\n", type);
		exit(0);
	}
}

static int socket_local(void)
{
	int so, rc;
	struct sockaddr_un sun;

	if ((so = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "ERROR: failed to open TESTBOXD UDS, %d\n",
				-errno);
		rc = -errno;
		goto done;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	memcpy(sun.sun_path, TB_DAEMON_SOCKET, sizeof(TB_DAEMON_SOCKET));

	if (connect(so, (struct sockaddr*) &sun, sizeof(sun)) == -1) {
		fprintf(stderr, "ERROR: failed to connect to TESTBOXD UDS, "
				"(socket:%d).\nMaybe daemon has not started "
				"yet? (ret:%d)\n", so, -errno);
		rc = -errno;
		goto cleanup;
	}
	return so;

cleanup:
	close(so);
done:
	return rc;
}

static int socket_inet(void)
{
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		error("socket:");
	}

	server = gethostbyname(ip_addr);
	if (server == NULL)
	{
		fprintf(stderr,"ERROR, no such host [%s]\n", ip_addr);
		exit(0);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(TB_DAEMON_LISTEN_PORT);

	if (connect(sockfd, (struct sockaddr *) &serv_addr,
			sizeof(serv_addr)) < 0)
	{
		error("connect:");
	}

	return sockfd;
}

static int socket_setup(void)
{
	if (AF_INET == socket_get())
	{
		return socket_inet();
	}
	else if (AF_UNIX == socket_get())
	{
		return socket_local();
	}
	else
	{
		fprintf(stderr, "Invalid socket type (%d)\n", socket_get());
		exit(0);
	}
}

int main(int argc, char *argv[])
{
	int so, n, br, i = 0, rc = 0;
	tb_socket_msg_t message;
	int pos = 0;
	int len = 0;

	ip_addr_set(LOCALHOST);

	memset(&message, 0, sizeof(message));

	for (i = 0; i < argc; i++) {
		if (strcmp("--socket_type", argv[i]) == 0) {
			socket_set(argv[i + 1]);
			i++;
		} else if (strcmp("--ip_addr", argv[i]) == 0) {
			ip_addr_set(argv[i + 1]);
			i++;
		} else {
			len = strlen(argv[i]) + 1;
			memcpy(&message.pBuf[pos], argv[i], len);
			pos += len;
		}
	}
	message.pBuf[pos] = 0;

	message.hdr.type = tb_sock_msg_cmd;

	so = socket_setup();
	if (so < 0) {
		rc = -1;
		goto done;
	}

	n = write(so, &message, sizeof(message));
	if (n == -1) {
		printf("ERROR sending command, %s, %d\n", strerror(errno),
				-errno);
		rc = -errno;
		goto cleanup;
	}

	do {
		memset(&message, 0, sizeof(message));

		for (br = 0;  br < sizeof(message); br += n){

			n = read(so, (char *)&message + br,
					sizeof(message) - br);

			if (n < 0) {
				printf("ERROR: reading response, %s, %d\n",
						strerror(errno), -errno);
				rc = -errno;
				goto cleanup;
			}
		}

		if (message.pBuf)
			printf("%s", message.pBuf);
		fflush(stdout);

	} while (message.hdr.type != tb_sock_msg_done);

	printf("\n");
	fflush(stdout);

cleanup:
	close(so);
done:
	return rc;
}
