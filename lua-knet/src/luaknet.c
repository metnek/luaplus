#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#include <jail.h>
#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_gre.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>
#include <net/route.h>
#include <netdb.h>


typedef void (action_fn)(struct sockaddr_dl *sdl, struct sockaddr_in *s_in,
    struct rt_msghdr *rtm, lua_State *L, char *rifname);


struct fibl {
	TAILQ_ENTRY(fibl)	fl_next;

	int	fl_num;
	int	fl_error;
	int	fl_errno;
};
TAILQ_HEAD(fibl_head_t, fibl) fibl_head;

#define	satosin(sa)	((struct sockaddr_in *)(sa))
#define	satosin6(sa)	((struct sockaddr_in6 *)(sa))
#define	sin6tosa(sin6)	((struct sockaddr *)(sin6))

#define	KNET_ALL_STRLEN					64

#define NEXTADDR(w, u)										\
	if (rtm_addrs & (w)) {									\
		l = (((struct sockaddr *)&(u))->sa_len == 0) ?		\
		    sizeof(long) :									\
		    1 + ((((struct sockaddr *)&(u))->sa_len - 1)	\
			| (sizeof(long) - 1));							\
		memmove(cp, (char *)&(u), l);						\
		cp += l;											\
	}

#define rtm m_rtmsg.m_rtm


static int
rtmsg(int type, int flags, int rtm_addrs, int *seq, int fib, struct sockaddr_storage *so)
{

	int s;
	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0) {
		return -1;
	}
	shutdown(s, SHUT_RD);
	struct {
		struct	rt_msghdr m_rtm;
		char	m_space[512];
	} m_rtmsg;

	struct rt_metrics rt_metrics = {0};
	u_long  rtm_inits = 0;

	rtm.rtm_type = type;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++(*seq);
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_pid = 0;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;
	rtm.rtm_errno = 0;

	char *cp = m_rtmsg.m_space;
	int l;

	NEXTADDR(RTA_DST, so[RTAX_DST]);
	NEXTADDR(RTA_GATEWAY, so[RTAX_GATEWAY]);
	NEXTADDR(RTA_NETMASK, so[RTAX_NETMASK]);
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;

	if (setsockopt(s, SOL_SOCKET, SO_SETFIB, (void *)&fib, sizeof(fib)) != 0) {
		close(s);
	    return -1;
	}
	int rlen;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
        if (type == RTM_DELETE && errno == ESRCH) {
          close(s);
          return 0;
        }
		close(s);
		return -1;
	}
	close(s);
	return 0;
}
static int
fiboptlist_range(const char *arg, struct fibl_head_t *flh, int numfibs)
{
	struct fibl *fl;
	char *str0, *str, *token, *endptr;
	int fib[2], i, error;
	str0 = str = strdup(arg);
	error = 0;
	i = 0;
	while ((token = strsep(&str, "-")) != NULL) {
		switch (i) {
		case 0:
		case 1:
			errno = 0;
			fib[i] = strtol(token, &endptr, 0);
			if (errno == 0) {
				if (*endptr != '\0' ||
				    fib[i] < 0 ||
				    (numfibs != -1 && fib[i] > numfibs - 1))
					errno = EINVAL;
			}
			if (errno)
				error = 1;
			break;
		default:
			error = 1;
		}
		if (error)
			goto fiboptlist_range_ret;
		i++;
	}
	if (fib[0] >= fib[1]) {
		error = 1;
		goto fiboptlist_range_ret;
	}
	for (i = fib[0]; i <= fib[1]; i++) {
		fl = calloc(1, sizeof(*fl));
		if (fl == NULL) {
			error = 1;
			goto fiboptlist_range_ret;
		}
		fl->fl_num = i;
		TAILQ_INSERT_TAIL(flh, fl, fl_next);
	}
fiboptlist_range_ret:
	free(str0);
	return (error ? -1 : 0);
}

static int
fiboptlist_csv(const char *arg, struct fibl_head_t *flh, int numfibs, int defaultfib)
{
	struct fibl *fl;
	char *str0, *str, *token, *endptr;
	int fib, error;

	str0 = str = NULL;
	if (strcmp("all", arg) == 0) {
		str = calloc(1, KNET_ALL_STRLEN);
		if (str == NULL) {
			error = 1;
			goto fiboptlist_csv_ret;
		}
		if (numfibs > 1)
			snprintf(str, KNET_ALL_STRLEN - 1, "%d-%d", 0, numfibs - 1);
		else
			snprintf(str, KNET_ALL_STRLEN - 1, "%d", 0);
	} else if (strcmp("default", arg) == 0) {
		str0 = str = calloc(1, KNET_ALL_STRLEN);
		if (str == NULL) {
			error = 1;
			goto fiboptlist_csv_ret;
		}
		snprintf(str, KNET_ALL_STRLEN - 1, "%d", defaultfib);
	} else
		str0 = str = strdup(arg);

	error = 0;
	while ((token = strsep(&str, ",")) != NULL) {
		if (*token != '-' && strchr(token, '-') != NULL) {
			error = fiboptlist_range(token, flh, numfibs);
			if (error)
				goto fiboptlist_csv_ret;
		} else {
			errno = 0;
			fib = strtol(token, &endptr, 0);
			if (errno == 0) {
				if (*endptr != '\0' ||
				    fib < 0 ||
				    (numfibs != -1 && fib > numfibs - 1))
					errno = EINVAL;
			}
			if (errno) {
				error = 1;
				goto fiboptlist_csv_ret;
			}
			fl = calloc(1, sizeof(*fl));
			if (fl == NULL) {
				error = 1;
				goto fiboptlist_csv_ret;
			}
			fl->fl_num = fib;
			TAILQ_INSERT_TAIL(flh, fl, fl_next);
		}
	}
fiboptlist_csv_ret:
	if (str0 != NULL)
		free(str0);
	return (error);
}

static int
fill_fibs(struct fibl_head_t *flh)
{
	int	defaultfib, numfibs;
	int error;
	size_t len;

	len = sizeof(numfibs);
	if (sysctlbyname("net.fibs", (void *)&numfibs, &len, NULL, 0) == -1)
		numfibs = -1;
	len = sizeof(defaultfib);
	if (numfibs != -1 &&
	    sysctlbyname("net.my_fibnum", (void *)&defaultfib, &len, NULL,
		0) == -1)
		defaultfib = -1;

	if (TAILQ_EMPTY(flh)) {
		error = fiboptlist_csv("default", flh, numfibs, defaultfib);
		if (error)
			return -1;
	}

	return 0;
}


static void l_pushtableboolean(lua_State* L, char* key, int value)
{
    lua_pushstring(L, key);
    lua_pushboolean(L, value);
    lua_settable(L, -3);
}


