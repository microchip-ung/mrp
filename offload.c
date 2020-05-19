// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <linux/types.h>
#include <linux/if_bridge.h>
#include <net/if.h>
#include <errno.h>

#include "state_machine.h"
#include "utils.h"
#include "libnetlink.h"

struct rtnl_handle rth = { .fd = -1 };

struct request {
	struct nlmsghdr		n;
	struct ifinfomsg	ifm;
	char			buf[1024];
};

static void mrp_nl_bridge_prepare(struct mrp *mrp, int cmd, struct request *req,
				  struct rtattr **afspec, struct rtattr **afmrp,
				  struct rtattr **af_submrp, int mrp_attr)
{
	req->n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req->n.nlmsg_flags = NLM_F_REQUEST;
	req->n.nlmsg_type = cmd;
	req->ifm.ifi_family = PF_BRIDGE;

	req->ifm.ifi_index = mrp->ifindex;

	*afspec = addattr_nest(&req->n, sizeof(*req), IFLA_AF_SPEC);
	addattr16(&req->n, sizeof(*req), IFLA_BRIDGE_FLAGS, BRIDGE_FLAGS_SELF);

	*afmrp = addattr_nest(&req->n, sizeof(*req),
			      IFLA_BRIDGE_MRP | NLA_F_NESTED);
	*af_submrp = addattr_nest(&req->n, sizeof(*req),
				  mrp_attr | NLA_F_NESTED);
}

static void mrp_nl_port_prepare(struct mrp_port *port, int cmd,
				struct request *req, struct rtattr **afspec,
				struct rtattr **afmrp,
				struct rtattr **af_submrp, int mrp_attr)
{
	req->n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req->n.nlmsg_flags = NLM_F_REQUEST;
	req->n.nlmsg_type = cmd;
	req->ifm.ifi_family = PF_BRIDGE;

	req->ifm.ifi_index = port->ifindex;

	*afspec = addattr_nest(&req->n, sizeof(*req), IFLA_AF_SPEC);
	*afmrp = addattr_nest(&req->n, sizeof(*req),
			      IFLA_BRIDGE_MRP | NLA_F_NESTED);
	*af_submrp = addattr_nest(&req->n, sizeof(*req),
				  mrp_attr | NLA_F_NESTED);
}

static int mrp_nl_terminate(struct request *req, struct rtattr *afspec,
			    struct rtattr *afmrp, struct rtattr *af_submrp)
{
	int err;

	addattr_nest_end(&req->n, af_submrp);
	addattr_nest_end(&req->n, afmrp);
	addattr_nest_end(&req->n, afspec);

	err = rtnl_talk(&rth, &req->n, NULL);
	if (err)
		return err;

	return 0;
}

int mrp_offload_init(void)
{
	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "Cannot open rtnetlink\n");
		return EXIT_FAILURE;
	}
	return 0;
}

void mrp_offload_uninit(void)
{
	rtnl_close(&rth);
}

int mrp_offload_add(struct mrp *mrp, struct mrp_port *p, struct mrp_port *s,
		    uint16_t prio)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	mrp_nl_bridge_prepare(mrp, RTM_SETLINK, &req, &afspec, &afmrp,
			      &af_submrp, IFLA_BRIDGE_MRP_INSTANCE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_RING_ID,
		  mrp->ring_nr);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_P_IFINDEX,
		  p->ifindex);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_S_IFINDEX,
		  s->ifindex);
	addattr16(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_PRIO, prio);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_offload_del(struct mrp *mrp)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	mrp_nl_bridge_prepare(mrp, RTM_DELLINK, &req, &afspec, &afmrp,
			      &af_submrp, IFLA_BRIDGE_MRP_INSTANCE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_RING_ID,
		  mrp->ring_nr);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_P_IFINDEX,
		  mrp->p_port->ifindex);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_INSTANCE_S_IFINDEX,
		  mrp->s_port->ifindex);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_port_offload_set_state(struct mrp_port *p,
			       enum br_mrp_port_state_type state)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	p->state = state;

	mrp_nl_port_prepare(p, RTM_SETLINK, &req, &afspec, &afmrp,
			    &af_submrp, IFLA_BRIDGE_MRP_PORT_STATE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_PORT_STATE_STATE, state);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_port_offload_set_role(struct mrp_port *p,
			      enum br_mrp_port_role_type role)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	p->role = role;

	mrp_nl_port_prepare(p, RTM_SETLINK, &req, &afspec, &afmrp,
			    &af_submrp, IFLA_BRIDGE_MRP_PORT_ROLE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_PORT_ROLE_ROLE, role);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_offload_set_ring_state(struct mrp *mrp,
			       enum br_mrp_ring_state_type state)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	mrp_nl_bridge_prepare(mrp, RTM_SETLINK, &req, &afspec, &afmrp,
			      &af_submrp, IFLA_BRIDGE_MRP_RING_STATE);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_RING_STATE_RING_ID,
		  mrp->ring_nr);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_RING_STATE_STATE,
		  state);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_offload_set_ring_role(struct mrp *mrp, enum br_mrp_ring_role_type role)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	mrp->ring_role = role;

	mrp_nl_bridge_prepare(mrp, RTM_SETLINK, &req, &afspec, &afmrp,
			      &af_submrp, IFLA_BRIDGE_MRP_RING_ROLE);

	if (mrp->mra_support)
		role = BR_MRP_RING_ROLE_MRA;

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_RING_ROLE_RING_ID,
		  mrp->ring_nr);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_RING_ROLE_ROLE,
		  role);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_offload_send_ring_test(struct mrp *mrp, uint32_t interval, uint32_t max,
			       uint32_t period)
{
	struct rtattr *afspec, *afmrp, *af_submrp;
	struct request req = { 0 };

	mrp_nl_bridge_prepare(mrp, RTM_SETLINK, &req, &afspec, &afmrp,
			      &af_submrp, IFLA_BRIDGE_MRP_START_TEST);

	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_START_TEST_RING_ID,
		  mrp->ring_nr);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_START_TEST_INTERVAL,
		  interval);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_START_TEST_MAX_MISS,
		  max);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_START_TEST_PERIOD,
		  period);
	addattr32(&req.n, sizeof(req), IFLA_BRIDGE_MRP_START_TEST_MONITOR,
		  mrp->test_monitor);

	return mrp_nl_terminate(&req, afspec, afmrp, af_submrp);
}

int mrp_offload_flush(struct mrp *mrp)
{
	struct request req = { 0 };
	struct rtattr *protinfo;

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = RTM_SETLINK;
	req.ifm.ifi_family = PF_BRIDGE;

	protinfo = addattr_nest(&req.n, sizeof(req),
				IFLA_PROTINFO | NLA_F_NESTED);
	addattr(&req.n, 1024, IFLA_BRPORT_FLUSH);

	addattr_nest_end(&req.n, protinfo);

	req.ifm.ifi_index = mrp->p_port->ifindex;
	if (rtnl_talk(&rth, &req.n, NULL) < 0)
		return -1;

	req.ifm.ifi_index = mrp->s_port->ifindex;
	if (rtnl_talk(&rth, &req.n, NULL) < 0)
		return -1;

	return 0;
}

