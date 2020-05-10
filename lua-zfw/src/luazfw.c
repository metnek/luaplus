#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include <kenv.h>
#include <zfw.h>

static void l_pushtablestring(lua_State* L, char* key, char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static void l_pushtableboolean(lua_State* L, char* key, int value)
{
    lua_pushstring(L, key);
    lua_pushboolean(L, value);
    lua_settable(L, -3);
}

static void l_pushtablenumber(lua_State* L, char* key, int value)
{
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}

static void l_pushtableinteger(lua_State* L, char* key, int value)
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

static void l_pushtable_numkey_string(lua_State* L, int key, char* value)
{
    lua_pushinteger(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_number(lua_State* L, int key, int value)
{
    lua_pushinteger(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_integer(lua_State* L, int key, int value)
{
    lua_pushinteger(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_boolean(lua_State* L, int key, int value)
{
    lua_pushinteger(L, key);
    lua_pushboolean(L, value);
    lua_settable(L, -3);
}

static void l_pushtable_numkey_nil(lua_State* L, int key)
{
    lua_pushinteger(L, key);
    lua_pushnil(L);
    lua_settable(L, -3);
}

static int libluazfw_rules_list(lua_State *L)
{
    int show_stats = luaL_optnumber(L, 1, 0);
    int set = luaL_optnumber(L, 2, -1);
	zfw_rule_list_t *list = zfw_rule_list(set, show_stats);
	if (list == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	if (list->count == 0) {
		zfw_rule_list_free(list);
		lua_pushnil(L);
	    return 1;
	}

	int i = 0;

	lua_newtable(L);
	for (i = 0; i < list->count; i++) {
		lua_pushinteger(L, i + 1);
		lua_newtable(L);

		l_pushtableinteger(L, "set", list->rules[i]->set);
		l_pushtableinteger(L, "rule_num", list->rules[i]->rule_num);
		l_pushtableboolean(L, "disabled", list->rules[i]->is_disabled);
		l_pushtablestring(L, "rule", list->rules[i]->body);
	    lua_settable(L, -3);
	}

	zfw_rule_list_free(list);
	if (i == 0) {
		lua_pop(L, 1);
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_rule_add(lua_State *L)
{
	if (lua_gettop(L) != 1) {
		lua_pushnil(L);
	    return 1;
	}

	int ret = 0;
	if (lua_istable(L, 1)) {

		lua_pushnil(L);
		while(lua_next(L, -2)) {
			if (lua_isstring(L, -1)) {
				const char *rule = luaL_optstring(L, -1, NULL);
				if (rule != NULL) {
					char buf[strlen(rule) + 5];
					snprintf(buf, sizeof(buf), "add %s", rule);
					printf("%s\n", buf);
					ret = zfw_rule_add(buf);
				} else {
					ret = 1;
				}
				if (ret)
					break;
			}
			lua_pop(L, 1);
		}
	} else if (lua_isstring(L, 1)) {
		const char *rule = luaL_optstring(L, 1, NULL);
		if (rule != NULL) {
			char buf[strlen(rule) + 5];
			snprintf(buf, sizeof(buf), "add %s", rule);

			ret = zfw_rule_add(buf);
		} else {
			ret = 1;
		}
	} else {
		lua_pushnil(L);
	    return 1;
	}

	if (ret) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
    return 1;
}

static int libluazfw_rule_delete(lua_State *L)
{
	int set = luaL_optnumber(L, 1, -1);
	int num_b = luaL_optnumber(L, 2, -1);
	int num_e = luaL_optnumber(L, 3, -1);

	if (set < 0 || set >= ZFW_SET_MAX_NUM) {
		lua_pushnil(L);
	    return 1;
	}

	if (num_b <= 0 || num_b > ZFW_RULE_MAX_NUM) {
		lua_pushnil(L);
	    return 1;
	}

	if (num_e < 0 || num_e > ZFW_RULE_MAX_NUM || num_b > num_e) {
		num_e = num_b;
	}

	int ret = zfw_rule_delete(set, num_b, num_e);
	if (ret) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}

    return 1;
}

static int libluazfw_rule_move(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	int new_set = luaL_optnumber(L, 2, -1);

	if (new_set < 0 || new_set >= ZFW_SET_MAX_NUM || num <= 0 || num > ZFW_RULE_MAX_NUM) {
		lua_pushnil(L);
	    return 1;
	}

	int ret = zfw_rule_move_to_set(num, new_set);

	if (ret) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}

    return 1;
}

static int libluazfw_tables_list(lua_State *L)
{
	zfw_table_list_t *table_list = zfw_table_list();
	if (table_list == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	int i = 0;
	lua_newtable(L);
	for (i = 0; i < table_list->count; i++) {
		lua_pushstring(L, table_list->tables[i]->table_name);
		lua_newtable(L);
		if (table_list->tables[i]->entries_size) {
			int j = 0;
			char *tmp = strtok(table_list->tables[i]->table_entries,  ",");
			while (tmp != NULL) {
				j++;
				l_pushtable_numkey_string(L, j, tmp);
				tmp = strtok(NULL,  ",");
			}
		}
		lua_settable(L, -3);
	}
	zfw_table_list_free(table_list);
	if (!i) {
		lua_pop(L, 1);
		lua_pushnil(L);
	    return 1;
	}
    return 1;
}

static int libluazfw_tables_destroy(lua_State *L)
{
	int ret = zfw_table_destroy_all();
	if (ret) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}

    return 1;
}

static int libluazfw_table_create(lua_State *L)
{
	const char *tablename = luaL_optstring(L, 1, NULL);
	if (tablename != NULL) {
		int ret;
		ret = zfw_table_create((char *)tablename, NULL, 0);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_table_destroy(lua_State *L)
{
	const char *tablename = luaL_optstring(L, 1, NULL);
	if (tablename != NULL) {
		int ret;
		ret = zfw_table_destroy((char *)tablename);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_table_add(lua_State *L)
{
	const char *tablename = luaL_optstring(L, 1, NULL);
	const char *entry = luaL_optstring(L, 2, NULL);
	if (tablename != NULL && entry != NULL) {
		int ret;
		ret = zfw_table_entry_add((char *)tablename, (char *)entry);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_table_delete(lua_State *L)
{
	const char *tablename = luaL_optstring(L, 1, NULL);
	const char *entry = luaL_optstring(L, 2, NULL);
	if (tablename != NULL && entry != NULL) {
		int ret;
		ret = zfw_table_entry_delete((char *)tablename, (char *)entry);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_table_get(lua_State *L)
{
	const char *tablename = luaL_optstring(L, 1, NULL);
	if (tablename != NULL) {
		zfw_table_t *table = zfw_table_get((char *)tablename);
		if (table != NULL) {
			lua_newtable(L);
			if (table->entries_size) {
				char *tmp = strtok(table->table_entries,  ",");
				int i = 0;
				while (tmp != NULL) {
					i++;
					l_pushtable_numkey_string(L, i, tmp);
					tmp = strtok(NULL,  ",");
				}
			}
			zfw_table_free(table);
			return 1;
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_table_flush(lua_State *L)
{
	const char *tablename = luaL_optstring(L, 1, NULL);
	if (tablename != NULL) {
		int ret;
		ret = zfw_table_flush((char *)tablename);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_set_delete(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	if (num >= 0 && num < ZFW_SET_MAX_NUM) {
		int ret;
		ret = zfw_set_delete(num);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_set_list(lua_State *L)
{
	unsigned char *list = zfw_set_list();
	if (list == NULL) {
		lua_pushnil(L);
	} else {
		lua_newtable(L);
	    for (int i = 0; i < ZFW_SET_MAX_NUM; i++) {
			l_pushtable_numkey_boolean(L, i, list[i]);
	    }
	}
    return 1;
}

static int libluazfw_set_enable(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	if (num >= 0 && num < ZFW_SET_MAX_NUM) {
		int ret;
		ret = zfw_set_enable(num);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_set_disable(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	if (num >= 0 && num < ZFW_SET_MAX_NUM) {
		int ret;
		ret = zfw_set_disable(num);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_set_move(lua_State *L)
{
	int from = luaL_optnumber(L, 1, -1);
	int to = luaL_optnumber(L, 2, -1);
	if ((from >= 0 && from < ZFW_SET_MAX_NUM) && (to >= 0 && to < ZFW_SET_MAX_NUM)) {
		int ret;
		ret = zfw_set_move(from, to);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_pipe_delete(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	if (num > 0) {
		int ret;
		ret = zfw_pipe_delete(ZFW_PIPE, num);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_queue_delete(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	if (num > 0) {
		int ret;
		ret = zfw_pipe_delete(ZFW_QUEUE, num);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_nat_delete(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	if (num > 0) {
		int ret;
		ret = zfw_nat_delete(num);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_pipe_config(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	const char *str = luaL_optstring(L, 2, NULL);
	if (num > 0 && str != NULL) {
		int ret;
		ret = zfw_pipe_config(ZFW_PIPE, num, (char *)str);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_queue_config(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	const char *str = luaL_optstring(L, 2, NULL);
	if (num > 0 && str != NULL) {
		int ret;
		ret = zfw_pipe_config(ZFW_QUEUE, num, (char *)str);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_nat_config(lua_State *L)
{
	int num = luaL_optnumber(L, 1, -1);
	const char *str = luaL_optstring(L, 2, NULL);
	if (num > 0 && str != NULL) {
		int ret;
		ret = zfw_nat_config(num, (char *)str);
		if (ret) {
			lua_pushnil(L);
		} else {
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
    return 1;
}

static int libluazfw_nat_show(lua_State *L)
{
  zfw_nat_show();
  lua_pushboolean(L, 1);
  return 1;
}

static const luaL_Reg luazfw[] = {
    {"rules_list", libluazfw_rules_list},
    {"rule_add", libluazfw_rule_add},
    {"rule_delete", libluazfw_rule_delete},
    {"rule_move", libluazfw_rule_move},

    {"tables_list", libluazfw_tables_list},
    {"tables_destroy", libluazfw_tables_destroy},
    {"table_create", libluazfw_table_create},
    {"table_destroy", libluazfw_table_destroy},
    {"table_add", libluazfw_table_add},
    {"table_delete", libluazfw_table_delete},
    {"table_get", libluazfw_table_get},
    {"table_flush", libluazfw_table_flush},

    {"set_delete", libluazfw_set_delete},
    {"set_list", libluazfw_set_list},
    {"set_enable", libluazfw_set_enable},
    {"set_disable", libluazfw_set_disable},
    {"set_move", libluazfw_set_move},

    {"pipe_delete", libluazfw_pipe_delete},
    {"queue_delete", libluazfw_queue_delete},
    {"nat_delete", libluazfw_nat_delete},
    {"pipe_config", libluazfw_pipe_config},
    {"queue_config", libluazfw_queue_config},
    {"nat_config", libluazfw_nat_config},
    {"nat_show", libluazfw_nat_show},
    {NULL, NULL}
};

#ifdef STATIC
LUAMOD_API int luaopen_libluazfw(lua_State *L)
#else
LUALIB_API int luaopen_libluazfw(lua_State *L)
#endif
{
    luaL_newlib(L, luazfw);
    return 1;
}