static void l_pushtableinteger(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

static void l_pushtablestring(lua_State* L, char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static int libknet_setip(lua_State *L)
{
	const char *ifname = luaL_optstring(L, 1, NULL);
	int type = luaL_optnumber(L, 2, 4);
	const char *ip = luaL_optstring(L, 3, NULL);
	const char *netmask = luaL_optstring(L, 4, NULL);
	if (ifname == NULL || ip == NULL || netmask == NULL) {
	    lua_pushnil(L);
	    return 1;
	}

	if (type == 4) {

		int s;
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		    lua_pushnil(L);
		    return 1;
		}
		int ret = 0;

		struct in_aliasreq in_addreq;
		memset(&in_addreq, 0, sizeof(in_addreq));
		struct sockaddr_in *min = ((struct sockaddr_in *) &(in_addreq.ifra_mask));
		min->sin_family = AF_INET;
		min->sin_len = sizeof(*min);
		inet_aton(netmask, &min->sin_addr);

		struct sockaddr_in *sin = ((struct sockaddr_in *) &(in_addreq.ifra_addr));
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		inet_aton(ip, &sin->sin_addr);
		strlcpy(((struct ifreq *)&in_addreq)->ifr_name, ifname,
				IFNAMSIZ);

		ret = ioctl(s, SIOCAIFADDR, (char *)&in_addreq);
		if (ret < 0) {
			close(s);
		    lua_pushnil(L);
		    return 1;
	    }
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, ifname);
	    ioctl(s, SIOCGIFFLAGS, &ifr);

	    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

	    ioctl(s, SIOCSIFFLAGS, &ifr);
		close(s);
	} else if (type == 6) {
		int prefix = (int)strtol(netmask, NULL, 10);
		int s;
		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		    lua_pushnil(L);
		    return 1;
		}

		int ret = 0;
		struct in6_aliasreq in6_addreq = 
		  { .ifra_flags = 0,
		    .ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };

		long prefix_cmd = (long)prefix;
		struct sockaddr_in6 *pref = ((struct sockaddr_in6 *) &(in6_addreq.ifra_prefixmask));
		pref->sin6_family = AF_INET6;
		pref->sin6_len = sizeof(*pref);
		if ((prefix == 0) || (prefix == 128)) {
			memset(&pref->sin6_addr, 0xff, sizeof(struct in6_addr));
		} else {
			memset((void *)&pref->sin6_addr, 0x00, sizeof(pref->sin6_addr));
			u_char *cp;
			for (cp = (u_char *)&pref->sin6_addr; prefix > 7; prefix -= 8)
				*cp++ = 0xff;
			*cp = 0xff << (8 - (long)prefix);
		}
		struct sockaddr_in6 *sin = ((struct sockaddr_in6 *) &(in6_addreq.ifra_addr));
		sin->sin6_len = sizeof(*sin);
		sin->sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, ip, &sin->sin6_addr) != 1) {
			close(s);
		    lua_pushnil(L);
		    return 1;
		}
		strlcpy(((struct ifreq *)&in6_addreq)->ifr_name, ifname,
				IFNAMSIZ);
		ret = ioctl(s, SIOCAIFADDR_IN6, (caddr_t)&in6_addreq);
		close(s);
		if (ret < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
	}

    lua_pushboolean(L, 1);
    return 1;
}

static int libknet_delip(lua_State *L)
{
	const char *ifname = luaL_optstring(L, 1, NULL);
	int type = luaL_optnumber(L, 2, 4);
	const char *ip = luaL_optstring(L, 3, NULL);

	if (ifname == NULL || ip == NULL) {
	    lua_pushnil(L);
	    return 1;
	}

	int ret = 0;
	if (type == 4) {

		int s;
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		    lua_pushnil(L);
		    return 1;
		}

		struct ifreq ifr;
		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, ifname);
		struct sockaddr_in *sai = (struct sockaddr_in *)&ifr.ifr_addr;
		memset(sai, 0, sizeof(struct sockaddr));
		sai->sin_family = AF_INET;
		sai->sin_port = 0;
		sai->sin_addr.s_addr = inet_addr(ip);

		ret = ioctl(s, SIOCDIFADDR, (caddr_t)&ifr);

		close(s);
		if (ret < 0) {
		    lua_pushnil(L);
		    return 1;
		}
	} else if (type == 6) {

		int s;
		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			lua_pushnil(L);
			return 1;
		}

		struct in6_aliasreq in6_addreq = 
		  { .ifra_flags = 0,
			.ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };
		strlcpy(((struct ifreq *)&in6_addreq)->ifr_name, ifname,
				IFNAMSIZ);
		struct addrinfo hints, *res;
		bzero(&hints, sizeof(struct addrinfo));
		hints.ai_family = AF_INET6;
		getaddrinfo(ip, NULL, &hints, &res);
		bcopy(res->ai_addr, (struct sockaddr_in6 *) &(in6_addreq.ifra_addr), res->ai_addrlen);
		freeaddrinfo(res);
		ret = ioctl(s, SIOCDIFADDR_IN6, (caddr_t)&in6_addreq);
		close(s);
	}

	if (ret < 0) {
	    lua_pushnil(L);
	    return 1;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int libknet_clear(lua_State *L)
{
	const char *ifname = luaL_optstring(L, 1, NULL);

	struct ifaddrs *ifap, *ifa;

	if (ifname == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

	if (getifaddrs(&ifap) != 0) {
	    lua_pushnil(L);
	    return 1;
    }

    int c = 0;

    int fail = 0;

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, ifname) != 0) {
			continue;
		}
		c++;
		if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6) {
			continue;
		}

		int s, ret;

		if ((s = socket(ifa->ifa_addr->sa_family, SOCK_DGRAM, 0)) < 0) {
			if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0)  {
				continue;
			}
		}

		if(ifa->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;
			struct ifreq ifr;
			memset(&ifr, 0, sizeof(struct ifreq));
			strcpy(ifr.ifr_name, ifname);
			memmove((struct sockaddr_in *) &(ifr.ifr_addr), (struct sockaddr_in *)ifa->ifa_addr, sizeof(struct sockaddr_in));
			ret = ioctl(s, SIOCDIFADDR, (caddr_t)&ifr);
			char address[INET6_ADDRSTRLEN];
			inet_ntop (((struct sockaddr_in *)ifa->ifa_addr)->sin_family, &((struct sockaddr_in *)  ifa->ifa_addr)->sin_addr, address, sizeof (address));
			if (ret)
				fail++;
		} else {
			struct sockaddr_in6 *sin;
			struct in6_aliasreq in6_addreq = 
			  { .ifra_flags = 0,
			    .ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };
			strlcpy(((struct ifreq *)&in6_addreq)->ifr_name, ifname,
					IFNAMSIZ);

			memmove((struct sockaddr_in6 *) &(in6_addreq.ifra_addr), (struct sockaddr_in6 *)ifa->ifa_addr, sizeof(struct sockaddr_in6));
			ret = ioctl(s, SIOCDIFADDR_IN6, (caddr_t)&in6_addreq);
			if (ret)
				fail++;
		}

		close(s);

    }
	freeifaddrs(ifap);
	if (c && !fail) {
	    lua_pushboolean(L, 1);
	} else if (fail) {
		lua_pushinteger(L, fail);
	} else {
	    lua_pushnil(L);
	}

    return 1;
}

