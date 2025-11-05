/*
 * Potential optimizations:
 *   - cache Netlink message and only change CBS parameters for next change
 */

#include "cbs_netlink.hpp"

/*
 * Taken from iproute2/lib/libnetlink.c
 * Adds attribute (struct rtattr) to buffer of netlink message.
 */
int addattr_l(nlmsghdr *n, int maxlen, int type, const void *data, int alen)
{
    int len = RTA_LENGTH(alen);
    rtattr *rta;

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > (__u32) maxlen) {
        fprintf(stderr,
            "addattr_l ERROR: message exceeded bound of %d\n",
            maxlen);
        return -1;
    }
    rta = NLMSG_TAIL(n);
    rta->rta_type = type;
    rta->rta_len = len;
    if (alen)
        memcpy(RTA_DATA(rta), data, alen);
    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
    return 0;
}

/*
 * Taken from iproute2/lib/libnetlink.c
 * Calls addattr_l() and returns pointer to newly added struct rtattr.
 */
rtattr *addattr_nest(nlmsghdr *n, int maxlen, int type)
{
    rtattr *nest = NLMSG_TAIL(n);

    addattr_l(n, maxlen, type, NULL, 0);
    return nest;
}

/*
 * Taken from iproute2/lib/libnetlink.c
 * Set rta_len attribute of nest to actual length of structure instead of maxlen
 * (Returns nlmsg_len attribute of netlink header)
 */
int addattr_nest_end(nlmsghdr *n, rtattr *nest)
{
    //nest->rta_len = (void *)NLMSG_TAIL(n) - (void *)nest;
    nest->rta_len = (char *)NLMSG_TAIL(n) - (char *)nest;
    return n->nlmsg_len;
}

/*
 * Taken from iproute2/tc/tc_util.c
 * Reads a classid string and saves classid as __u32 in *h
 * Returns 0 if classid was found, otherwise -1
 * ! Function changed to remove goto statements !
 */
int get_tc_classid(__u32 *h, const char *str)
{
	unsigned long maj, min;
	char *p;

	if (strcmp(str, "root") == 0) {
        *h = TC_H_ROOT;
	    return 0;
    }		
	if (strcmp(str, "none") == 0) {
        *h = TC_H_UNSPEC;
	    return 0;
    }

	maj = strtoul(str, &p, 16);
	if (p == str) {
		maj = 0;
		if (*p != ':')
			return -1;
	}
	if (*p == ':') {
		if (maj >= (1<<16))
			return -1;
		maj <<= 16;
		str = p+1;
		min = strtoul(str, &p, 16);
		if (*p != 0)
			return -1;
		if (min >= (1<<16))
			return -1;
		maj |= min;
	} else if (*p != 0)
		return -1;

	*h = maj;
	return 0;
}

/*
 * Create and send a netlink message for CBS qdisc
 * This function uses sections from the iproute2 library:
 *   + iproute2/tc/tc.c
 *   + iproute2/tc/tc_qdisc.c
 *   + iproute2/tc/q_cbs.c
 *   + iproute2/lib/libnetlink.c
 * Source for netlink message creation and sending:
 *   https://www.linuxjournal.com/article/8498
 */
int send_cbs_message(__u16 type, __u16 flags, const char *dev, const char *handle, const char *parent, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope)
{
    unsigned int ifindex = if_nametoindex(dev);
    if (!ifindex) {
        fprintf(stderr, "Could not find interface.\n");
        return -1;
    }
    __u32 parent_classid;
    if (get_tc_classid(&parent_classid, parent)) {
        fprintf(stderr, "Could not find parent qdisc.\n");
        return -1;
    }

    __u32 qdisc_classid;
    if (get_tc_classid(&qdisc_classid, handle)) {
        fprintf(stderr, "Could not determine qdisc classid.\n");
        return -1;
    }

    nl_tc_message request = {};
    request.nl_header.nlmsg_len = NLMSG_LENGTH(sizeof(tcmsg));
    request.nl_header.nlmsg_flags = NLM_F_REQUEST | flags;
    request.nl_header.nlmsg_type = type;
    request.tc_header.tcm_family = AF_NETLINK;
    request.tc_header.tcm_handle = qdisc_classid;
    request.tc_header.tcm_parent = parent_classid;
    request.tc_header.tcm_ifindex = ifindex;

    addattr_l(&request.nl_header, sizeof(request), TCA_KIND, "cbs", strlen("cbs")+1);

    tc_cbs_qopt cbs_params = {};
    cbs_params.hicredit = hicredit;
    cbs_params.locredit = locredit;
    cbs_params.idleslope = idleslope;
    cbs_params.sendslope = sendslope;

    rtattr *tail = addattr_nest(&request.nl_header, 1024, TCA_OPTIONS);
    addattr_l(&request.nl_header, 2024, TCA_CBS_PARMS, &cbs_params, sizeof(cbs_params));
    addattr_nest_end(&request.nl_header, tail);

    int nl_socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (nl_socket < 0) {
        fprintf(stderr, "Error while opening netlink socket.\n");
        return -1;
    }

    // socket bind is not required -> leave out if error occurs
    /*sockaddr_nl local_address = {};
    local_address.nl_family = AF_NETLINK;
    local_address.nl_pid = getpid();
    
    int bind_error = bind(nl_socket, (sockaddr*) &local_address, sizeof(local_address));
    if (bind_error < 0) {
        fprintf(stderr, "Error while binding netlink socket.\n");
        return -1;
    }*/

    int send_error = send(nl_socket, &request, request.nl_header.nlmsg_len, 0);
    if (send_error < 0) {
        fprintf(stderr, "Problem with Netlink request.\n");
        return -1;
    }

    close(nl_socket);
    return 0;
}

// tc qdisc add ... cbs ...
int add_cbs(const char *dev, const char *handle, const char *parent, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope)
{
    return send_cbs_message(RTM_NEWQDISC, NLM_F_CREATE | NLM_F_EXCL, dev, handle, parent, hicredit, locredit, idleslope, sendslope);
}

// tc qdisc change ... cbs ...
int change_cbs(const char *dev, const char *handle, const char *parent, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope)
{
    return send_cbs_message(RTM_NEWQDISC, 0, dev, handle, parent, hicredit, locredit, idleslope, sendslope);
}

// tc qdisc replace ... cbs ...
int replace_cbs(const char *dev, const char *handle, const char *parent, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope)
{
    return send_cbs_message(RTM_NEWQDISC, NLM_F_CREATE | NLM_F_REPLACE, dev, handle, parent, hicredit, locredit, idleslope, sendslope);
}

// tc qdisc link ... cbs ...
int link_cbs(const char *dev, const char *handle, const char *parent, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope)
{
    return send_cbs_message(RTM_NEWQDISC, NLM_F_REPLACE, dev, handle, parent, hicredit, locredit, idleslope, sendslope);
}

// tc qdisc delete ... cbs ...
int delete_cbs(const char *dev, const char *handle, const char *parent, __s32 hicredit, __s32 locredit, __s32 idleslope, __s32 sendslope)
{
    return send_cbs_message(RTM_DELQDISC, 0, dev, handle, parent, hicredit, locredit, idleslope, sendslope);
}
