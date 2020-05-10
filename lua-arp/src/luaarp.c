#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/iso88025.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef void (action_fn)(struct sockaddr_dl *sdl, struct sockaddr_in *s_in,
    struct rt_msghdr *rtm, lua_State *L, char *rifname);


static void l_pushtablestring(lua_State* L, char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static void l_pushtableboolean(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushboolean(L, value);
    lua_settable(L, -3);
}

static void l_pushtablenumber(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}

static void l_pushtableinteger(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

static void l_pushtablenil(lua_State* L, char* key)
{
    lua_pushstring(L, key);
    lua_pushnil(L);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_string(lua_State* L, int key , char* value)
{
    lua_pushinteger(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_number(lua_State* L, int key , int value)
{
    lua_pushinteger(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_integer(lua_State* L, int key , int value)
{
    lua_pushinteger(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_nil(lua_State* L, int key)
{
    lua_pushinteger(L, key);
    lua_pushnil(L);
    lua_settable(L, -3);
}

static int
valid_type(int type)
{

	switch (type) {
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_INFINIBAND:
	case IFT_ISO88023:
	case IFT_ISO88024:
	case IFT_ISO88025:
	case IFT_L2VLAN:
	case IFT_BRIDGE:
		return (1);
	default:
		return (0);
	}
}

static struct rt_msghdr *
arp_rtmsg(int cmd, struct sockaddr_in *dst, struct sockaddr_dl *sdl, int flags, int doing_proxy)
{
	static int seq;
	int rlen;
	int l;
	struct sockaddr_in so_mask, *som = &so_mask;
	static int s = -1;
	static pid_t pid;

	static struct	{
		struct	rt_msghdr m_rtm;
		char	m_space[512];
	}	m_rtmsg;

	time_t	expire_time;
	struct rt_msghdr *rtm = &m_rtmsg.m_rtm;
	char *cp = m_rtmsg.m_space;

	if (s < 0) {	/* first time: open socket, get pid */
		s = socket(PF_ROUTE, SOCK_RAW, 0);
		if (s < 0) {
			printf("%s\n", strerror(errno));
			return NULL;
		}
		pid = getpid();
	}
	bzero(&so_mask, sizeof(so_mask));
	so_mask.sin_len = 8;
	so_mask.sin_addr.s_addr = 0xffffffff;

	errno = 0;
	/*
	 * XXX RTM_DELETE relies on a previous RTM_GET to fill the buffer
	 * appropriately.
	 */
	if (cmd == RTM_DELETE)
		goto doit;
	bzero((char *)&m_rtmsg, sizeof(m_rtmsg));
	rtm->rtm_flags = flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		return NULL;
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = expire_time;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC | RTF_LLDATA);
		if (doing_proxy) {
			rtm->rtm_addrs |= RTA_NETMASK;
			rtm->rtm_flags &= ~RTF_HOST;
		}
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}
#define NEXTADDR(w, s)						\
	do {							\
		if ((s) != NULL && rtm->rtm_addrs & (w)) {	\
			bcopy((s), cp, sizeof(*(s)));		\
			cp += SA_SIZE(s);			\
		}						\
	} while (0)

	NEXTADDR(RTA_DST, dst);
	NEXTADDR(RTA_GATEWAY, sdl);
	NEXTADDR(RTA_NETMASK, som);

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			printf("%s\n", "writing to routing socket");
			return (NULL);
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		printf("%s\n", "read from routing socket");

	return (rtm);
}

static void
print_entry(struct sockaddr_dl *sdl,
	struct sockaddr_in *addr, struct rt_msghdr *rtm, lua_State *L, char *rifname)
{
	const char *host;
	struct hostent *hp;
	struct iso88025_sockaddr_dl_data *trld;
	struct if_nameindex *p;
	int seg;
	struct if_nameindex *ifnameindex = NULL;

	if ((ifnameindex = if_nameindex()) == NULL) {
		return;
	}

	hp = gethostbyaddr((caddr_t)&(addr->sin_addr),
	    sizeof addr->sin_addr, AF_INET);
	if (hp) {
		host = hp->h_name;
	} else {
		host = "?";
	}
	l_pushtablestring(L, "hostname", (char *)host);
	l_pushtablestring(L, "ipaddr", inet_ntoa(addr->sin_addr));
	time_t	expire_time;
	if (sdl->sdl_alen) {
		if ((sdl->sdl_type == IFT_ETHER ||
		    sdl->sdl_type == IFT_L2VLAN ||
		    sdl->sdl_type == IFT_BRIDGE) &&
		    sdl->sdl_alen == ETHER_ADDR_LEN)
		    l_pushtablestring(L, "mac", ether_ntoa((struct ether_addr *)LLADDR(sdl)));
		else {
			int n = sdl->sdl_nlen > 0 ? sdl->sdl_nlen + 1 : 0;

		    l_pushtablestring(L, "mac", link_ntoa(sdl) + n);
		}
	} else
		l_pushtableboolean(L, "incomplete", 1);

	for (p = ifnameindex; p && ifnameindex->if_index &&
	    ifnameindex->if_name; p++) {
		if (p->if_index == sdl->sdl_index) {
			l_pushtablestring(L, "ifname", p->if_name);
			break;
		}
	}

	if (rtm->rtm_rmx.rmx_expire == 0) {
		l_pushtableboolean(L, "permanent", 1);
	} else {
		static struct timespec tp;
		if (tp.tv_sec == 0)
			clock_gettime(CLOCK_MONOTONIC, &tp);
		if ((expire_time = rtm->rtm_rmx.rmx_expire - tp.tv_sec) > 0) {
			l_pushtableinteger(L, "expire_secs", (int)expire_time);
		} else {
			l_pushtableboolean(L, "expired", 1);
		}
	}

	if (rtm->rtm_flags & RTF_ANNOUNCE)
		l_pushtableboolean(L, "published", 1);

	switch(sdl->sdl_type) {
	case IFT_ETHER:
		l_pushtablestring(L, "type", "ethernet");
		break;
	case IFT_ISO88025:
	l_pushtablestring(L, "type", "token-ring");
		// trld = SDL_ISO88025(sdl);
		// if (trld->trld_rcf != 0) {
		// 	xo_emit(" rt=%x", ntohs(trld->trld_rcf));
		// 	for (seg = 0;
		// 	     seg < ((TR_RCF_RIFLEN(trld->trld_rcf) - 2 ) / 2);
		// 	     seg++)
		// 		xo_emit(":%x", ntohs(*(trld->trld_route[seg])));
		// }
                break;
	case IFT_FDDI:
		l_pushtablestring(L, "type", "fddi");
		break;
	case IFT_ATM:
		l_pushtablestring(L, "type", "atm");
		break;
	case IFT_L2VLAN:
		l_pushtablestring(L, "type", "vlan");
		break;
	case IFT_IEEE1394:
		l_pushtablestring(L, "type", "firewire");
		break;
	case IFT_BRIDGE:
		l_pushtablestring(L, "type", "bridge");
		break;
	case IFT_INFINIBAND:
		l_pushtablestring(L, "type", "infiniband");
		break;
	default:
		break;
	}
	if_freenameindex(ifnameindex);
}


static int
search(u_long addr, action_fn *action, lua_State *L, char *rifname)
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_in *sin2;
	struct sockaddr_dl *sdl;
	char ifname[IF_NAMESIZE];
	int st, found_entry = 0;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
#ifdef RTF_LLINFO
	mib[5] = RTF_LLINFO;
#else
	mib[5] = 0;
#endif
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		return -1;
	}
	if (needed == 0)	/* empty table */
		return 0;
	buf = NULL;
	for (;;) {
		buf = reallocf(buf, needed);
		if (buf == NULL) {
			return -1;
		}
		st = sysctl(mib, 6, buf, &needed, NULL, 0);
		if (st == 0 || errno != ENOMEM)
			break;
		needed += needed / 8;
	}
	if (st == -1) {
		return -1;
	}
	lim = buf + needed;
	int i = 0;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sin2 = (struct sockaddr_in *)(rtm + 1);
		sdl = (struct sockaddr_dl *)((char *)sin2 + SA_SIZE(sin2));
		if (rifname && if_indextoname(sdl->sdl_index, ifname) &&
		    strcmp(ifname, rifname))
			continue;
		if (addr) {
			if (addr != sin2->sin_addr.s_addr)
				continue;
			found_entry = 1;
		}
		if (action == print_entry) {
			lua_pushinteger(L, i + 1);
			lua_newtable(L);
		}
		(*action)(sdl, sin2, rtm, L, rifname);
		if (action == print_entry)
			lua_settable(L, -3);
		i++;
	}
	free(buf);
	return (found_entry);
}

static struct sockaddr_in *
getaddr(char *host)
{
	struct hostent *hp;
	static struct sockaddr_in reply;

	bzero(&reply, sizeof(reply));
	reply.sin_len = sizeof(reply);
	reply.sin_family = AF_INET;
	reply.sin_addr.s_addr = inet_addr(host);
	if (reply.sin_addr.s_addr == INADDR_NONE) {
		if (!(hp = gethostbyname(host))) {
			printf("%s: %s\n", host, strerror(h_errno));
			return (NULL);
		}
		bcopy((char *)hp->h_addr, (char *)&reply.sin_addr,
			sizeof reply.sin_addr);
	}
	return (&reply);
}

static int
delete(char *host, int flags, int doing_proxy)
{
	struct sockaddr_in *addr, *dst;
	struct rt_msghdr *rtm;
	struct sockaddr_dl *sdl;
	struct sockaddr_dl sdl_m;

	dst = getaddr(host);
	if (dst == NULL)
		return (1);

	/*
	 * Perform a regular entry delete first.
	 */
	flags &= ~RTF_ANNOUNCE;

	/*
	 * setup the data structure to notify the kernel
	 * it is the ARP entry the RTM_GET is interested
	 * in
	 */
	bzero(&sdl_m, sizeof(sdl_m));
	sdl_m.sdl_len = sizeof(sdl_m);
	sdl_m.sdl_family = AF_LINK;

	for (;;) {	/* try twice */
		rtm = arp_rtmsg(RTM_GET, dst, &sdl_m, flags, doing_proxy);
		if (rtm == NULL) {
			return (1);
		}
		addr = (struct sockaddr_in *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(SA_SIZE(addr) + (char *)addr);

		if (sdl->sdl_family == AF_LINK &&
		    !(rtm->rtm_flags & RTF_GATEWAY) &&
		    valid_type(sdl->sdl_type) ) {
			addr->sin_addr.s_addr = dst->sin_addr.s_addr;
			break;
		}

		/*
		 * Regualar entry delete failed, now check if there
		 * is a proxy-arp entry to remove.
		 */
		if (flags & RTF_ANNOUNCE) {
			printf("delete: cannot locate %s", host);
			return (1);
		}

		flags |= RTF_ANNOUNCE;
	}
	rtm->rtm_flags |= RTF_LLDATA;
	if (arp_rtmsg(RTM_DELETE, dst, NULL, flags, doing_proxy) != NULL) {
		printf("%s (%s) deleted\n", host, inet_ntoa(addr->sin_addr));
		return (0);
	}
	return (1);
}


static int libluaarp_get(lua_State *L)
{
	const char *ifname = luaL_optstring(L, 1, NULL);
	const char *hostname = luaL_optstring(L, 2, NULL);
	if (ifname != NULL) {
		if (if_nametoindex(ifname) == 0) {
			printf("interface %s does not exist\n", ifname);
			lua_pushnil(L);
		    return 1;
		}
	}
	if (hostname == NULL) {
		lua_newtable(L);
		if (search(0, print_entry, L, (char *)ifname) < 0) {
			lua_pop(L, 1);
		} else {
			return 1;
		}
	} else {
		struct sockaddr_in *addr;
		addr = getaddr((char *)hostname);
		if (addr != NULL) {
			lua_newtable(L);
			if (search(addr->sin_addr.s_addr, print_entry, L, (char *)ifname) <= 0) {
				lua_pop(L, 1);
			} else {
				return 1;
			}
		}
	}

	lua_pushnil(L);
    return 1;
}
static void
nuke_entry(struct sockaddr_dl *sdl __unused,
	struct sockaddr_in *addr, struct rt_msghdr *rtm, lua_State *L, char *rifname)
{
	char ip[20];

	if (rtm->rtm_flags & RTF_PINNED)
		return;

	snprintf(ip, sizeof(ip), "%s", inet_ntoa(addr->sin_addr));
	delete(ip, 0, 0);
}


static int libluaarp_delete(lua_State *L)
{
	const char *hostname = luaL_optstring(L, 1, NULL);

	if (hostname != NULL) {
		if (!delete((char *)hostname, 0, 0)) {
			lua_pushboolean(L, 1);
			return 1;
		}
	}

	lua_pushnil(L);
    return 1;
}

static int libluaarp_flush(lua_State *L)
{
	const char *ifname = luaL_optstring(L, 1, NULL);
	if (ifname != NULL) {
		if (if_nametoindex(ifname) == 0) {
			printf("interface %s does not exist\n", ifname);
			lua_pushnil(L);
		    return 1;
		}
	}

	if (search(0, nuke_entry, NULL, (char *)ifname) == 0) {
		lua_pushboolean(L, 1);
		return 1;
	}

	lua_pushnil(L);
    return 1;
}

static const luaL_Reg luaarp[] = {
    {"get", libluaarp_get},
    {"delete", libluaarp_delete},
    {"flush", libluaarp_flush},
    {NULL, NULL}
};


#ifdef STATIC
LUAMOD_API int luaopen_libluaarp(lua_State *L)
#else
LUALIB_API int luaopen_libluaarp(lua_State *L)
#endif
{
    luaL_newlib(L, luaarp);
    return 1;
}