static int libknet_rename(lua_State *L)
{
	const char *ifname = luaL_optstring(L, 1, NULL);
	const char *new_ifname = luaL_optstring(L, 2, NULL);
	if (ifname == NULL || new_ifname == NULL) {
		lua_pushnil(L);
		return 1;
	}

	int s, ret;
	if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
		if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			lua_pushnil(L);
			return 1;
		}
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(struct ifreq));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	char *newname = strdup(new_ifname);
	if (newname == NULL) {
		close(s);
		lua_pushnil(L);
		return 1;
	}

	ifr.ifr_data = newname;
	if (ioctl(s, SIOCSIFNAME, (caddr_t)&ifr) < 0) {
		free(newname);
		close(s);
		lua_pushnil(L);
		return 1;
	}
	free(newname);
	close(s);

	lua_pushboolean(L, 1);
	return 1;
}

static int libknet_setmac(lua_State *L)
{
	const char *iface = luaL_optstring(L, 1, NULL);
	const char *mac = luaL_optstring(L, 2, NULL);
    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			lua_pushnil(L);
			return 1;
        }
    }
    struct ifreq link_ridreq;
    memset(&link_ridreq, 0, sizeof(struct ifreq));
    struct sockaddr_dl sdl;
    memset(&sdl, 0, sizeof(struct sockaddr_dl));
    struct sockaddr *sa = &link_ridreq.ifr_addr;
    char *temp;

    if ((temp = malloc(strlen(mac) + 2)) == NULL) {
        close(s);
		lua_pushnil(L);
		return 1;
    }
    temp[0] = ':';
    strcpy(temp + 1, mac);
    sdl.sdl_len = sizeof(sdl);
    link_addr(temp, &sdl);
    free(temp);
    sa->sa_family = AF_LINK;
    sa->sa_len = sdl.sdl_alen;
    bcopy(LLADDR(&sdl), sa->sa_data, sdl.sdl_alen);
    strlcpy(link_ridreq.ifr_name, iface, sizeof(link_ridreq.ifr_name));
    if (ioctl(s, SIOCSIFLLADDR, (caddr_t)&link_ridreq) < 0) {
        close(s);
		lua_pushnil(L);
		return 1;
    }

    close(s);
	lua_pushboolean(L, 1);
	return 1;
}

static int libknet_ifcap(lua_State *L)
{
	const char *iface = luaL_optstring(L, 1, NULL);
	if (iface == NULL) {
		lua_pushnil(L);
		return 1;
	}

	int top = lua_gettop(L);

    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			lua_pushnil(L);
			return 1;
        }
    }
	struct ifreq ifr;

	for (int i = 2; i <= top; i++) {
		const char *flagname = luaL_optstring(L, i, NULL);
		int is_off = 0;
		if (flagname == NULL)
			continue;

		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, iface);
	 	if (ioctl(s, SIOCGIFCAP, (caddr_t)&ifr) < 0) {
			lua_pushnil(L);
			return 1;
	 	}
		int all_flags = 0;
		all_flags = ifr.ifr_curcap;
		if (*flagname == '-') {
			flagname++;
			is_off = 1;
		}

		int flag = 0;
		if (strcmp(flagname, "lro") == 0) {
			flag = IFCAP_LRO;
		} else if (strcmp(flagname, "tso") == 0) {
			flag = IFCAP_TSO;
		} else if (strcmp(flagname, "rxcsum") == 0) {
			flag = IFCAP_RXCSUM;
		} else if (strcmp(flagname, "txcsum") == 0) {
			flag = IFCAP_TXCSUM;
		} else if (strcmp(flagname, "polling") == 0) {
			flag = IFCAP_POLLING;
		} else if (strcmp(flagname, "toe") == 0) {
			flag = IFCAP_TOE;
		} else if (strcmp(flagname, "wol") == 0) {
			flag = IFCAP_WOL;
		} else if (strcmp(flagname, "netcons") == 0) {
			flag = IFCAP_NETCONS;
		}

		if (is_off) {
			all_flags &= ~flag;
		} else {
			all_flags |= flag;
		}

		all_flags &= ifr.ifr_reqcap;
		ifr.ifr_reqcap = all_flags;
		ioctl(s, SIOCSIFCAP, (caddr_t)&ifr);
	}

    close(s);
	lua_pushboolean(L, 1);
	return 1;
}

static int libknet_flags(lua_State *L)
{
	const char *iface = luaL_optstring(L, 1, NULL);
	if (iface == NULL) {
		lua_pushnil(L);
		return 1;
	}
	int top = lua_gettop(L);

    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			lua_pushnil(L);
			return 1;
        }
    }
	struct ifreq ifr;

	for (int i = 2; i <= top; i++) {
		const char *flagname = luaL_optstring(L, i, NULL);
		int is_off = 0;
		if (flagname == NULL)
			continue;

		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, iface);
	 	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
			lua_pushnil(L);
			return 1;
	 	}

		int all_flags = 0;
		all_flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);
		if (*flagname == '-') {
			flagname++;
			is_off = 1;
		}

		int flag = 0;
		if (strcmp(flagname, "up") == 0) {
			flag = IFF_UP;
		} else if (strcmp(flagname, "down") == 0) {
			flag = IFF_UP;
			is_off = 1;
		} else if (strcmp(flagname, "arp") == 0) {
			flag = IFF_NOARP;
			is_off = !is_off;
		} else if (strcmp(flagname, "debug") == 0) {
			flag = IFF_DEBUG;
		} else if (strcmp(flagname, "promisc") == 0) {
			flag = IFF_PPROMISC;
		} else if (strcmp(flagname, "link0") == 0) {
			flag = IFF_LINK0;
		} else if (strcmp(flagname, "link1") == 0) {
			flag = IFF_LINK1;
		} else if (strcmp(flagname, "link2") == 0) {
			flag = IFF_LINK2;
		} else if (strcmp(flagname, "monitor") == 0) {
			flag = IFF_MONITOR;
		} else if (strcmp(flagname, "staticarp") == 0) {
			flag = IFF_STATICARP;
		} else if (strcmp(flagname, "normal") == 0) {
			flag = IFF_LINK0;
			is_off = 1;
		} else if (strcmp(flagname, "compress") == 0) {
			flag = IFF_LINK0;
		} else if (strcmp(flagname, "noicmp") == 0) {
			flag = IFF_LINK1;
		}

		if (is_off) {
			all_flags &= ~flag;
		} else {
			all_flags |= flag;
		}

		ifr.ifr_flags = all_flags & 0xffff;
		ifr.ifr_flagshigh = all_flags >> 16;
		ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr);
	}

    close(s);
	lua_pushboolean(L, 1);
	return 1;
}

static int libknet_list(lua_State *L)
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
	    lua_pushnil(L);
	    return 1;
    }
    lua_newtable( L );

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		// if ((ifa->ifa_flags & IFF_CANTCONFIG) != 0)
		// 	continue;

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
				int s, ret;
				if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
					if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) 
						continue;
				}
				int xmedia = 1;
				struct ifmediareq ifmr;
				(void) memset(&ifmr, 0, sizeof(ifmr));
				(void) strlcpy(ifmr.ifm_name, ifa->ifa_name, sizeof(ifmr.ifm_name));
				if (ioctl(s, SIOCGIFXMEDIA, (caddr_t)&ifmr) < 0)
					xmedia = 0;
				if (xmedia == 0 && ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
					close(s);
					l_pushtableinteger(L, ifa->ifa_name, 2);
					continue;
				}
				if (ifmr.ifm_status & IFM_AVALID) {
					switch (IFM_TYPE(ifmr.ifm_active)) {
					case IFM_ETHER:
					case IFM_ATM:
						if (ifmr.ifm_status & IFM_ACTIVE && (ifa->ifa_flags & IFF_UP) != 0){
							l_pushtableinteger(L, ifa->ifa_name, 1);
							// add_assoc_long(return_value, Z_STRVAL_P(data), 1);
						} else{
							l_pushtableinteger(L, ifa->ifa_name, 0);
							// add_assoc_long(return_value, Z_STRVAL_P(data), 0);
						}
						break;
					}
				}
				close(s);
		// l_pushtableinteger(L, ifa->ifa_name, ifa->ifa_flags & IFF_UP);
    }

	freeifaddrs(ifap);
	return 1;
}


