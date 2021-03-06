/*
 * DPVS is a software load balancer (Virtual Server) based on DPDK.
 *
 * Copyright (C) 2017 iQIYI (www.iqiyi.com).
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __DPVS_SVC_H__
#define __DPVS_SVC_H__

#include <stdint.h>
#include <net/if.h>
#include "match.h"
#include "ipvs/stats.h"
#include "ipvs/dest.h"
#include "inet.h"
#include "../src/ipvs/libtree/libtree.h"

#define DP_VS_SCHEDNAME_MAXLEN      16

#ifdef __DPVS__
#include "list.h"
#include "dpdk.h"
#include "netif.h"
#include "ipvs/ipvs.h"
#include "ipvs/sched.h"

#define RTE_LOGTYPE_SERVICE RTE_LOGTYPE_USER3
#define DP_VS_SVC_F_PERSISTENT      0x0001      /* peristent port */
#define DP_VS_SVC_F_HASHED          0x0002      /* hashed entry */
#define DP_VS_SVC_F_SYNPROXY        0x8000      /* synrpoxy flag */

#define DP_VS_SVC_F_SIP_HASH        0x0100      /* sip hash target */
#define DP_VS_SVC_F_QID_HASH        0x0200      /* quic cid hash target */

#define DP_VS_SVC_F_MATCH           0x0400      /* snat match */

#define DP_VS_SVC_F_SRC_DENY        0X1000      /* src deny flag */
#define DP_VS_SVC_F_DST_DENY        0X2000      /* dst deny flag */

enum {
    SRC_ACCESS_TREE = 0,
    SRC_DENY_TREE,
    DST_ACCESS_TREE,
    DST_DENY_TREE,
    DP_VS_TREE_NUM,
};

/* virtual service */
struct dp_vs_service {
    struct list_head    s_list;     /* node for normal service table */
    struct list_head    f_list;     /* node for fwmark service table */
    struct list_head    m_list;     /* node for match  service table */
    rte_atomic32_t      refcnt;     /* svc is per core, conn will not refer to svc, but dest will.
                                     *  while conn will refer to dest */

    /*
     * to identify a service
     * 1. <af, proto, vip, vport>
     * 2. fwmark (no use now).
     * 3. match.
     */
    int                 af;
    uint8_t             proto;      /* TCP/UDP/... */
    union inet_addr     addr;       /* virtual IP address */
    uint16_t            port;
    uint32_t            fwmark;
    struct dp_vs_match  *match;

    unsigned            flags;
    unsigned            timeout;
    unsigned            conn_timeout;
    unsigned            bps;
    unsigned            limit_proportion;
    uint32_t            netmask;

    struct list_head    dests;      /* real services (dp_vs_dest{}) */
    uint32_t            num_dests;
    long                weight;     /* sum of servers weight */

    struct dp_vs_scheduler  *scheduler;
    void                *sched_data;

    struct dp_vs_stats  stats;

    /* FNAT only */
    struct list_head    laddr_list; /* local address (LIP) pool */
    struct list_head    *laddr_curr;
    uint32_t            num_laddrs;

    /* ACL */
    uint32_t            num_acls;
    struct avltree      tree[DP_VS_TREE_NUM];

    /* ... flags, timer ... */
} __rte_cache_aligned;
#endif

struct dp_vs_service_conf {
    /* virtual service addresses */
    uint16_t            af;
    uint16_t            protocol;
    union inet_addr     addr;       /* virtual ip address */
    uint16_t            port;
    uint32_t            fwmark;     /* firwall mark of service */
    struct dp_vs_match  match;

    /* virtual service options */
    char                *sched_name;
    unsigned            flags;     /* virtual service flags */
    unsigned            timeout;   /* persistent timeout in sec */
    unsigned            conn_timeout;
    uint32_t            netmask;        /* persistent netmask */
    unsigned            bps;
    unsigned            limit_proportion;
};

