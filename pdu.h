// Copyright (c) 2020 Microchip Technology Inc. and its subsidiaries.
// SPDX-License-Identifier: (GPL-2.0)

#ifndef PDU_H
#define PDU_H

#include <linux/mrp_bridge.h>

struct br_mrp_tlv_hdr {
	__u8 type;
	__u8 length;
};

struct br_mrp_sub_tlv_hdr {
	__u8 type;
	__u8 length;
};

struct br_mrp_end_hdr {
	struct br_mrp_tlv_hdr hdr;
};

struct br_mrp_common_hdr {
	__be16 seq_id;
	__u8 domain[MRP_DOMAIN_UUID_LENGTH];
};

struct br_mrp_ring_test_hdr {
	__be16 prio;
	__u8 sa[ETH_ALEN];
	__be16 port_role;
	__be16 state;
	__be16 transitions;
	__be32 timestamp;
} __attribute__((__packed__));

struct br_mrp_ring_topo_hdr {
	__be16 prio;
	__u8 sa[ETH_ALEN];
	__be16 interval;
};

struct br_mrp_ring_link_hdr {
	__u8 sa[ETH_ALEN];
	__be16 port_role;
	__be16 interval;
	__be16 blocked;
};

struct br_mrp_sub_opt_hdr {
	__u8 type;
	__u8 manufacture_data[MRP_MANUFACTURE_DATA_LENGTH];
};

struct br_mrp_test_mgr_nack_hdr {
	__be16 prio;
	__u8 sa[ETH_ALEN];
	__be16 other_prio;
	__u8 other_sa[ETH_ALEN];
};

struct br_mrp_test_prop_hdr {
	__be16 prio;
	__u8 sa[ETH_ALEN];
	__be16 other_prio;
	__u8 other_sa[ETH_ALEN];
};

struct br_mrp_oui_hdr {
	__u8 oui[MRP_OUI_LENGTH];
};

struct br_mrp_in_test_hdr {
	__be16 id;
	__u8 sa[ETH_ALEN];
	__be16 port_role;
	__be16 state;
	__be16 transitions;
	__be32 timestamp;
} __attribute__((__packed__));

struct br_mrp_in_topo_hdr {
	__u8 sa[ETH_ALEN];
	__be16 id;
	__be16 interval;
};

struct br_mrp_in_link_hdr {
	__u8 sa[ETH_ALEN];
	__be16 port_role;
	__be16 id;
	__be16 interval;
};

struct br_mrp_in_link_status_hdr {
	__u8 sa[ETH_ALEN];
	__be16 port_role;
	__be16 id;
};

#endif
