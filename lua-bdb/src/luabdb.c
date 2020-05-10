#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <kenv.h>
#include <bdb.h>

static void l_pushtablestring(lua_State* L, char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
}
static void l_pushtableinteger(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

static void l_pushtablenumber(lua_State* L, char* key , int value)
{
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
}
static void l_pushtablenil(lua_State* L, char* key)
{
    lua_pushstring(L, key);
    lua_pushnil(L);
    lua_settable(L, -3);
}
static void l_pushtable_numkey_nil(lua_State* L, int key)
{
    lua_pushinteger(L, key);
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


static int
l_bdb_str_to_obj(lua_State *L, bdb_data_t *data)
{
	int ret = 0;
	bdb_value_t value;
	memset(&value, 0, sizeof(bdb_value_t));


	while ((ret = bdb_parse_next(data, &value)) == 0) {
		if(BDB_COL_TYPE(value) == BDB_COL_STRING) {
			l_pushtablestring(L, value.bdb_vl_colname, value.bdb_vl_s);
		} else if (BDB_COL_TYPE(value) == BDB_COL_LONG) {
			l_pushtableinteger(L, value.bdb_vl_colname, value.bdb_vl_l);
		} else if (BDB_COL_TYPE(value) == BDB_COL_DOUBLE) {
			l_pushtablenumber(L, value.bdb_vl_colname, value.bdb_vl_d);
		} else {
			lua_pushstring(L, value.bdb_vl_colname);
			lua_newtable( L );
			char del[] = BDB_ARRAY_DELIM;
			 if (BDB_COL_TYPE(value) == BDB_COL_ASTRING) {
			 	char *tmp;
			 	char *cur = value.bdb_vl_s;
			 	int is_last_empty = 0;
			 	int i = 1;
			 	while ((tmp = strchr(cur, *del)) != NULL) {
			 		*tmp = '\0';
	                if (tmp == cur) {
	                    l_pushtable_numkey_nil(L, i++);
	                } else {
						l_pushtable_numkey_string(L, i++, cur);
	                }
	                cur = tmp + 1;
	                if (!(*cur)) {
	                    l_pushtable_numkey_nil(L, i++);
	                    is_last_empty = 1;
	                }
			 	}
			 	if (!is_last_empty) {
			 		if (strlen(cur) != 0) {
						l_pushtable_numkey_string(L, i++, cur);
			 		}
			 	}
			} else if (BDB_COL_TYPE(value) == BDB_COL_ADOUBLE) {
			 	int i = 1;
				char *tmp = strtok(value.bdb_vl_s, BDB_ARRAY_DELIM);
				while (tmp != NULL) {
					errno = 0;
					double dvl = strtod(tmp, NULL);
					if (errno) {
						return -1;
					}
					l_pushtable_numkey_number(L, i++, dvl);
					tmp = strtok(NULL, BDB_ARRAY_DELIM);
				}
			} else if (BDB_COL_TYPE(value) == BDB_COL_ALONG) {
			 	int i = 1;
				char *tmp = strtok(value.bdb_vl_s, BDB_ARRAY_DELIM);
				while (tmp != NULL) {
					errno = 0;
					long lvl = strtol(tmp, NULL, 10);
					if (errno) {
						return -1;
					}
					l_pushtable_numkey_integer(L, i++, lvl);
					tmp = strtok(NULL, BDB_ARRAY_DELIM);
				}
			}
		    lua_settable(L, -3);
		}
		if (ret) {
		    lua_settable(L, -3);
			return -1;
		}
	}
	return 0;
}

static int l_bdb_fill_value_string(lua_State* L, bdb_value_t *value)
{
	int first = 1;
    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
 		if (strcmp(lua_tostring(L, -2), value->bdb_vl_colname ) == 0 && 
 			lua_isstring(L, -1) &&
 			first) {
    		value->bdb_vl_s = (char *)lua_tostring(L, -1);
    		first = 0;
 		}
        lua_pop(L, 1);
    }
    if (first)
    	return -1;
    return 0;
}
static int l_bdb_fill_value_int(lua_State* L, bdb_value_t *value)
{
	int first = 1;
    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
 		if (strcmp(lua_tostring(L, -2), value->bdb_vl_colname ) == 0 && 
 			lua_isinteger(L, -1) &&
 			first) {
    		value->bdb_vl_l = lua_tointeger(L, -1);
    		first = 0;
 		}
        lua_pop(L, 1);
    }
    if (first)
    	return -1;
    return 0;
}
static int l_bdb_fill_value_double(lua_State* L, bdb_value_t *value)
{
	int first = 1;
    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
 		if (strcmp(lua_tostring(L, -2), value->bdb_vl_colname ) == 0 && 
 			lua_isnumber(L, -1) &&
 			first) {
    		value->bdb_vl_d = lua_tonumber(L, -1);
    		first = 0;
 		}
        lua_pop(L, 1);
    }
    if (first)
    	return -1;
    return 0;
}
static int l_bdb_fill_value_table(lua_State* L, bdb_value_t *value)
{
	int first = 1;

    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
 		if (strcmp(lua_tostring(L, -2), value->bdb_vl_colname ) == 0 && 
 			lua_istable(L, -1) &&
 			first) {
			char *buf = NULL;
    		first = 0;
 			if (lua_rawlen(L, -1)) {
	 			size_t len = 0;
				lua_pushnil(L);
				while(lua_next(L, -2) != 0) {
		 			if ((*value->bdb_vl_type == BDB_COL_ADOUBLE && lua_isnumber(L, -1)) || 
		 				(*value->bdb_vl_type == BDB_COL_ALONG && lua_isinteger(L, -1)) ||
		 				(*value->bdb_vl_type == BDB_COL_ASTRING && lua_isstring(L, -1))) {
				        if(lua_isinteger(L, -1)) {
					    	buf = bdb_mk_long_array(lua_tointeger(L, -1), buf, &len);
				        } else if (lua_isnumber(L, -1)) {
					    	buf = bdb_mk_double_array(lua_tonumber(L, -1), buf, &len);
				        } else {
					    	buf = bdb_mk_string_array((char *)lua_tostring(L, -1), buf, &len);

				        }
				    }
			        lua_pop(L, 1);
				}
		    	if (buf != NULL) {
					if (len) {
						value->bdb_vl_s = buf;
					} else {
						value->bdb_vl_s = "";
					}
		    	} else {
		    		value->bdb_vl_s = "";
		    	}
 			} else {
 				value->bdb_vl_s = malloc(1);
 				memset(value->bdb_vl_s, 0, 1);
 				first = 0;
 			}
		}
        lua_pop(L, 1);
	}
    if (first) {
    	return -1;
    }
    return 0;

}
static int
l_bdb_obj_to_str(lua_State *L, bdb_data_t *data)
{
 	bdb_column_t *col = NULL;
 	while ((col = bdb_get_next_col(data)) != NULL) {
		bdb_value_t value;
		value.bdb_vl_colname = col->col_name;
		value.bdb_vl_type = col->type;
		int is_table = 0;

		if (*col->type == BDB_COL_STRING) {
    		if (l_bdb_fill_value_string(L, &value)) {
    			free(col);
    			return -1;
    		}
		} else if (*col->type == BDB_COL_LONG) {
    		if (l_bdb_fill_value_int(L, &value)) {
    			free(col);
    			return -1;
    		}
		} else if (*col->type == BDB_COL_DOUBLE) {
    		if (l_bdb_fill_value_double(L, &value)) {
    			free(col);
    			return -1;
    		}
		} else {
    		if (l_bdb_fill_value_table(L, &value)) {
    			free(col);
    			return -1;
    		}
			is_table = 1;
		}
		int ret = bdb_add_next(data, &value);
		if (is_table)
			free(value.bdb_vl_s);
		if (ret) {
	        lua_pop(L, 1);
			free(col);
			return -1;
		}
		free(col);
	}
	return 0;
}