static void print_table(lua_State *L, int depth)
    {
        // if ((lua_type(L, -2) == LUA_TSTRING))
        //     printf("%s", lua_tostring(L, -2));

        lua_pushnil(L);
        while(lua_next(L, -2) != 0) {
            if(lua_isstring(L, -1))
                printf("%*s = %s\n", depth, lua_tostring(L, -2), lua_tostring(L, -1));
            else if(lua_isnumber(L, -1))
                printf("%*s = %f\n", depth, lua_tostring(L, -2), lua_tonumber(L, -1));
            else if(lua_istable(L, -1)) {
            	printf("table = %s\n", lua_tostring(L, -2));
                print_table(L, depth + 2);
            }
            lua_pop(L, 1);
        }
    }


static void l_pushtable_numkey_string(lua_State* L, int key , char* value)
{
    lua_pushinteger(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static void l_pushtablenumber(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}


static void
printb(lua_State* L, unsigned v, const char *bits)
{
	int i, any = 0;
	char c;

	int j = 0;
	if (bits) {
		bits++;
		while ((i = *bits++) != '\0') {

			if (v & (1 << (i-1))) {
				any = 1;
				char tmp[128];
				const char *p = bits;
				memset(tmp, 0, 128);
				for (; (c = *bits) > 32; bits++) {
				}
				strncpy(tmp, p, bits-p);
				l_pushtable_numkey_string(L, ++j, tmp);
			} else
				for (; *bits > 32; bits++)
					;
		}
	}
}

static int
isnd6defif(int s, char *ifname)
{
	struct in6_ndifreq ndifreq;
	unsigned int ifindex;
	int error;

	memset(&ndifreq, 0, sizeof(ndifreq));
	strlcpy(ndifreq.ifname, ifname, sizeof(ndifreq.ifname));

	ifindex = if_nametoindex(ndifreq.ifname);
	error = ioctl(s, SIOCGDEFIFACE_IN6, (caddr_t)&ndifreq);
	if (error) {
		return (error);
	}
	return (ndifreq.ifindex == ifindex);
}


#define	GREBITS	"\020\01ENABLE_CSUM\02ENABLE_SEQ"

#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\7RUNNING" \
"\10NOARP\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2" \
"\20MULTICAST\22PPROMISC\23MONITOR\24STATICARP"

#define	ND6BITS	"\020\001PERFORMNUD\002ACCEPT_RTADV\003PREFER_SOURCE" \
		"\004IFDISABLED\005DONT_SET_IFROUTE\006AUTO_LINKLOCAL" \
		"\007NO_RADR\010NO_PREFER_IFACE\011NO_DAD\020DEFAULTIF"

static void knet_get_if_nd6(lua_State *L, char *ifname)
{
    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			return;
        }
    }

	struct in6_ndireq nd;
	int s6;
	int error;
	int isdefif;

	struct	ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	memset(&nd, 0, sizeof(nd));
	strlcpy(nd.ifname, ifr.ifr_name, sizeof(nd.ifname));
	if ((s6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		close(s);
		return;
	}
	error = ioctl(s6, SIOCGIFINFO_IN6, &nd);
	if (error) {
		close(s);
		close(s6);
		return;
	}
	isdefif = isnd6defif(s6, ifname);
	close(s6);
	if (nd.ndi.flags == 0 && !isdefif) {
		close(s);
		return;
	}
	lua_pushstring(L, "nd6");
    lua_newtable( L );

	printb(L, (unsigned int)(nd.ndi.flags | (isdefif << 15)), ND6BITS);
    lua_settable( L, -3 );

	close(s);
}

static void knet_get_if_gre(lua_State *L, char *ifname)
{
    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			return;
        }
    }

	struct	ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

	uint32_t opts = 0;

	ifr.ifr_data = (caddr_t)&opts;
	if (ioctl(s, GREGKEY, &ifr) == 0)
		if (opts != 0) {
			l_pushtablenumber(L, "grekey", opts);
		}
	opts = 0;
	if (ioctl(s, GREGOPTS, &ifr) != 0 || opts == 0) {
		close(s);
		return;
	}
	lua_pushstring(L, "gre");
    lua_newtable( L );
	printb(L, opts, GREBITS);
    lua_settable( L, -3 );
	close(s);
}

static void knet_get_if_group(lua_State *L, char *ifname)
{
    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			return;
        }
    }
	int			 len, cnt;
	struct ifgroupreq	 ifgr;
	struct ifg_req		*ifg;

	memset(&ifgr, 0, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, ifname, IFNAMSIZ);

	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
		close(s);
		return;
	}

	len = ifgr.ifgr_len;
	ifgr.ifgr_groups =
	    (struct ifg_req *)calloc(len / sizeof(struct ifg_req),
	    sizeof(struct ifg_req));
	if (ifgr.ifgr_groups == NULL) {
		close(s);
		return;
	}
	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
		close(s);
		return;
	}

	cnt = 0;
	lua_pushstring(L, "groups");
    lua_newtable( L );
	ifg = ifgr.ifgr_groups;
	for (; ifg && len >= sizeof(struct ifg_req); ifg++) {
		len -= sizeof(struct ifg_req);
		if (strcmp(ifg->ifgrq_group, "all")) {
			cnt++;
			l_pushtable_numkey_string(L, cnt, ifg->ifgrq_group);
		}
	}
    lua_settable( L, -3 );

	free(ifgr.ifgr_groups);
	close(s);
}


static void knet_get_if_ip4(lua_State *L, char *ifname)
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
		printf("%s\n", "nooo");
	    lua_pushnil(L);
	    return;
    }
    lua_newtable( L );
    int i = 1;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifname) {
			if (strcmp(ifname, ifa->ifa_name))
				continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET) {
			lua_pushinteger(L, i++);
		    lua_newtable( L );
			char address[INET6_ADDRSTRLEN];
			inet_ntop (((struct sockaddr_in *)ifa->ifa_addr)->sin_family, &((struct sockaddr_in *)  ifa->ifa_addr)->sin_addr, address, sizeof (address));
			l_pushtablestring(L, "ip", address);
			inet_ntop (((struct sockaddr_in *)ifa->ifa_netmask)->sin_family, &((struct sockaddr_in *)  ifa->ifa_netmask)->sin_addr, address, sizeof (address));
			l_pushtablestring(L, "netmask", address);
			if (ifa->ifa_flags & IFF_BROADCAST) {
				inet_ntop (((struct sockaddr_in *)ifa->ifa_broadaddr)->sin_family, &((struct sockaddr_in *)  ifa->ifa_broadaddr)->sin_addr, address, sizeof (address));
				l_pushtablestring(L, "broadcast", address);
			}
		    lua_settable( L, -3 );
		}
    }
	freeifaddrs(ifap);
}

