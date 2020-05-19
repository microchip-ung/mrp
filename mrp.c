// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <net/if.h>

#include "utils.h"
#include "linux.h"
#include <linux/mrp_bridge.h>

static int fd = 1;

static int valid_ring_role(char *arg)
{
	if (strcmp(arg, "disabled") == 0 ||
	    strcmp(arg, "mrc") == 0 ||
	    strcmp(arg, "mrm") == 0 ||
	    strcmp(arg, "mra") == 0)
		return 1;
	return 0;
}

static enum br_mrp_ring_role_type ring_role_int(char *arg)
{
	if (strcmp(arg, "disabled") == 0)
		return BR_MRP_RING_ROLE_DISABLED;
	if (strcmp(arg, "mrc") == 0)
		return BR_MRP_RING_ROLE_MRC;
	if (strcmp(arg, "mrm") == 0)
		return BR_MRP_RING_ROLE_MRM;
	if (strcmp(arg, "mra") == 0)
		return BR_MRP_RING_ROLE_MRA;
	return BR_MRP_RING_ROLE_DISABLED;
}

static char *ring_role_str(int ring_role)
{
	switch (ring_role) {
	case BR_MRP_RING_ROLE_DISABLED: return "Disabled";
	case BR_MRP_RING_ROLE_MRC: return "MRC";
	case BR_MRP_RING_ROLE_MRM: return "MRM";
	case BR_MRP_RING_ROLE_MRA: return "MRA";
	default:
		return "Unknown ring role";
	}
}

static char *mrm_state_str(int mrm_state)
{
	switch (mrm_state) {
	case MRP_MRM_STATE_AC_STAT1: return "AC_STAT1";
	case MRP_MRM_STATE_PRM_UP: return "PRM_UP";
	case MRP_MRM_STATE_CHK_RO: return "CHK_RO";
	case MRP_MRM_STATE_CHK_RC: return "CHK_RC";
	default:
		return "Unknown mrm_state";
	}
}

static char *mrc_state_str(int mrc_state)
{
	switch (mrc_state) {
	case MRP_MRC_STATE_AC_STAT1: return "AC_STAT1";
	case MRP_MRC_STATE_DE_IDLE: return "DE_IDLE";
	case MRP_MRC_STATE_PT: return "PT";
	case MRP_MRC_STATE_DE: return "DE";
	case MRP_MRC_STATE_PT_IDLE: return "PT_IDLE";
	default:
		return "Unknwon mrc_state";
	}
}

static int client_init(void)
{
	struct sockaddr_un sa_svr;
	struct sockaddr_un sa;
	char tmpname[64];
	int s;

	if (0 > (s = socket(PF_UNIX, SOCK_DGRAM, 0))) {
		fprintf(stderr, "Couldn't open unix socket: %m");
		return -1;
	}

	set_socket_address(&sa_svr, MRP_SERVER_SOCK_NAME);

	sprintf(tmpname, "MRPCTL_%d", getpid());
	set_socket_address(&sa, tmpname);

	/* We need this bind. The autobind on connect isn't working properly.
	 * The server doesn't get a proper sockaddr in recvmsg if we don't do this.
	 */
	if (0 != bind(s, (struct sockaddr *)&sa, sizeof(sa))) {
	    fprintf(stderr, "Couldn't bind socket: %m");
	    close(s);
	    return -1;
	}

	if (0 != connect(s, (struct sockaddr *)&sa_svr, sizeof(sa_svr))) {
	    fprintf(stderr, "Couldn't connect to server");
	    close(s);
	    return -1;
	}
	fd = s;

	return 0;
}

static void client_cleanup(void)
{
	if (fd >= 0)
	{
		close(fd);
		fd = -1;
	}
}

static int client_send_message(int cmd, void *inbuf, int lin, void *outbuf,
			       int lout, int *res)
{
	struct ctl_msg_hdr mhdr;
	struct iovec iov[2];
	int timeout = 5000; /* 5 s */
	struct msghdr msg;
	struct pollfd pfd;
	int r;
	int l;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	mhdr.cmd = cmd;
	mhdr.lin = lin;
	mhdr.lout = lout;
	iov[0].iov_base = &mhdr;
	iov[0].iov_len = sizeof(mhdr);
	iov[1].iov_base = (void *)inbuf;
	iov[1].iov_len = lin;

	l = sendmsg(fd, &msg, 0);
	if(0 > l) {
		fprintf(stderr, "Error sending message to server: %m");
		return -1;
	}

	if(l != sizeof(mhdr) + lin) {
		fprintf(stderr, "Error sending message to server: Partial write");
		return -1;
	}

	iov[1].iov_base = outbuf;
	iov[1].iov_len = lout;

	pfd.fd = fd;
	pfd.events = POLLIN;
	do {
		if (0 == (r = poll(&pfd, 1, timeout)))
		{
			fprintf(stderr, "Error getting message from server: Timeout");
			return -1;
		}
		if (0 > r)
		{
			fprintf(stderr, "Error getting message from server: poll error: %m");
			return -1;
		}
	} while(0 == (pfd.revents & (POLLERR | POLLHUP | POLLNVAL | POLLIN)));

	l = recvmsg(fd, &msg, 0);
	if (0 > l) {
		fprintf(stderr, "Error getting message from server: %m");
		return -1;
	}

	if ((sizeof(mhdr) > l) || (l != sizeof(mhdr) + mhdr.lout)
	   || (mhdr.cmd != cmd)) {
		fprintf(stderr, "Error getting message from server: Bad format");
		return -1;
	}

	if (mhdr.lout != lout) {
		fprintf(stderr, "Error, unexpected result length %d, expected %d\n",
		      mhdr.lout, lout);
		return -1;
	}

	if (res)
		*res = mhdr.res;

	return 0;
}

