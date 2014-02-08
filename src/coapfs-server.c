#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <limits.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <getopt.h>

#include <log.h>
#include <coap.h>

#define MAX_FILE_READ 256

static coap_context_t *ctx;
static int end;

void hnd_put_index(coap_context_t *ctx, struct coap_resource_t *resource,
		coap_address_t *peer, coap_pdu_t *request, str *token,
		coap_pdu_t *response)
{
	int fd;
	size_t size;
	ssize_t ret;
	unsigned char *data;

	response->hdr->code = COAP_RESPONSE_CODE(204);

	resource->dirty = 1;
	coap_get_data(request, &size, &data);

	fd = open(resource->uri.s, O_WRONLY);
	if (fd == -1)
		response->hdr->code = COAP_RESPONSE_CODE(500);

	ret = write(fd, data, size);
	if (ret == -1)
		response->hdr->code = COAP_RESPONSE_CODE(500);
	log_print(DEBUG, "file %s, write %s, size %d\n",
			resource->uri.s, data, size);

	close(fd);
}

void hnd_get_index(coap_context_t *ctx, struct coap_resource_t *resource,
		coap_address_t *peer, coap_pdu_t *request, str *token,
		coap_pdu_t *response)
{
	int fd;
	ssize_t ret;
	unsigned char buf[MAX_FILE_READ] = {};

	response->hdr->code = COAP_RESPONSE_CODE(205);

	fd = open(resource->uri.s, O_RDONLY);
	if (fd == -1)
		response->hdr->code = COAP_RESPONSE_CODE(500);

	ret = read(fd, buf, MAX_FILE_READ - 1);
	if (ret == -1)
		response->hdr->code = COAP_RESPONSE_CODE(500);

	log_print(DEBUG, "file %s, read %s, len: %d\n", resource->uri.s, buf, ret);
	coap_add_data(response, ret, (unsigned char *)buf);
	
	close(fd);
}

static int dirwalk(char *dir, void (*fcn)(const char *))
{
	char *name;
	struct stat st;
	struct dirent *dp;
	DIR *dfd;
	int ret;

	dfd = opendir(dir);
	if (!dfd) {
		fprintf(stderr, "dirwalk: can't open %s\n", dir);
		return -1;
	}

	while (dp = readdir(dfd)) {
		if (!strcmp(dp->d_name, ".")
				|| !strcmp(dp->d_name, ".."))
			continue;

		name = malloc(PATH_MAX);
		sprintf(name, "%s/%s", dir, dp->d_name);
		
		ret = lstat(name, &st);
		if (ret == -1) {
			log_print(ERROR, "Can't stat %s", name);
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			dirwalk(name, fcn);
		} else {
			(*fcn)(name);
		}
	}

	closedir(dfd);
}

static void fcn(const char *str)
{
	coap_resource_t *r = NULL;

	log_print(DEBUG, "%s %d\n", str, strlen(str));
	r = coap_resource_init(str, strlen(str), 0);
	coap_register_handler(r, COAP_REQUEST_GET, hnd_get_index);
	coap_register_handler(r, COAP_REQUEST_PUT, hnd_put_index);
	coap_add_attr(r, (unsigned char *)"ct", 2, (unsigned char *)"0", 1, 0);
	coap_add_resource(ctx, r);
}

static coap_context_t *get_context(const char *node, const char *port)
{
	coap_context_t *ctx = NULL;
	int s;
	struct addrinfo hints;
	struct addrinfo *result, *rp;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Coap uses UDP */
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

	s = getaddrinfo(node, port, &hints, &result);
	if (s != 0) {
		log_print(ERROR, "getaddrinfo: %s\n", gai_strerror(s));
		return NULL;
	}

	/* iterate through results until success */
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		coap_address_t addr;

		if (rp->ai_addrlen <= sizeof(addr.addr)) {
			coap_address_init(&addr);
			addr.size = rp->ai_addrlen;
			memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);

			ctx = coap_new_context(&addr);
			if (ctx) {
				/* TODO: output address:port for successful binding */
				goto finish;
			}
		}
	}

	log_print(ERROR, "no context available for interface '%s'\n", node);

finish:
	freeaddrinfo(result);
	return ctx;
}

int main(int argc, char *argv[])
{
	fd_set readfds;
	int err, c, dbg = 0;
	coap_tick_t now;
	coap_queue_t *nextpdu;
	char addr_str[NI_MAXHOST] = "127.0.0.1";
	char port_str[NI_MAXSERV] = "5683";
	char dir[PATH_MAX];
	struct timeval tv;
	coap_resource_t *r;

	while ((c = getopt(argc, argv, "a:p:d")) != -1) {
		switch (c) {
		case 'a':
			strncpy(addr_str, optarg, NI_MAXHOST);
			break;
		case 'p':
			strncpy(port_str, optarg, NI_MAXSERV);
			break;
		case 'd':
			dbg = 1;
			break;
		default:
			return EXIT_FAILURE;
		}
	}

	log_init();
	coap_set_log_level(LOG_INFO);
	log_set_log_level(INFO | ERROR);

	if (optind >= argc) {
		log_print(INFO, "Please add a directory.\n"
				"WARNING: Not a directory with a high hierarchy)\n");
		return EXIT_FAILURE;
	}

	strncpy(dir, argv[optind], PATH_MAX);

	if (dbg) {
		coap_set_log_level(LOG_DEBUG);
		log_set_log_level(DEBUG | INFO | ERROR);
	}
	
	log_print(INFO, "%s\n", "Starting CoAP-Server");

	ctx = get_context(addr_str, port_str);
	if (!ctx)
		return EXIT_FAILURE;

	err = dirwalk(dir, fcn);
	if (err < 0)
		return EXIT_FAILURE;

	tv.tv_usec = 0;
	tv.tv_sec = COAP_RESOURCE_CHECK_TIME;

	while (!end) {
		FD_ZERO(&readfds);
		FD_SET(ctx->sockfd, &readfds);

		nextpdu = coap_peek_next(ctx);

		coap_ticks(&now);
		while (nextpdu && nextpdu->t <= now) {
			coap_retransmit(ctx, coap_pop_next(ctx));
			nextpdu = coap_peek_next(ctx);
		}

		if ( nextpdu && nextpdu->t <= now + COAP_RESOURCE_CHECK_TIME ) {
			/* set timeout if there is a pdu to send before our automatic timeout occurs */
			tv.tv_usec = ((nextpdu->t - now) % COAP_TICKS_PER_SECOND) * 1000000 / COAP_TICKS_PER_SECOND;
			tv.tv_sec = (nextpdu->t - now) / COAP_TICKS_PER_SECOND;
		} else {
			tv.tv_usec = 0;
			tv.tv_sec = COAP_RESOURCE_CHECK_TIME;
		}

		err = select(FD_SETSIZE, &readfds, 0, 0, &tv);
		if (err < 0 && errno != EINTR) {
			perror("select");
		}

		if (FD_ISSET(ctx->sockfd, &readfds)) {
			coap_read(ctx);
			coap_dispatch(ctx);
		}
	}

	return 0;
}