static void knet_get_if_ip6(lua_State *L, char *ifname)
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
	    lua_pushnil(L);
	    return;
    }
    lua_newtable( L );
    int i = 1;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifname) {
			if (strcmp(ifname, ifa->ifa_name))
				continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			lua_pushinteger(L, i++);
		    lua_newtable( L );
			char address[INET6_ADDRSTRLEN];
			inet_ntop (((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_family, &((struct sockaddr_in6 *)  ifa->ifa_addr)->sin6_addr, address, sizeof (address));
			l_pushtablestring(L, "ip", address);
			unsigned char *c = ((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr.s6_addr;
            int i = 0, j = 0;
            unsigned char n = 0;
            while (i < 16) {
                n = c[i];
                while (n > 0) {
                    if (n & 1) j++;
                    n = n/2;
                }
                i++;
            }
            l_pushtableinteger(L, "prefix", j);
		    lua_settable( L, -3 );
		}
    }
	freeifaddrs(ifap);
}


static int libknet_info(lua_State *L)
{
	const char *ifname = luaL_optstring(L, 1, NULL);

	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
	    lua_pushnil(L);
	    return 1;
    }
    lua_newtable( L );

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			if (ifname) {
				if (strcmp(ifname, ifa->ifa_name))
					continue;
			}
			lua_pushstring(L, ifa->ifa_name);
		    lua_newtable( L );
			struct sockaddr_dl *sdl;

			sdl = (struct sockaddr_dl *) ifa->ifa_addr;
			l_pushtablestring(L, "mac", ether_ntoa((struct ether_addr *)LLADDR(sdl)));
            knet_get_if_group(L, ifa->ifa_name);
            knet_get_if_gre(L, ifa->ifa_name);
            knet_get_if_nd6(L, ifa->ifa_name);

		    int s;
		    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
		        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
		        }
		    }
		    if (s > 0) {
				struct	ifreq ifr;
				memset(&ifr, 0, sizeof(ifr));
				strlcpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
				if (ioctl(s, SIOCGIFMETRIC, &ifr) != -1)
					l_pushtableinteger(L, "metric", ifr.ifr_metric);

				memset(&ifr, 0, sizeof(ifr));
				strlcpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ);
				if (ioctl(s, SIOCGIFMTU, &ifr) != -1)
					l_pushtableinteger(L, "mtu", ifr.ifr_mtu);

				close(s);
			}

			lua_pushstring(L, "flags");
		    lua_newtable( L );
            printb(L, ifa->ifa_flags, IFFBITS);
            lua_settable( L, -3 ); /*key flags*/


			lua_pushstring(L, "ip4");
			knet_get_if_ip4(L, ifa->ifa_name);
            lua_settable( L, -3 ); /*key ip4*/
			lua_pushstring(L, "ip6");
			knet_get_if_ip6(L, ifa->ifa_name);
            lua_settable( L, -3 ); /*key ip6*/

            lua_settable( L, -3 ); /*key ifa->ifa_name*/
		}
    }

	freeifaddrs(ifap);
	return 1;
}

static int libknet_route_add(lua_State *L)
{
	int type = luaL_optnumber(L, 1, 4);
	if (type != 4 && type != 6) {
		lua_pushnil(L);
		return 1;
	}
	const char *gtw = luaL_optstring(L, 2, NULL);
	if (gtw == NULL) {
		lua_pushnil(L);
		return 1;
	}
	int fibnum = luaL_optnumber(L, 3, 0);
	struct sockaddr_storage so[RTAX_MAX];
	char dest[] = "0.0.0.0/0";
	TAILQ_INIT(&fibl_head);

	int error = 0;

	if (fill_fibs(&fibl_head) != 0) {
		lua_pushnil(L);
		return 1;
	}

	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	struct sockaddr *sa;

	sa = (struct sockaddr *)&so[RTAX_DST];
	memset(&so[RTAX_DST], 0, sizeof(struct sockaddr));
	memset(&so[RTAX_NETMASK], 0, sizeof(struct sockaddr));
	sa->sa_family = (type == 4 ? AF_INET : AF_INET6);
	sa->sa_len = (type == 4 ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
	sa = (struct sockaddr *)&so[RTAX_GATEWAY];
	if (type == 4) {
		u_long val;
		struct sockaddr_in *sin;

		sa->sa_family = AF_INET;
		sa->sa_len = sizeof(struct sockaddr_in);
		sin = (struct sockaddr_in *)(void *)sa;

		inet_aton(gtw, &sin->sin_addr);
		val = sin->sin_addr.s_addr;
		inet_lnaof(sin->sin_addr);
	} else {
		struct sockaddr_in6 *sin;
		sa->sa_family = AF_INET6;
		sa->sa_len = sizeof(struct sockaddr_in6);
		sin = (struct sockaddr_in6 *)(void *)sa;

		struct addrinfo hints, *res;
		int ecode;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = sa->sa_family;
		hints.ai_socktype = SOCK_DGRAM;
		ecode = getaddrinfo(gtw, NULL, &hints, &res);
		if (ecode != 0 || res->ai_family != AF_INET6 ||
		    res->ai_addrlen != sizeof(struct sockaddr_in6)) {
			freeaddrinfo(res);
			lua_pushnil(L);
			return 1;
		}

		memcpy(sa, res->ai_addr, res->ai_addrlen);
		freeaddrinfo(res);
	}

	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		if (fibnum != 0 && fibnum != fl->fl_num)
			continue;
		error += rtmsg(RTM_ADD, flags, rtm_addrs, &seq, fl->fl_num, so);
	}
	if (error)
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
	return 1;
}

static int libknet_route_del(lua_State *L)
{
	int type = luaL_optnumber(L, 1, 4);
	if (type != 4 && type != 6) {
		lua_pushnil(L);
		return 1;
	}

	struct sockaddr_storage so[RTAX_MAX];

	TAILQ_INIT(&fibl_head);
	int error = 0;

	if (fill_fibs(&fibl_head) != 0) {
		lua_pushnil(L);
		return 1;
	}

	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC | RTF_PINNED;
	rtm_addrs = RTA_DST | RTA_NETMASK;

	struct sockaddr *sa;

	memset(&so[RTAX_DST], 0, sizeof(struct sockaddr));
	memset(&so[RTAX_NETMASK], 0, sizeof(struct sockaddr));
	sa = (struct sockaddr *)&so[RTAX_DST];
	sa->sa_family = (type == 4 ? AF_INET : AF_INET6);
	sa->sa_len = (type == 4 ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));

	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		error += rtmsg(RTM_DELETE, flags, rtm_addrs, &seq, fl->fl_num, so);
	}

	if (error)
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
	return 1;
}