static int cmd_addmrp(int argc, char *const *argv)
{
	int br, pport, sport, ring_nr, ring_role;
	uint16_t prio;

	/* skip the command */
	argv++;
	argc -= 2;

	br = if_nametoindex(argv[0]);
	if (!br)
		return -1;

	ring_nr = atoi(argv[1]);
	if (!ring_nr)
		return -1;

	pport = if_nametoindex(argv[2]);
	if (!pport)
		return -1;

	sport = if_nametoindex(argv[3]);
	if (!sport)
		return -1;

	if (!valid_ring_role(argv[4]))
		return -1;

	ring_role = ring_role_int(argv[4]);
	if (!ring_role)
		return -1;

	if (argc >= 5)
		prio = atoi(argv[5]);
	else
		prio = MRP_DEFAULT_PRIO;

	return CTL_addmrp(br, ring_nr, pport, sport, ring_role, prio);
}

static int cmd_delmrp(int argc, char *const *argv)
{
	int br, ring_nr;

	/* skip the command */
	argv++;

	br = if_nametoindex(argv[0]);
	if (!br)
		return -1;

	ring_nr = atoi(argv[1]);
	if (!ring_nr)
		return -1;

	return CTL_delmrp(br, ring_nr);
}

static int cmd_getmrp(int argc, char *const *argv)
{
	struct mrp_status status[MAX_MRP_INSTANCES];
	char ifname[IF_NAMESIZE];
	int count = 0;
	int i;

	memset(ifname, 0, IF_NAMESIZE);

	if (CTL_getmrp(&count, status))
		return -1;

	for (i = 0; i < count; ++i) {
		printf("bridge: %s ", if_indextoname(status[i].br, ifname));
		printf("ring_nr: %d ", status[i].ring_nr);
		printf("pport: %s ", if_indextoname(status[i].pport, ifname));
		printf("sport: %s ", if_indextoname(status[i].sport, ifname));
		printf("mra_support: %d ", status[i].mra_support);
		printf("ring_role: %s ", ring_role_str(status[i].ring_role));
		printf("prio: %d ", status[i].prio);
		if (status[i].ring_role == BR_MRP_RING_ROLE_MRM)
			printf("ring_state: %s \n", mrm_state_str(status[i].ring_state));
		if (status[i].ring_role == BR_MRP_RING_ROLE_MRC)
			printf("ring_state: %s \n", mrc_state_str(status[i].ring_state));
	}

	return 0;
}

struct command
{
	int nargs;
	int optargs;
	const char *name;
	int (*func) (int argc, char *const *argv);
	const char *format;
	const char *help;
};

static const struct command commands[] =
{
	/* Add/delete bridges */
	{5, 1, "addmrp", cmd_addmrp,
	 "<bridge> <ring_nr> <pport> <sport> <role> <prio>", "Create MRP instance"},
	{2, 0, "delmrp", cmd_delmrp,
	 "<bridge> <ring_nr>", "Create MRP instance"},
	{0, 0, "getmrp", cmd_getmrp, "", "Show MRP instances"},
};

static void command_helpall(void)
{
	int i;

	for (i = 0; i < COUNT_OF(commands); ++i) {
		if(strcmp("setportdonttxmt", commands[i].name))
			printf("-%s:\n   %-16s %s\n", commands[i].help,
			       commands[i].name, commands[i].format);
	}
}

static void help(void)
{
	printf("Usage: mrp [options] [commands]\n");
	printf("options:\n");
	printf("  -h | --help              Show this help text\n");
	printf("commands:\n");
	command_helpall();
}

static const struct command *command_lookup(const char *cmd)
{
	int i;

	for(i = 0; i < COUNT_OF(commands); ++i) {
		if(!strcmp(cmd, commands[i].name))
			return &commands[i];
	}

	return NULL;
}

static const struct command *command_lookup_and_validate(int argc,
							 char *const *argv,
							 int line_num)
{
	const struct command *cmd;

	cmd = command_lookup(argv[0]);
	if (!cmd) {
		if (line_num > 0)
			fprintf(stderr, "Error on line %d:\n", line_num);
		fprintf(stderr, "Unknown command [%s]\n", argv[0]);
		if (line_num == 0) {
			help();
			return NULL;
		}
	}

	if (argc < cmd->nargs + 1 || argc > cmd->nargs + cmd->optargs + 1)
	{
		if (line_num > 0)
			fprintf(stderr, "Error on line %d:\n", line_num);
		fprintf(stderr, "Incorrect number of arguments for command '%s'\n",
			cmd->name);
		fprintf(stderr, "Usage: mrp %s %s\n  %s\n",
			cmd->name, cmd->format, cmd->help);
		return NULL;
	}

	return cmd;
}

int main (int argc, char *const *argv)
{
	const struct command *cmd;
	int f;
	int ret;

	static const struct option options[] =
	{
		{.name = "help",	.val = 'h'},
		{0}
	};

	while (EOF != (f = getopt_long(argc, argv, "h", options, NULL))) {
		switch (f) {
			case 'h':
			help();
			return 0;
		}
	}

	if (client_init()) {
		fprintf(stderr, "can't setup control connection\n");
		return 1;
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		help();
		return 1;
	}

	cmd = command_lookup_and_validate(argc, argv, 0);
	if (!cmd)
		return 1;

	ret = cmd->func(argc, argv);
	client_cleanup();
	return ret;
}

CLIENT_SIDE_FUNCTION(addmrp);
CLIENT_SIDE_FUNCTION(delmrp);
CLIENT_SIDE_FUNCTION(getmrp);