static int libbdb_set(lua_State *L)
{
    const char *dbname = luaL_optstring(L, 1, NULL);
    const char *path = luaL_optstring(L, 2, NULL);
    long key = luaL_optnumber(L, 3, 0);
    if (dbname == NULL) {
	    lua_pushnil(L);
	    return 1;
    }
    
    DB *dbp = NULL;
    if (path == NULL) {
		char path_kenv[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
    	dbp = bdb_open((char *)dbname, path_kenv);
    } else {
    	dbp = bdb_open((char *)dbname, (char *)path);
    }

    if (dbp == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

	bdb_data_t data;
	memset(&data, 0, sizeof(bdb_data_t));
	data.bdb_dt_key = key;
	data.bdb_dt_schema = bdb_get_schema(dbp, &data.bdb_dt_schema_len);

	if (data.bdb_dt_schema == NULL) {
		bdb_close(dbp);
	    lua_pushnil(L);
	    return 1;
	}
	int ret = 0;
	if ((ret = l_bdb_obj_to_str(L, &data)) == 0) {
		bdb_set(dbp, &data);
		free(data.bdb_dt_p);
		lua_pushboolean(L, 1);
	} else {
	    lua_pushnil(L);
	}
	bdb_close(dbp);
    return 1;
}

static int libbdb_del(lua_State *L)
{
    const char *dbname = luaL_optstring(L, 1, NULL);
    const char *path = luaL_optstring(L, 2, NULL);
    long key = luaL_optnumber(L, 3, 0);
    if (dbname == NULL || key == 0) {
	    lua_pushnil(L);
	    return 1;
    }
    DB *dbp = NULL;
    if (path == NULL) {
		char path_kenv[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
    	dbp = bdb_open((char *)dbname, path_kenv);
    } else {
    	dbp = bdb_open((char *)dbname, (char *)path);
    }

    if (dbp == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    if (bdb_del(dbp, key)) {
	    lua_pushnil(L);
    } else {
		lua_pushboolean(L, 1);
    }

    bdb_close(dbp);
    return 1;
}

static int libbdb_get_all(lua_State *L)
{
    const char *dbname = luaL_optstring(L, 1, NULL);
    const char *path = luaL_optstring(L, 2, NULL);
    if (dbname == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    DB *dbp = NULL;
    if (path == NULL) {
		char path_kenv[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
    	dbp = bdb_open((char *)dbname, path_kenv);
    } else {
    	dbp = bdb_open((char *)dbname, (char *)path);
    }

    if (dbp == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    bdb_data_t *data = NULL;
	lua_newtable( L );
    while ((data = bdb_seq(dbp, BDB_POS_NEXT, -1)) != NULL) {
		lua_pushinteger(L, data->bdb_dt_key);
		lua_newtable( L );

		if (l_bdb_str_to_obj(L, data) != 0) {
	        lua_pop(L, 1);
		} else {
		    lua_settable(L, -3);
		}
		bdb_freeget(data);
    }

    bdb_close(dbp);
    return 1;
}


static int libbdb_get(lua_State *L)
{
    const char *dbname = luaL_optstring(L, 1, NULL);
    const char *path = luaL_optstring(L, 2, NULL);
    long key = luaL_optnumber(L, 3, 0);
    if (dbname == NULL || key == 0) {
	    lua_pushnil(L);
	    return 1;
    }

    DB *dbp = NULL;
    if (path == NULL) {
		char path_kenv[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
    	dbp = bdb_open((char *)dbname, path_kenv);
    } else {
    	dbp = bdb_open((char *)dbname, (char *)path);
    }

    if (dbp == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    bdb_data_t *data = NULL;
    int f = 0;
    if ((data = bdb_get(dbp, key)) != NULL) {
        f = 1;
		lua_newtable( L );
		l_bdb_str_to_obj(L, data);
		bdb_freeget(data);
    }

    bdb_close(dbp);
    if (!f)
        lua_pushnil(L);
    return 1;
}

static int libbdb_schema(lua_State *L)
{
    const char *dbname = luaL_optstring(L, 1, NULL);
    const char *path = luaL_optstring(L, 2, NULL);
    if (dbname == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    DB *dbp = NULL;
    if (path == NULL) {
		char path_kenv[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
    	dbp = bdb_open((char *)dbname, path_kenv);
    } else {
    	dbp = bdb_open((char *)dbname, (char *)path);
    }

    if (dbp == NULL) {
	    lua_pushnil(L);
	    return 1;
    }
	lua_newtable( L );
	bdb_data_t data;
	data.bdb_dt_schema = bdb_get_schema(dbp, &data.bdb_dt_schema_len);
	if (data.bdb_dt_schema == NULL) {
	    lua_pushnil(L);
	    return 1;
	}

 	bdb_column_t *col = NULL;
 	while ((col = bdb_get_next_col(&data)) != NULL) {
 		l_pushtablestring(L, col->col_name, col->type);
		free(col);
	}

    bdb_close(dbp);
    return 1;
}

static int libbdb_check(lua_State *L)
{
	printf("%s\n", "BDB Library loaded");
	lua_pushboolean(L, 1);
    return 1;
}

static void l_bdb_free_cols(bdb_column_t *schema[], int n)
{
	for (int i = 0; i < n; i++)
		free(schema[i]);
}

static int libbdb_create(lua_State *L)
{
	if (lua_gettop(L) < 3) {
		lua_pushnil(L);
	    return 1;
	}
    const char *dbname = luaL_optstring(L, 1, NULL);
    const char *path = luaL_optstring(L, 2, NULL);
    if (dbname == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    DB *dbp = NULL;
    if (path == NULL) {
		char path_kenv[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
    	dbp = bdb_create((char *)dbname, path_kenv);
    } else {
    	dbp = bdb_create((char *)dbname, (char *)path);
    }

	int top = lua_gettop(L), i = 3;
	bdb_column_t *schema[top - 2];
	bdb_data_t c_data;
	memset(&c_data, 0, sizeof(bdb_data_t));

	while (i <= top) {
		const char *col = luaL_optstring(L, i, NULL);
		if (col == NULL)
			break;
		c_data.bdb_dt_schema = (char *)col;
		c_data.bdb_dt_schema_len = strlen(col) + 1;
		schema[i - 3] = bdb_get_next_col(&c_data);
		if (schema[i - 3] == NULL) {
			l_bdb_free_cols(schema, i - 2);
			lua_pushnil(L);
		    return 1;
		}
		i++;
	}

	if (i == 3) {
		lua_pushnil(L);
	    return 1;
	}
	i--;
	int ret = bdb_set_schema(dbp, schema, i - 2);
	dbp->close(dbp);
	l_bdb_free_cols(schema, i - 2);
	if (!ret)
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);
    return 1;
}

#define LUA_BDB_UD "BDB"

static DB **toDB (lua_State *L, int index)
{
  DB **bar = (DB **)lua_touserdata(L, index);
  if (bar == NULL) return NULL;
  return bar;
}

static DB **checkDB (lua_State *L, int index)
{
  DB **bar;
  luaL_checktype(L, index, LUA_TUSERDATA);
  bar = (DB **)luaL_checkudata(L, index, LUA_BDB_UD);
  if (bar == NULL) return NULL;
  return bar;
}
static DB **pushDB (lua_State *L)
{
	DB **bar = (DB **)lua_newuserdata(L, sizeof(DB *));
	luaL_getmetatable(L, LUA_BDB_UD);
	lua_setmetatable(L, -2);
	return bar;
}

static int libbdb_open(lua_State *L)
{
    const char *dbname = luaL_optstring(L, 1, NULL);
    const char *path = luaL_optstring(L, 2, NULL);
    if (dbname == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    DB *dbp = NULL;
    if (path == NULL) {
		char path_kenv[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0) {
		    lua_pushnil(L);
		    return 1;
	    }
    	dbp = bdb_open((char *)dbname, path_kenv);
    } else {
    	dbp = bdb_open((char *)dbname, (char *)path);
    }

    if (dbp == NULL) {
	    lua_pushnil(L);
	    return 1;
    }

    DB **dbp_r = pushDB(L);
    *dbp_r = dbp;

	return 1;
}

static int libbdb_close(lua_State *L)
{
	DB **dbp_r = checkDB(L, 1);
	if (dbp_r != NULL) {
		if ((*dbp_r)->close(*dbp_r)) {
			lua_pushnil(L);
		} else {
			*dbp_r = NULL;
			lua_pushboolean(L, 1);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int libbdb_fetch(lua_State *L)
{
	DB **dbp_r = checkDB(L, 1);
	if (dbp_r != NULL && *dbp_r != NULL) {
		long key = luaL_optnumber(L, 2, 0);
	    bdb_data_t *data = NULL;
	    int f = 0;
	    if ((data = bdb_get(*dbp_r, key)) != NULL) {
	        f = 1;
			lua_newtable( L );
			l_bdb_str_to_obj(L, data);
			bdb_freeget(data);
	    }
	    if (!f)
	    	lua_pushnil(L);
	} else {
		lua_pushnil(L);
	}
	return 1;
}
static int libbdb_put(lua_State *L)
{
	DB **dbp_r = checkDB(L, 1);
	if (dbp_r != NULL && *dbp_r != NULL) {
		long key = luaL_optnumber(L, 2, 0);
		bdb_data_t data;
		memset(&data, 0, sizeof(bdb_data_t));
		data.bdb_dt_key = key;
		data.bdb_dt_schema = bdb_get_schema(*dbp_r, &data.bdb_dt_schema_len);

		if (data.bdb_dt_schema == NULL) {
			bdb_close(*dbp_r);
		    lua_pushnil(L);
		    return 1;
		}
		int ret = 0;
		if ((ret = l_bdb_obj_to_str(L, &data)) == 0) {
			bdb_set(*dbp_r, &data);
			free(data.bdb_dt_p);
			bdb_sync(*dbp_r);
			lua_pushboolean(L, 1);
		} else {
		    lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int DB_gc (lua_State *L)
{
	DB **dbp_r = toDB(L, 1);
	if (dbp_r != NULL)
		printf("%s = %p\n", LUA_BDB_UD, dbp_r);
	else
		printf("%s\n", "error");
	return 0;
}

static int DB_tostring (lua_State *L)
{
	DB **dbp_r = toDB(L, 1);
	if (dbp_r != NULL) {
		char buff[32];
		sprintf(buff, "%p", dbp_r);
		lua_pushfstring(L, "%s (%s)", LUA_BDB_UD, buff);
	} else {
		lua_pushstring(L, "error");
	}
	return 1;
}



static const luaL_Reg DB_meta[] = {
  {"__gc",       DB_gc},
  {"__tostring", DB_tostring},
  {0, 0}
};

static const luaL_Reg bdb[] = {
    {"set", libbdb_set},
    {"del", libbdb_del},
    {"get", libbdb_get},
    {"get_all", libbdb_get_all},
    {"schema", libbdb_schema},
    {"check", libbdb_check},
    {"create", libbdb_create},
    {"open", libbdb_open},
    {"close", libbdb_close},
    {"fetch", libbdb_fetch},
    {"put", libbdb_put},
    {NULL, NULL}
};

#ifdef STATIC
LUAMOD_API int luaopen_libluabdb(lua_State *L)
#else
LUALIB_API int luaopen_libluabdb(lua_State *L)
#endif
{
    luaL_newmetatable(L, LUA_BDB_UD);
    luaL_newlib(L, DB_meta);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);
    luaL_newlib(L, bdb);
    return 1;
}