struct ifmap_entry {
	char ifname[IFNAMSIZ];
};
static int libknet_route_get(lua_State *L)
{
	int ifmap_size = 0;
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_dl *sdl;
	struct ifmap_entry *ifmap = NULL;
	int ifindex = 0, size;

	if (getifaddrs(&ifap) != 0) {
		lua_pushnil(L);
		return 1;
	}
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		ifindex = sdl->sdl_index;

		if (ifindex >= ifmap_size) {
			size = roundup(ifindex + 1, 32) *
			    sizeof(struct ifmap_entry);
			if ((ifmap = realloc(ifmap, size)) == NULL) {
				freeifaddrs(ifap);
				lua_pushnil(L);
				return 1;
			}
			memset(&ifmap[ifmap_size], 0,
			    size - ifmap_size *
			    sizeof(struct ifmap_entry));

			ifmap_size = roundup(ifindex + 1, 32);
		}

		if (*ifmap[ifindex].ifname != '\0')
			continue;

		strlcpy(ifmap[ifindex].ifname, ifa->ifa_name, IFNAMSIZ);
	}

	freeifaddrs(ifap);


	int mib[7];
	int fibnum, numfibs, af = AF_UNSPEC;
	size_t intsize, needed;
	intsize = sizeof(int);

	if (sysctlbyname("net.my_fibnum", &fibnum, &intsize, NULL, 0) == -1)
		fibnum = 0;
	if (sysctlbyname("net.fibs", &numfibs, &intsize, NULL, 0) == -1)
		numfibs = 1;


	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = af;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = fibnum;

	if (sysctl(mib, nitems(mib), NULL, &needed, NULL, 0) < 0) {
		free(ifmap);
		lua_pushnil(L);
		return 1;
	}

	char *buf, *next, *lim;
	struct rt_msghdr *rtms;
	struct sockaddr *sa;

	if ((buf = malloc(needed)) == NULL) {
		free(ifmap);
		lua_pushnil(L);
		return 1;
	}

	if (sysctl(mib, nitems(mib), buf, &needed, NULL, 0) < 0) {
		free(ifmap);
		free(buf);
		lua_pushnil(L);
		return 1;
	}

	lim  = buf + needed;

    lua_newtable( L );
	lua_pushstring(L, "ipv4");
	lua_newtable(L);
	int count = 0;
	for (next = buf; next < lim; next += rtms->rtm_msglen) {
		rtms = (struct rt_msghdr *)next;
		if (rtms->rtm_version != RTM_VERSION)
			continue;
		struct sockaddr *sa, *addr[RTAX_MAX];
		sa = (struct sockaddr *)(rtms + 1);

		for (int i = 0; i < RTAX_MAX; i++) {
			if (rtms->rtm_addrs & (1 << i))
				addr[i] = sa;
			sa = (struct sockaddr *)((char *)sa + SA_SIZE(sa));
		}
		if (((struct sockaddr_in *)addr[RTAX_DST])->sin_family == AF_INET) {
			lua_pushinteger(L, ++count);
			lua_newtable(L);
			if (((struct sockaddr_in *)addr[RTAX_DST])->sin_addr.s_addr == INADDR_ANY ){
				char *ip = inet_ntoa(((struct sockaddr_in *)addr[RTAX_GATEWAY])->sin_addr);
				l_pushtablestring(L, "dest", "default");
				l_pushtablestring(L, "gtw", ip);
				l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
			} else if (rtms->rtm_flags & RTF_HOST) {
				char nline[INET_ADDRSTRLEN];
				in_addr_t in = ((struct sockaddr_in *)addr[RTAX_DST])->sin_addr.s_addr;
				inet_ntop(AF_INET, &in, nline, sizeof(nline));
				char line[NI_MAXHOST];
				if (!getnameinfo(addr[RTAX_GATEWAY], addr[RTAX_GATEWAY]->sa_len, line, sizeof(line),
					    NULL, 0, 0)) {
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				} else {
					in_addr_t in1 = ((struct sockaddr_in *)addr[RTAX_GATEWAY])->sin_addr.s_addr;
					inet_ntop(AF_INET, &in1, line, sizeof(line));
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				}
			} else {
				char nline[NI_MAXHOST];
                in_addr_t in = ((struct sockaddr_in *)addr[RTAX_DST])->sin_addr.s_addr;
                inet_ntop(AF_INET, &in, nline, sizeof(nline));
				in_addr_t mask1 = ((struct sockaddr_in *)addr[RTAX_NETMASK])->sin_addr.s_addr;
				u_long mask = ntohl(mask1);
				int b, i;
				if (mask != 0) {
					i = 0;
					for (b = 0; b < 32; b++)
						if (mask & (1 << b)) {
							int bb;

							i = b;
							for (bb = b+1; bb < 32; bb++)
								if (!(mask & (1 << bb))) {
									i = -1;	/* noncontig */
									break;
								}
							break;
						}
					if (i == -1)
						snprintf(nline, sizeof(nline), "%s&0x%lx", nline, mask);
					else
						snprintf(nline, sizeof(nline), "%s/%d", nline, 32 - i);
				}
				char line[NI_MAXHOST];
				if (!getnameinfo(addr[RTAX_GATEWAY], addr[RTAX_GATEWAY]->sa_len, line, sizeof(line),
					    NULL, 0, 0)) {
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				} else {
	                in_addr_t in1 = ((struct sockaddr_in *)addr[RTAX_GATEWAY])->sin_addr.s_addr;
	                inet_ntop(AF_INET, &in1, line, sizeof(line));
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				}
			}
			lua_settable(L, -3);
		}
	}
	lua_settable(L, -3);
	lua_pushstring(L, "ipv6");
	lua_newtable(L);
	count = 0;
	for (next = buf; next < lim; next += rtms->rtm_msglen) {
		rtms = (struct rt_msghdr *)next;
		if (rtms->rtm_version != RTM_VERSION)
			continue;
		struct sockaddr *sa, *addr[RTAX_MAX];
		sa = (struct sockaddr *)(rtms + 1);

		for (int i = 0; i < RTAX_MAX; i++) {
			if (rtms->rtm_addrs & (1 << i))
				addr[i] = sa;
			sa = (struct sockaddr *)((char *)sa + SA_SIZE(sa));
		}

		if(((struct sockaddr_in *)addr[RTAX_DST])->sin_family == AF_INET6) {
			struct sockaddr_in6 *mask = ((struct sockaddr_in6 *)addr[RTAX_NETMASK]);
			const u_char masktolen[256] = {
				[0xff] = 8 + 1,
				[0xfe] = 7 + 1,
				[0xfc] = 6 + 1,
				[0xf8] = 5 + 1,
				[0xf0] = 4 + 1,
				[0xe0] = 3 + 1,
				[0xc0] = 2 + 1,
				[0x80] = 1 + 1,
				[0x00] = 0 + 1,
			};
			u_char *p, *lim;
			bool illegal = false;
			u_char masklen;
			if (mask) {
				p = (u_char *)&mask->sin6_addr;
				for (masklen = 0, lim = p + 16; p < lim; p++) {
					if (masktolen[*p] > 0) {
						masklen += (masktolen[*p] - 1);
					} else
						illegal = true;
				}
				if (illegal)
					continue;
			} else {
				masklen = 128;
			}
			lua_pushinteger(L, ++count);
			lua_newtable(L);
			if (masklen == 0 && IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)addr[RTAX_GATEWAY])->sin6_addr) ){
				char ip[NI_MAXHOST];
				inet_ntop(AF_INET6, &((struct sockaddr_in6 *)addr[RTAX_GATEWAY])->sin6_addr.s6_addr, ip, sizeof(ip));

				l_pushtablestring(L, "dest", "default");
				l_pushtablestring(L, "gtw", ip);
				l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
			} else if (rtms->rtm_flags & RTF_HOST) {
				char nline[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, &((struct sockaddr_in6 *)addr[RTAX_DST])->sin6_addr.s6_addr, nline, sizeof(nline));
				char line[NI_MAXHOST];
				if (!getnameinfo(addr[RTAX_GATEWAY], addr[RTAX_GATEWAY]->sa_len, line, sizeof(line),
					    NULL, 0, 0)) {
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				} else {
					inet_ntop(AF_INET6, &((struct sockaddr_in *)addr[RTAX_GATEWAY])->sin_addr.s_addr, line, sizeof(line));
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				}
			} else {
				char nline[NI_MAXHOST];
				inet_ntop(AF_INET6, &((struct sockaddr_in6 *)addr[RTAX_DST])->sin6_addr.s6_addr, nline, sizeof(nline));
				snprintf(nline, sizeof(nline), "%s/%d", nline, masklen);
				char line[NI_MAXHOST];
				if (!getnameinfo(addr[RTAX_GATEWAY], addr[RTAX_GATEWAY]->sa_len, line, sizeof(line),
					    NULL, 0, NI_NUMERICHOST)) {
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				} else {
					inet_ntop(AF_INET6, &((struct sockaddr_in *)addr[RTAX_GATEWAY])->sin_addr.s_addr, line, sizeof(line));
					l_pushtablestring(L, "dest", nline);
					l_pushtablestring(L, "gtw", line);
					l_pushtablestring(L, "ifname", ifmap[rtms->rtm_index].ifname);
				}
			}
			lua_settable(L, -3);
		}
	}
	lua_settable(L, -3);

	free(ifmap);
	free(buf);
	return 1;
}


