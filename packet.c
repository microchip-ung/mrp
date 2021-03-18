// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#include <ev.h>
#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <linux/filter.h>
#include <asm/byteorder.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <errno.h>

#include "state_machine.h"
#include "utils.h"
#include "print.h"

static ev_io packet_watcher;
static int fd;

void packet_send(int ifindex, const struct iovec *iov, int iov_count, int len)
{
	int l;

	struct sockaddr_ll sl =
	{
		.sll_family = AF_PACKET,
		.sll_protocol = __constant_cpu_to_be16(ETH_P_MRP),
		.sll_ifindex = ifindex,
		.sll_halen = ETH_ALEN,
	};

	/* First ETH_ALEN in iov[0] contains the DMAC */
	memcpy(&sl.sll_addr, iov[0].iov_base, ETH_ALEN);

	struct msghdr msg =
	{
		.msg_name = &sl,
		.msg_namelen = sizeof(sl),
		.msg_iov = (struct iovec *)iov,
		.msg_iovlen = iov_count,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};

	l = sendmsg(fd, &msg, 0);

	if (l < 0) {
		if(errno != EWOULDBLOCK)
			pr_err("send failed: %d", errno);
	} else {
		if (l != len)
			pr_err("short write in sendto: %d instead of %d",
			       l, len);
	}
}

static void packet_rcv(EV_P_ ev_io *w, int revents)
{
	int cc;
	unsigned char buf[2048];
	struct sockaddr_ll sl;
	socklen_t salen = sizeof sl;
	unsigned char mac[ETH_ALEN];

	cc = recvfrom(fd, &buf, sizeof(buf), 0, (struct sockaddr *) &sl, &salen);
	if (cc <= 0) {
		pr_err("recvfrom failed: %d", errno);
		return;
	}

	if_get_mac(sl.sll_ifindex, mac);

	if (memcmp(&buf[ETH_ALEN], mac, ETH_ALEN) == 0)
		return;

	mrp_recv(buf, cc, &sl, salen);
}

static struct sock_filter mrp_filter[] = {
	{ 0x28, 0, 0, 0x0000000c },
	{ 0x15, 0, 1, 0x000088e3 },
	{ 0x6, 0, 0, 0xffffffff },
	{ 0x6, 0, 0, 0x00000000 },
};

/*
 * Open up a raw packet socket to catch all MRP packets
 */
int packet_socket_init(void)
{
	int optval = 7;
	int ignore_out = 1;

	struct sock_fprog prog =
	{
		.len = sizeof(mrp_filter) / sizeof(mrp_filter[0]),
		.filter = mrp_filter,
	};
	int s;

	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(s < 0) {
		pr_err("socket failed: %d", errno);
		return -1;
	}

	if (setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER, &prog, sizeof(prog)) < 0) {
		pr_err("setsockopt packet filter failed: %d", errno);
	} else if (setsockopt(s, SOL_PACKET, PACKET_IGNORE_OUTGOING, &ignore_out, sizeof(ignore_out)) < 0) {
		pr_err("setsockopt packet ignore outgoing: %d", errno);
	} else if (setsockopt(s, SOL_SOCKET, SO_PRIORITY, &optval, 4)) {
		pr_err("setsockopt priority failed: %d", errno);
	} else if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		pr_err("fcntl set nonblock failed: %d", errno);
	} else {
		fd = s;
		ev_io_init(&packet_watcher, packet_rcv, fd, EV_READ);
		ev_io_start(EV_DEFAULT, &packet_watcher);

		return 0;
	}

	close(s);
	return -1;
}

void packet_socket_cleanup(void)
{
	ev_io_stop(EV_DEFAULT, &packet_watcher);
	close(fd);
}
