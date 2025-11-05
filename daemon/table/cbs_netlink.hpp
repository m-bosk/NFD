#ifndef CBS_NETLINK_H_
#define CBS_NETLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <bits/sockaddr.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define NLMSG_TAIL(nmsg) \
    ((rtattr *) (((char *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))


/*
 * Taken from iproute2/include/uapi/linux/pkt_sched.h
 * Definitions for CBS qdisc:
 *  - struct for storing CBS parameters
 *  - enum for message types
 */
struct tc_cbs_qopt {
    __u8 offload;
    __u8 _pad[3];
    __s32 hicredit;
    __s32 locredit;
    __s32 idleslope;
    __s32 sendslope;
};

enum {
    TCA_CBS_UNSPEC,
    TCA_CBS_PARMS,
    __TCA_CBS_MAX,
};

#define TCA_CBS_MAX (__TCA_CBS_MAX - 1)
#define TC_H_ROOT	(0xFFFFFFFFU)
#define TC_H_UNSPEC	(0U)


/*
 * RTNETLINK message with traffic control operation header
 */
struct nl_tc_message {
    nlmsghdr nl_header;
    tcmsg tc_header;
    char buffer[4096];
};

int addattr_l(nlmsghdr *n, int maxlen, int type, const void *data, int alen);
rtattr *addattr_nest(nlmsghdr *n, int maxlen, int type);
int addattr_nest_end(nlmsghdr *n, rtattr *nest);
int get_tc_classid(__u32 *h, const char *str);
int send_cbs_message(__u16 type, __u16 flags, const char *dev, const char *handle, const char *parent, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope);
int add_cbs(const char *dev, const char *classid, const char *handle, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope);
int change_cbs(const char *dev, const char *classid, const char *handle, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope);
int replace_cbs(const char *dev, const char *classid, const char *handle, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope);
int link_cbs(const char *dev,  const char *classid, const char *handle, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope);
int delete_cbs(const char *dev, const char *classid, const char *handle, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope);

#endif // CBS_NETLINK_H_