static int libknet_create(lua_State *L)
{
	const char *name = luaL_optstring(L, 1, NULL);

	if (name == NULL) {
		lua_pushnil(L);
		return 1;
	}

	struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    (void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
            printf("%s\n", strerror(errno));
            return 1;
        }
    }

	int error = 0;
    error = ioctl(s, SIOCIFCREATE2, &ifr);

	if (error) {
        printf("%s\n", strerror(errno));
		lua_pushnil(L);
	} else {
		lua_pushstring(L, ifr.ifr_name);
	}
	return 1;
}

static int libknet_destroy(lua_State *L)
{
	const char *name = luaL_optstring(L, 1, NULL);

	if (name == NULL) {
		lua_pushnil(L);
		return 1;
	}

	struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    (void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
            printf("%s\n", strerror(errno));
            return 1;
        }
    }

	int error = 0;
    error = ioctl(s, SIOCIFDESTROY, &ifr);

	if (error) {
        printf("%s\n", strerror(errno));
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	return 1;
}

static int libknet_vnet(lua_State *L)
{
	const char *name = luaL_optstring(L, 1, NULL);

	if (name == NULL) {
		lua_pushnil(L);
		return 1;
	}

	struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    (void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

    if (lua_isstring(L, 2)) {
    	const char *jname = luaL_optstring(L, 2, NULL);
    	if (jname == NULL) {
			lua_pushnil(L);
			return 1;
    	}
    	ifr.ifr_jid = jail_getid(jname);
    } else if (lua_isnumber(L, 2)) {
    	int jid = luaL_optnumber(L, 2, 0);
    	if (jid <= 0) {
			lua_pushnil(L);
			return 1;
    	}
    	ifr.ifr_jid = jid;
    }

	int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
            printf("%s\n", strerror(errno));
            return 1;
        }
    }


	int error = 0;
    error = ioctl(s, SIOCSIFVNET, &ifr);

	if (error) {
        printf("%s\n", strerror(errno));
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	return 1;
}

static int libknet_vnetd(lua_State *L)
{
	const char *name = luaL_optstring(L, 1, NULL);

	if (name == NULL) {
		lua_pushnil(L);
		return 1;
	}

	struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    (void) strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

    if (lua_isstring(L, 2)) {
    	const char *jname = luaL_optstring(L, 2, NULL);
    	if (jname == NULL) {
			lua_pushnil(L);
			return 1;
    	}
    	ifr.ifr_jid = jail_getid(jname);
    } else if (lua_isnumber(L, 2)) {
    	int jid = luaL_optnumber(L, 2, 0);
    	if (jid <= 0) {
			lua_pushnil(L);
			return 1;
    	}
    	ifr.ifr_jid = jid;
    }

	int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
            printf("%s\n", strerror(errno));
            return 1;
        }
    }


	int error = 0;
    error = ioctl(s, SIOCSIFRVNET, &ifr);

	if (error) {
        printf("%s\n", strerror(errno));
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	return 1;
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
	struct rt_msghdr *arp_rtm = &m_rtmsg.m_rtm;
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
	arp_rtm->rtm_flags = flags;
	arp_rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		return NULL;
	case RTM_ADD:
		arp_rtm->rtm_addrs |= RTA_GATEWAY;
		arp_rtm->rtm_rmx.rmx_expire = expire_time;
		arp_rtm->rtm_inits = RTV_EXPIRE;
		arp_rtm->rtm_flags |= (RTF_HOST | RTF_STATIC | RTF_LLDATA);
		if (doing_proxy) {
			arp_rtm->rtm_addrs |= RTA_NETMASK;
			arp_rtm->rtm_flags &= ~RTF_HOST;
		}
		/* FALLTHROUGH */
	case RTM_GET:
		arp_rtm->rtm_addrs |= RTA_DST;
	}
#define NEXTADDR1(w, s)						\
	do {							\
		if ((s) != NULL && arp_rtm->rtm_addrs & (w)) {	\
			bcopy((s), cp, sizeof(*(s)));		\
			cp += SA_SIZE(s);			\
		}						\
	} while (0)

	NEXTADDR1(RTA_DST, dst);
	NEXTADDR1(RTA_GATEWAY, sdl);
	NEXTADDR1(RTA_NETMASK, som);

	arp_rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = arp_rtm->rtm_msglen;
	arp_rtm->rtm_seq = ++seq;
	arp_rtm->rtm_type = cmd;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			printf("%s\n", "writing to routing socket");
			return (NULL);
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (arp_rtm->rtm_seq != seq || arp_rtm->rtm_pid != pid));
	if (l < 0)
		printf("%s\n", "read from routing socket");

	return (arp_rtm);
}

