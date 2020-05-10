#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <jail.h>
#include <base.h>

int jail_main(int argc, char **argv);
int jexec_main(int argc, char *argv[]);

static void l_pushtablestring(lua_State* L, char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static int libjail_start(lua_State *L)
{
	const char *file = luaL_optstring(L, 1, NULL);
	if (file != NULL) {
		pid_t child;
		child = fork();
		if (!child) {
			char *list[5];
			list[0] = "jail";
			list[1] = "-c";
			list[2] = "-f";
			list[3] = (char *)file;
			list[4] = NULL;
			int list_count = 4;
			jail_main(list_count, list);
		} else {
			int status;
			if (waitpid(child, &status, 0) < 0 || status) {
				lua_pushnil(L);
				return 1;
	        }
			lua_pushboolean(L, 1);
			return 1;
		}
	}

	lua_pushnil(L);
    return 1;
}

static int libjail_stop(lua_State *L)
{
	const char *name = luaL_optstring(L, 1, NULL);
	const char *file = luaL_optstring(L, 2, NULL);
	if (name != NULL) {
		pid_t child;
		child = fork();
		if (!child) {
			char *list[6];
			int ac = 0;
			list[ac++] = "jail";
			list[ac++] = "-r";
			if (file != NULL) {
				list[ac++] = "-f";
				list[ac++] = (char *)file;
			}
			list[ac++] = (char *)name;
			list[ac] = NULL;
			jail_main(ac, list);
		} else {
			int status;
			if (waitpid(child, &status, 0) < 0 || status) {
				lua_pushnil(L);
				return 1;
	        }
			lua_pushboolean(L, 1);
			return 1;
		}
	}

	lua_pushnil(L);
    return 1;
}

static int libjail_jexec(lua_State *L)
{
	const char *name = luaL_optstring(L, 1, NULL);
	const char *cmd = luaL_optstring(L, 2, NULL);
	if (name != NULL) {
		pid_t child;
		child = fork();
		if (!child) {
			char *list[BASE_EXEC_MAX_ARGS];
			int ac = 0;
			list[ac++] = "jexec";
			list[ac++] = (char *)name;
			parse_line_quoted((char *)cmd, list, BASE_EXEC_MAX_ARGS, &ac, " ");
			list[ac] = NULL;
			jexec_main(ac, list);
			_exit(-1);
		} else {
			int status;
			if (waitpid(child, &status, 0) < 0 || status) {
				lua_pushnil(L);
				return 1;
	        }
			lua_pushboolean(L, 1);
			return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libjail_jls(lua_State *L)
{
	struct jailparam params[6];
	int ljid = 0;
	jailparam_init(&params[0], "jid");
	jailparam_init(&params[1], "name");
	jailparam_init(&params[2], "ip4.addr");
	jailparam_init(&params[3], "host.hostname");
	jailparam_init(&params[4], "path");
	jailparam_init(&params[5], "lastjid");
	jailparam_import_raw(&params[5], &ljid, sizeof(ljid));
	int count = 0;
	lua_newtable(L);
	for (ljid = 0; (ljid = jailparam_get(params, 6, 0)) >= 0;) {
		count++;
		lua_pushinteger(L, *(int *)params[0].jp_value);
		lua_newtable(L);
		l_pushtablestring(L, "name", (char *)params[1].jp_value);
		l_pushtablestring(L, "ip", params[2].jp_valuelen == 0 ? "-" : (char *)params[2].jp_value);
		l_pushtablestring(L, "hostname", (char *)params[3].jp_value);
		l_pushtablestring(L, "path", (char *)params[4].jp_value);
		lua_settable(L, -3);
	}
	jailparam_free(params, 6);
	if (!count) {
		lua_pop(L, 1);
		lua_pushnil(L);
	}

	return 1;
}

static const luaL_Reg luajail[] = {
    {"start", libjail_start},
    {"stop", libjail_stop},
    {"jexec", libjail_jexec},
    {"jls", libjail_jls},
    {NULL, NULL}
};


#ifdef STATIC
LUAMOD_API int luaopen_libluajail(lua_State *L)
#else
LUALIB_API int luaopen_libluajail(lua_State *L)
#endif
{
    luaL_newlib(L, luajail);
    return 1;
}