struct dp_vs_service_entry {
    int                 af;
    uint16_t            proto;
    union inet_addr     addr;
    uint16_t            port;
    uint32_t            fwmark;

    char                sched_name[DP_VS_SCHEDNAME_MAXLEN];
    unsigned            flags;
    unsigned            timeout;
    unsigned            conn_timeout;
    uint32_t            netmask;
    unsigned            bps;
    unsigned            limit_proportion;

    unsigned int        num_dests;
    unsigned int        num_laddrs;
    lcoreid_t           cid;

    struct dp_vs_stats  stats;

    char                srange[256];
    char                drange[256];
    char                iifname[IFNAMSIZ];
    char                oifname[IFNAMSIZ];
};

struct dp_vs_get_services {
    lcoreid_t     cid;
    unsigned int        num_services;
    struct dp_vs_service_entry entrytable[0];
};

struct dp_vs_service_user{
    int               af;
    uint16_t          proto;
    union inet_addr   addr;
    uint16_t          port;
    uint32_t          fwmark;

    char              sched_name[DP_VS_SCHEDNAME_MAXLEN];
    unsigned          flags;
    unsigned          timeout;
    unsigned          conn_timeout;
    uint32_t          netmask;
    unsigned          bps;
    unsigned          limit_proportion;

    char              srange[256];
    char              drange[256];
    char              iifname[IFNAMSIZ];
    char              oifname[IFNAMSIZ];
};

struct dp_vs_getinfo {
    unsigned int version;
    unsigned int size;
    unsigned int num_services;
    unsigned int num_lcores;
};

#ifdef __DPVS__
int dp_vs_service_init(void);
int dp_vs_service_term(void);

struct dp_vs_service *
dp_vs_service_lookup(int af, uint16_t protocol,
                     const union inet_addr *vaddr,
                     uint16_t vport, uint32_t fwmark,
                     const struct rte_mbuf *mbuf,
                     const struct dp_vs_match *match,
                     bool *outwall, lcoreid_t cid);

int dp_vs_match_parse(const char *srange, const char *drange,
                      const char *iifname, const char *oifname,
                      int af, struct dp_vs_match *match);

void dp_vs_bind_svc(struct dp_vs_dest *dest, struct dp_vs_service *svc);

void dp_vs_unbind_svc(struct dp_vs_dest *dest);

void dp_vs_svc_put(struct dp_vs_service *svc);

struct dp_vs_service *dp_vs_lookup_vip(int af, uint16_t protocol,
                                       const union inet_addr *vaddr,
                                       lcoreid_t cid);

unsigned dp_vs_get_conn_timeout(struct dp_vs_conn *conn);

#define MAX_ARG_LEN    (sizeof(struct dp_vs_service_user) +    \
                         sizeof(struct dp_vs_dest_user))
#endif

enum{
    DPVS_SO_SET_FLUSH = 200,
    DPVS_SO_SET_ZERO,
    DPVS_SO_SET_ADD,
    DPVS_SO_SET_EDIT,
    DPVS_SO_SET_DEL,
    DPVS_SO_SET_ADDDEST,
    DPVS_SO_SET_EDITDEST,
    DPVS_SO_SET_DELDEST,
    DPVS_SO_SET_GRATARP,
};

enum{
    DPVS_SO_GET_VERSION = 200,
    DPVS_SO_GET_INFO,
    DPVS_SO_GET_SERVICES,
    DPVS_SO_GET_SERVICE,
    DPVS_SO_GET_DESTS,
};


#define SOCKOPT_SVC_BASE         DPVS_SO_SET_FLUSH
#define SOCKOPT_SVC_SET_CMD_MAX  DPVS_SO_SET_GRATARP
#define SOCKOPT_SVC_GET_CMD_MAX  DPVS_SO_GET_DESTS
#define SOCKOPT_SVC_MAX          299

#endif /* __DPVS_SVC_H__ */