static void
print_entry(struct sockaddr_dl *sdl,
	struct sockaddr_in *addr, struct rt_msghdr *arp_rtm, lua_State *L, char *rifname)
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

	if (arp_rtm->rtm_rmx.rmx_expire == 0) {
		l_pushtableboolean(L, "permanent", 1);
	} else {
		static struct timespec tp;
		if (tp.tv_sec == 0)
			clock_gettime(CLOCK_MONOTONIC, &tp);
		if ((expire_time = arp_rtm->rtm_rmx.rmx_expire - tp.tv_sec) > 0) {
			l_pushtableinteger(L, "expire_secs", (int)expire_time);
		} else {
			l_pushtableboolean(L, "expired", 1);
		}
	}

	if (arp_rtm->rtm_flags & RTF_ANNOUNCE)
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
	struct rt_msghdr *arp_rtm;
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
	for (next = buf; next < lim; next += arp_rtm->rtm_msglen) {
		arp_rtm = (struct rt_msghdr *)next;
		sin2 = (struct sockaddr_in *)(arp_rtm + 1);
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
		(*action)(sdl, sin2, arp_rtm, L, rifname);
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
	struct rt_msghdr *arp_rtm;
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
		arp_rtm = arp_rtmsg(RTM_GET, dst, &sdl_m, flags, doing_proxy);
		if (arp_rtm == NULL) {
			return (1);
		}
		addr = (struct sockaddr_in *)(arp_rtm + 1);
		sdl = (struct sockaddr_dl *)(SA_SIZE(addr) + (char *)addr);

		if (sdl->sdl_family == AF_LINK &&
		    !(arp_rtm->rtm_flags & RTF_GATEWAY) &&
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
	arp_rtm->rtm_flags |= RTF_LLDATA;
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
	struct sockaddr_in *addr, struct rt_msghdr *arp_rtm, lua_State *L, char *rifname)
{
	char ip[20];

	if (arp_rtm->rtm_flags & RTF_PINNED)
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

static int
fill_sockaddr_storage(int id, char *addr, struct sockaddr_storage *so)
{
	struct sockaddr_in *sin;
	struct sockaddr *sa;
	u_long val;

	sa = (struct sockaddr *)&so[id];
	sa->sa_family = AF_INET;
	sa->sa_len = sizeof(struct sockaddr_in);
	sin = (struct sockaddr_in *)(void *)sa;
	if (strncmp(addr, "0.0.0.0", 7) == 0) 
		return 0;
	char *q;
	q = strchr(addr,'/');

	if (q != NULL && id == RTAX_DST) {				// net
		*q = '\0';
		val = inet_network(addr);
		u_long mask = strtoul(q+1, 0, 0);
		if (val > 0)
			while ((val & 0xff000000) == 0)
				val <<= 8;

		mask = 0xffffffff << (32 - mask);
		sin->sin_addr.s_addr = htonl(val);
		((struct sockaddr_in *)&so[RTAX_NETMASK])->sin_addr.s_addr = htonl(mask);
		((struct sockaddr_in *)&so[RTAX_NETMASK])->sin_len = sizeof(struct sockaddr_in);
		((struct sockaddr_in *)&so[RTAX_NETMASK])->sin_family = AF_INET;
		*q = '/';
	} else {										// gw/host
		inet_aton(addr, &sin->sin_addr);
		val = sin->sin_addr.s_addr;
		inet_lnaof(sin->sin_addr);
	}

	return 0;
}

static int libknet_route_add_static4(lua_State *L)
{
	if (lua_gettop(L) < 3) {
		lua_pushnil(L);
	    return 1;
	}
	int type = luaL_optnumber(L, 1, -1);   //net - 0, host - 1
	const char *dest = luaL_optstring(L, 2, NULL);
	const char *gtw = luaL_optstring(L, 3, NULL);
	int fibnum = luaL_optnumber(L, 4, 0);


	if ((type != 0 && type != 1) || dest == NULL || gtw == NULL) {
		lua_pushnil(L);
	    return 1;
	}

	if (type == 0 && strchr(dest, '/') == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	struct sockaddr_storage so[RTAX_MAX];
	memset(so, 0, sizeof(struct sockaddr_storage) * RTAX_MAX);
	int error = 0;
	TAILQ_INIT(&fibl_head);
	if (fill_fibs(&fibl_head) != 0) {
		lua_pushnil(L);
	    return 1;
	}
	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	rtm_addrs = RTA_DST | RTA_GATEWAY;
	if (type == 1) {
		flags |= RTF_HOST;
	} else {
		rtm_addrs |= RTA_NETMASK;
	}

	fill_sockaddr_storage(RTAX_DST, (char *)dest, so);
	fill_sockaddr_storage(RTAX_GATEWAY, (char *)gtw, so);
	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		if (fibnum != 0 && fibnum != fl->fl_num)
			continue;
		error += rtmsg(RTM_ADD, flags, rtm_addrs, &seq, fl->fl_num, so);
	}
	if (error){
		lua_pushnil(L);
	    return 1;
	}

	lua_pushboolean(L, 1);
    return 1;
}

static int libknet_route_del_static4(lua_State *L)
{
	if (lua_gettop(L) != 3) {
		lua_pushnil(L);
	    return 1;
	}
	int type = luaL_optnumber(L, 1, -1);   //net - 0, host - 1
	const char *dest = luaL_optstring(L, 2, NULL);
	const char *gtw = luaL_optstring(L, 3, NULL);

	if ((type != 0 && type != 1) || dest == NULL || gtw == NULL) {
		lua_pushnil(L);
	    return 1;
	}

	if (type == 0 && strchr(dest, '/') == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	struct sockaddr_storage so[RTAX_MAX];
	memset(so, 0, sizeof(struct sockaddr_storage) * RTAX_MAX);
	int error = 0;
	TAILQ_INIT(&fibl_head);
	if (fill_fibs(&fibl_head) != 0) {
		lua_pushnil(L);
	    return 1;
	}
	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC | RTF_PINNED;
	rtm_addrs = RTA_DST | RTA_GATEWAY;
	if (type == 1) {
		flags |= RTF_HOST;
	} else {
		rtm_addrs |= RTA_NETMASK;
	}

	fill_sockaddr_storage(RTAX_DST, (char *)dest, so);
	fill_sockaddr_storage(RTAX_GATEWAY, (char *)gtw, so);
	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		error += rtmsg(RTM_DELETE, flags, rtm_addrs, &seq, fl->fl_num, so);
	}
	if (error) {
		lua_pushnil(L);
	    return 1;
	}

	lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg knet[] = {
	{"create", libknet_create},
	{"destroy", libknet_destroy},

	{"vnet", libknet_vnet},
	{"vnetd", libknet_vnetd},

    {"setip", libknet_setip},
    {"delip", libknet_delip},
    {"clear", libknet_clear},
    {"rename", libknet_rename},
    {"setmac", libknet_setmac},
    {"ifcap", libknet_ifcap},
    {"flags", libknet_flags},
    {"list", libknet_list},
    {"info", libknet_info},

    {"route_add", libknet_route_add},
    {"route_del", libknet_route_del},
    {"route_get", libknet_route_get},

    {"route_add_static4", libknet_route_add_static4},
    {"route_del_static4", libknet_route_del_static4},

    {"arp_get", libluaarp_get},
    {"arp_delete", libluaarp_delete},
    {"arp_flush", libluaarp_flush},
    {NULL, NULL}
};

#ifdef STATIC
LUAMOD_API int luaopen_libluaknet(lua_State *L)
#else
LUALIB_API int luaopen_libluaknet(lua_State *L)
#endif
{
    luaL_newlib(L, knet);
    return 1;
}