// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <netinet/in.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/mrp_bridge.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ev.h>

#include "list.h"
#include "linux.h"
#include "utils.h"

struct mrp_port {
	struct mrp			*mrp;
	enum br_mrp_port_state_type	state;
	enum br_mrp_port_role_type	role;
	uint32_t			ifindex;
	uint8_t				macaddr[ETH_ALEN];
	bool				loc;
	uint8_t				operstate;
};

struct mrp {
	/* list of mrp instances */
	struct list_head		list;

	/* lock for each MRP instance */
	pthread_mutex_t			lock;

	/* ifindex of the bridge */
	uint32_t			ifindex;
	/* mac address of the bridge */
	uint8_t				macaddr[ETH_ALEN];
	struct mrp_port			*p_port;
	struct mrp_port			*s_port;

	/* mac address of the ring MRM */
	uint32_t			ring_prio;
	uint8_t				ring_mac[ETH_ALEN];

	uint32_t			ring_nr;
	bool				mra_support;
	bool				test_monitor;
	enum br_mrp_ring_role_type	ring_role;
	enum mrp_ring_recovery_type	ring_recv;

	enum mrp_mrm_state_type		mrm_state;
	enum mrp_mrc_state_type		mrc_state;

	bool				add_test;
	bool				no_tc;

	uint16_t			ring_transitions;

	uint16_t			seq_id;
	uint16_t			prio;
	uint8_t				domain[MRP_DOMAIN_UUID_LENGTH];

	ev_timer			clear_fdb_work;

	ev_timer			ring_test_work;
	ev_timer			ring_watcher_work;
	uint32_t			ring_test_conf_short;
	uint32_t			ring_test_conf_interval;
	uint32_t			ring_test_conf_max;
	uint32_t			ring_test_conf_ext_max;
	uint32_t			ring_test_curr;
	uint32_t			ring_test_curr_max;
	uint32_t			ring_test_conf_period;
	uint32_t			ring_test_hw_interval;

	ev_timer			ring_topo_work;
	uint32_t			ring_topo_conf_interval;
	uint32_t			ring_topo_conf_max;
	uint32_t			ring_topo_curr_max;

	ev_timer			ring_link_up_work;
	ev_timer			ring_link_down_work;
	uint32_t			ring_link_conf_interval;
	uint32_t			ring_link_conf_max;
	uint32_t			ring_link_curr_max;

	uint32_t			blocked;
	uint32_t			react_on_link_change;
};

int mrp_recv(unsigned char *buf, int buf_len, struct sockaddr_ll *sl,
	     socklen_t salen);
void mrp_port_link_change(struct mrp_port *p, bool up);
void mrp_destroy(uint32_t ifindex, uint32_t ring_nr, bool offload);
void mrp_mac_change(uint32_t ifindex, unsigned char *mac);
void mrp_port_open(struct mrp_port *p, bool loc);

int mrp_get(int *count, struct mrp_status *status);
int mrp_add(uint32_t br_ifindex, uint32_t ring_nr, uint32_t pport,
	    uint32_t sport, uint32_t ring_role, uint16_t prio,
	    uint8_t ring_recv);
int mrp_del(uint32_t br_ifindex, uint32_t ring_nr);
void mrp_uninit(void);

void mrp_set_mrm_init(struct mrp* mrp);
void mrp_set_mrc_init(struct mrp* mrp);

struct mrp_port *mrp_get_port(uint32_t ifindex);
struct mrp *mrp_find(uint32_t br_ifindex, uint32_t ring_nr);

void mrp_ring_test_req(struct mrp *mrp, uint32_t interval);
void mrp_ring_topo_req(struct mrp *mrp, uint32_t interval);
void mrp_ring_link_req(struct mrp_port *p, bool up, uint32_t interval);

void mrp_set_mrm_state(struct mrp *mrp, enum mrp_mrm_state_type state);
void mrp_set_mrc_state(struct mrp *mrp, enum mrp_mrc_state_type state);

/* mrp_timer.c */
void mrp_timer_init(struct mrp *mrp);
void mrp_timer_stop(struct mrp *mrp);

void mrp_ring_open(struct mrp *mrp);

void mrp_clear_fdb_start(struct mrp *mrp, uint32_t interval);

int mrp_ring_test_start(struct mrp *mrp, uint32_t interval);
void mrp_ring_test_stop(struct mrp *mrp);
void mrp_ring_topo_start(struct mrp *mrp, uint32_t interval);
void mrp_ring_topo_stop(struct mrp *mrp);
void mrp_ring_link_up_start(struct mrp *mrp, uint32_t interval);
void mrp_ring_link_up_stop(struct mrp *mrp);
void mrp_ring_link_down_start(struct mrp *mrp, uint32_t interval);
void mrp_ring_link_down_stop(struct mrp *mrp);

/* mrp_offload.c */
int mrp_offload_add(struct mrp *mrp, struct mrp_port *p, struct mrp_port *s,
		    uint16_t prio);
int mrp_offload_del(struct mrp *mrp);
int mrp_port_offload_set_state(struct mrp_port *p,
			       enum br_mrp_port_state_type state);
int mrp_port_offload_set_role(struct mrp_port *p,
			      enum br_mrp_port_role_type role);

int mrp_offload_set_ring_role(struct mrp *mrp, enum br_mrp_ring_role_type role);
int mrp_offload_set_ring_state(struct mrp *mrp,
			       enum br_mrp_ring_state_type state);
int mrp_offload_send_ring_test(struct mrp *mrp, uint32_t interval, uint32_t max,
			       uint32_t period);
int mrp_offload_flush(struct mrp *mrp);

#endif /* STATE_MACHINE_H */

