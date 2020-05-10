#include <sys/mount.h>
#include <sys/uio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <kzfs.h>

static int l_fill_zfs_props(lua_State* L, kzfs_prop_list_t **list)
{
	int err = 0;
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
        if(lua_isstring(L, -1))
            err += kzfs_props_fill(list, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return err;
}

static int l_fill_zfs_propsval(lua_State* L, kzfs_propval_list_t **list)
{
	int err = 0;
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
        if(lua_isstring(L, -1)) {
            err += kzfs_props_val_fill(list, lua_tostring(L, -2), lua_tostring(L, -1));
        }
        lua_pop(L, 1);
    }
    return err;
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
                // print_table(L, depth + 2);
            }
            lua_pop(L, 1);
        }
    }

static int l_fill_zfs_devs(lua_State* L, kzpool_dev_list_t **list)
{
	char **type_list = NULL;
	int *is_log_list = NULL;
	int count = 0;

	int err = 0;
	int flg = 0;
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		if (lua_istable(L, -1)) {
			count++;
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				if (strcmp("type", lua_tostring(L, -2)) == 0) {
					type_list = realloc(type_list, count * sizeof(char *));
					type_list[count - 1] = malloc(strlen(lua_tostring(L, -1)) + 1);
					snprintf(type_list[count - 1], strlen(lua_tostring(L, -1)) + 1, "%s", lua_tostring(L, -1));
				} else if (strcmp("is_log", lua_tostring(L, -2)) == 0) {
					is_log_list = realloc(is_log_list, count * sizeof(char *));
					if (lua_tonumber(L, -1)) {
						is_log_list[count - 1] = 1;
					} else {
						is_log_list[count - 1] = 0;
					}
				}
				lua_pop(L, 1);
			}
		}
        lua_pop(L, 1);
    }
    int i = 0;
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		if (lua_istable(L, -1)) {
			lua_pushnil(L);
			kzpool_dev_info_t *info = NULL;
			while (lua_next(L, -2) != 0) {
				if (strcmp("devs", lua_tostring(L, -2)) == 0) {
					lua_pushnil(L);
					while (lua_next(L, -2) != 0) {
						err += kzpool_dev_info_fill(&info, type_list[i], 0, is_log_list[i], (char *)lua_tostring(L, -1));
						lua_pop(L, 1);
					}
				}
				lua_pop(L, 1);
			}
			err += kzpool_dev_list_fill(list, info);
			i++;
		}
        lua_pop(L, 1);
    }
    for (i = 0; i < count; i++) {
    	free(type_list[i]);
    }
    free(type_list);
    free(is_log_list);
    if (err) {
    	kzpool_free_dev_list(*list);
    }
    return err;
}

static void l_pushtablestring(lua_State* L, char* key , char* value)
{
    lua_pushstring(L, key);
    lua_pushstring(L, value);
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

static int libzfs_ds_destroy(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	if (zname != NULL) {
		int recursive = luaL_optnumber(L, 2, 0);
		int ret = kzfs_ds_destroy(zname, recursive);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_ds_rename(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *new_zname = luaL_optstring(L, 2, NULL);
	if (zname != NULL && new_zname != NULL) {
		int force = luaL_optnumber(L, 3, 0);
		int noumount = luaL_optnumber(L, 4, 0);
		int parents = luaL_optnumber(L, 5, 0);
		int ret = kzfs_ds_rename(zname, new_zname, force, noumount, parents);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_ds_create(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	if (zname == NULL) {
		lua_pushnil(L);
		return 1;
	}
	int ret = 0;
	kzfs_propval_list_t *pval = NULL;
	if (lua_istable(L, 2)) {
		ret = l_fill_zfs_propsval(L, &pval);
	}
	if (ret) {
		kzfs_free_props_val(pval);
		lua_pushnil(L);
	    return 1;
	}
	ret = kzfs_ds_create(zname, pval);
	kzfs_free_props_val(pval);
	if (ret)
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
    return 1;
}

static int libzfs_ds_update(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	if (zname == NULL) {
		lua_pushnil(L);
		return 1;
	}
	if (!lua_istable(L, 2)) {
		lua_pushnil(L);
		return 1;
	}
	int ret = 0;
	kzfs_propval_list_t *pval = NULL;
	ret = l_fill_zfs_propsval(L, &pval);
	if (ret) {
		kzfs_free_props_val(pval);
		lua_pushnil(L);
	    return 1;
	}
	ret = kzfs_ds_update(zname, pval);
	kzfs_free_props_val(pval);
	if (ret)
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
    return 1;
}

// int l_zfs_mount(char *zname, const char *mntopts)
// {
// 	char tmp_mntopts[mntopts ? strlen(mntopts) + 1 : 1];
// 	int flags = 0;
	
// 	kzfs_prop_list_t *list = NULL;
//     if (kzfs_props_fill(&list, "mountpoint"))
//     	return -1;
// 	kzfs_ds_list_t *dss = kzfs_ds_list(list, zname);

// 	if (dss == NULL) {
// 		kzfs_free_props(list);
//     	return -1;
// 	}

// 	char *names[] = {"fstype", "fspath", "from"};
// 	int i = 0;
// 	int ret = 0;
// 	for (; i < dss->count; i++) {
// 		kzfs_propval_list_t *pval = dss->ds_info_list[i]->propval_list;
		
// 		struct iovec iov[6];
// 		iov[0].iov_base = __DECONST(char *, names[0]);
// 		iov[0].iov_len = strlen(names[0]) + 1;
// 		iov[1].iov_base = __DECONST(char *, "zfs");
// 		iov[1].iov_len = strlen("zfs") + 1;
// 		iov[2].iov_base = __DECONST(char *, names[1]);
// 		iov[2].iov_len = strlen(names[1]) + 1;
// 		iov[3].iov_base = __DECONST(char *, pval->values[0]);
// 		iov[3].iov_len = strlen(pval->values[0]) + 1;
// 		iov[4].iov_base = __DECONST(char *, names[2]);
// 		iov[4].iov_len = strlen(names[2]) + 1;
// 		iov[5].iov_base = __DECONST(char *, dss->ds_info_list[i]->zname);
// 		iov[5].iov_len = strlen(dss->ds_info_list[i]->zname) + 1;

// 		ret = nmount(iov, 6, force ? MNT_FORCE : 0);
// 	}

// 	kzfs_free_props(list);
// 	kzfs_free_ds_list(dss);
// 	return 0;
// }


static int libzfs_ds_list(lua_State *L)
{
	const char *zname = NULL;
	kzfs_prop_list_t *list = NULL;
	if(lua_isstring(L, 1))
		zname = luaL_optstring(L, 1, NULL);
	int ret = 0;
	if (lua_istable(L, 1)) {
		ret = l_fill_zfs_props(L, &list);
	} else if (zname && lua_istable(L, 2)) {
		ret = l_fill_zfs_props(L, &list);
	}
	if (ret) {
		kzfs_free_props(list);
		lua_pushnil(L);
	    return 1;
	}
 
	kzfs_ds_list_t *dss = kzfs_ds_list(list, zname);
	if (dss == NULL) {
		kzfs_free_props(list);
		lua_pushnil(L);
	    return 1;
	}
	int i = 0;
	for (; i < dss->count; i++) {
		if (!i)
			lua_newtable( L );
		lua_pushstring(L, dss->ds_info_list[i]->zname);
		printf("%s\n", dss->ds_info_list[i]->zname);
		if (list == NULL) {
			lua_pushstring(L, "-");
		} else {
			kzfs_propval_list_t *pval = dss->ds_info_list[i]->propval_list;
			lua_newtable( L );
			for (int j = 0; j < pval->count; j++) {
				l_pushtablestring(L, pval->props[j], pval->values[j]);
			}
		}
		lua_settable( L, -3 );
	}
	kzfs_free_props(list);
	kzfs_free_ds_list(dss);

	if (!i)
		lua_pushnil(L);
    return 1;
}

static int libzfs_snap_create(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *snapname = luaL_optstring(L, 2, NULL);
	if (zname != NULL && snapname != NULL) {
		const char *desc = luaL_optstring(L, 3, NULL);
		if (desc == NULL)
			desc = "";
		int recursive = luaL_optnumber(L, 4, 0);
		int ret = kzfs_snap_create(zname, snapname, desc, recursive);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_snap_destroy(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *snapname = luaL_optstring(L, 2, NULL);
	if (zname != NULL && snapname != NULL) {
		int recursive = luaL_optnumber(L, 3, 0);
		int ret = kzfs_snap_destroy(zname, snapname, recursive);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_snap_list(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	if (zname == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	kzfs_snap_list_st_t *list = kzfs_snap_list(zname);
	if (list == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	int i = 0;
	for (; i < list->count; i++) {
		if (!i)
			lua_newtable( L );
		lua_pushstring(L, list->list[i]->snapname);
		lua_newtable( L );
		l_pushtablenumber(L, "creation", list->list[i]->creation);
		l_pushtablestring(L, "desc", list->list[i]->desc);
		l_pushtablestring(L, "used", list->list[i]->used);
		lua_settable( L, -3 );
	}
	kzfs_free_snap_list(list);
	if (!i)
		lua_pushnil(L);
    return 1;
}

static int libzfs_snap_rollback(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *snapname = luaL_optstring(L, 2, NULL);
	if (zname != NULL && snapname != NULL) {
		int ret = kzfs_snap_rollback(zname, snapname);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_snap_rename(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *snapname = luaL_optstring(L, 2, NULL);
	const char *new_snapname = luaL_optstring(L, 3, NULL);
	if (zname != NULL && snapname != NULL && new_snapname != NULL) {
		int force = luaL_optnumber(L, 4, 0);
		int recursive = luaL_optnumber(L, 5, 0);
		int ret = kzfs_snap_rename(zname, snapname, new_snapname, force, recursive);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_snap_update(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *snapname = luaL_optstring(L, 2, NULL);
	if (zname != NULL && snapname != NULL) {
		const char *desc = luaL_optstring(L, 3, "");
		int ret = kzfs_snap_update(zname, snapname, desc);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_send(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *ip = luaL_optstring(L, 2, NULL);
	int port = luaL_optnumber(L, 3, 0);
	if (zname != NULL && ip != NULL && port) {
		int move = luaL_optnumber(L, 4, 0);
		int ret = kzfs_send(zname, ip, port, move);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_recv(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	const char *ip = luaL_optstring(L, 2, NULL);
	int port = luaL_optnumber(L, 3, 0);
	if (zname != NULL && ip != NULL && port) {
		int ret = kzfs_recv(zname, ip, port);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_settings_merge(lua_State *L)
{
	const char *tmp_from = luaL_optstring(L, 1, NULL);
	const char *tmp_to = luaL_optstring(L, 2, NULL);
	if (tmp_from != NULL && tmp_to != NULL) {
		char from[strlen(tmp_from) + 1];
		snprintf(from, sizeof(from), "%s", tmp_from);
		char to[strlen(tmp_to) + 1];
		snprintf(to, sizeof(to), "%s", tmp_to);
		int ret = kzfs_settings_merge(from, to);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_unmount(lua_State *L)
{
	const char *entry = luaL_optstring(L, 1, NULL);
	if (entry != NULL) {
		int force = luaL_optnumber(L, 2, 0);
		int ret = kzfs_unmount(entry, force);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_unmount_all(lua_State *L)
{
	int force = luaL_optnumber(L, 1, 0);
	int ret = kzfs_unmount_all(force);
	if (!ret) {
		lua_pushboolean(L, 1);
	    return 1;
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_mount(lua_State *L)
{
	const char *zname = luaL_optstring(L, 1, NULL);
	if (zname != NULL) {
		const char *mntopts = luaL_optstring(L, 2, NULL);
		int ret = kzfs_mount(zname, mntopts);
		// int ret = l_zfs_mount((char)entry, mntopts);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_mount_all(lua_State *L)
{
	const char *mntopts = luaL_optstring(L, 1, NULL);
	int ret = kzfs_mount_all(mntopts);
	if (!ret) {
		lua_pushboolean(L, 1);
	    return 1;
	}
	lua_pushnil(L);
    return 1;
}

static int libzfs_mount_list(lua_State *L)
{
	kzfs_mount_list_st_t *list = kzfs_mount_list();
	if (list == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	int i = 0;
	for (; i < list->count; i++) {
		if (!i)
			lua_newtable( L );
		l_pushtablestring(L, list->list[i]->mi_special, list->list[i]->mi_mountpoint);
	}
	kzfs_free_mount_list(list);
	if (!i)
		lua_pushnil(L);
    return 1;
}

static int libzfs_test(lua_State *L)
{
	printf("%d\n", lua_gettop(L));
	kzpool_dev_list_t *list = NULL;
	lua_settop(L, 1);
	l_fill_zfs_devs(L, &list);
	if (list != NULL) {
		for (int i = 0; i < list->count; i++) {
            kzpool_dev_info_t *el = list->list[i];
            printf("%s %s\n", el->type, el->is_log ? "log" : "");
            if (el->is_complex) {
                for (int j = 0; j < el->count; j++) {
                    kzpool_dev_info_t *subel = el->devs[j];
                    printf("\t%s\t", subel->type);
                    for (int k = 0; k < subel->count; k++) {
                        printf("%s ", subel->devs[k]);
                    }
                    printf("\n");
                }
            } else {
                for (int j = 0; j < el->count; j++) {
                    printf("%s ", (char *)el->devs[j]);
                }
            }
            printf("\n");
        }
        kzpool_free_dev_list(list);
	}

	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		printf("%s = %s\n", lua_tostring(L, -2), lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    return 1;
}

/* ZPOOL */

static int libzpool_create(lua_State *L)
{
	if (lua_gettop(L) != 4) {
		lua_pushnil(L);
	    return 1;
	}

	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
	    return 1;	
	}

	if (!lua_istable(L, 2)) {
		lua_pushnil(L);
		return 1;
	}

	kzfs_propval_list_t *props_list = NULL;
	kzfs_propval_list_t *fs_props_list = NULL;
	if (lua_istable(L, 4)) {
		l_fill_zfs_propsval(L, &fs_props_list);
	}
	if (lua_istable(L, 3)) {
		lua_settop(L, 3);
		l_fill_zfs_propsval(L, &props_list);
	}
	lua_settop(L, 2);
	kzpool_dev_list_t *list = NULL;
	if (l_fill_zfs_devs(L, &list)) {
		kzfs_free_props_val(props_list);
		kzfs_free_props_val(fs_props_list);
		lua_pushnil(L);
		return 1;
	}

	if (kzpool_create(zpool_name, list, props_list, fs_props_list)) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	kzpool_free_dev_list(list);
	kzfs_free_props_val(props_list);
	kzfs_free_props_val(fs_props_list);
    return 1;
}

static int libzpool_destroy(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name != NULL) {
		int ret = kzpool_destroy(zpool_name);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_update(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}
	if (!lua_istable(L, 2)) {
		lua_pushnil(L);
		return 1;
	}
	int ret = 0;
	kzfs_propval_list_t *pval = NULL;
	ret = l_fill_zfs_propsval(L, &pval);
	if (ret) {
		kzfs_free_props_val(pval);
		lua_pushnil(L);
	    return 1;
	}
	ret = kzpool_update(zpool_name, pval);
	kzfs_free_props_val(pval);
	if (ret)
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
    return 1;
}

static int libzpool_list(lua_State *L)
{
	int ret = 0;
	kzfs_prop_list_t *list = NULL;
	if (lua_istable(L, 1)) {
		ret = l_fill_zfs_props(L, &list);
	}
	if (ret) {
		kzfs_free_props(list);
		lua_pushnil(L);
	    return 1;
	}

	kzfs_zpool_list_t *dss = kzpool_list(list);
	if (dss == NULL) {
		kzfs_free_props(list);
		lua_pushnil(L);
	    return 1;
	}
	int i = 0;
	for (; i < dss->count; i++) {
		if (!i)
			lua_newtable( L );
		int n = snprintf(NULL, 0, "%lu", dss->zpool_info_list[i]->guid);
		char tmp_guid[n + 1];
		snprintf(tmp_guid, sizeof(tmp_guid), "%lu", dss->zpool_info_list[i]->guid);
		lua_pushstring(L, tmp_guid);
		if (list == NULL) {
			lua_pushstring(L, dss->zpool_info_list[i]->zpool_name);
		} else {
			kzfs_propval_list_t *pval = dss->zpool_info_list[i]->propval_list;
			lua_newtable( L );
			for (int j = 0; j < pval->count; j++) {
				l_pushtablestring(L, pval->props[j], pval->values[j]);
			}
			l_pushtablestring(L, "name", dss->zpool_info_list[i]->zpool_name);
		}
		lua_settable( L, -3 );
	}
	kzfs_free_props(list);
	kzpool_free_list(dss);
	if (!i)
		lua_pushnil(L);
    return 1;
}

static int libzpool_export(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name != NULL) {
		int force = luaL_optnumber(L, 2, 0);
		int ret = kzpool_export(zpool_name, force);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_clear(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name != NULL) {
		const char *dev = NULL;
		if (lua_isstring(L, 2)) {
			dev = luaL_optstring(L, 2, NULL);
		}
		int do_rewind = luaL_optnumber(L, 3, 0);
		int ret = kzpool_clear(zpool_name, dev, do_rewind);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_reguid(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name != NULL) {
		int ret = kzpool_reguid(zpool_name);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_reopen(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name != NULL) {
		int ret = kzpool_reopen(zpool_name);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_labelclear(lua_State *L)
{
	const char *dev = luaL_optstring(L, 1, NULL);
	if (dev != NULL) {
		int force = luaL_optnumber(L, 2, 0);
		int ret = kzpool_labelclear(dev, force);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_attach(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	const char *dev = luaL_optstring(L, 2, NULL);
	const char *new_dev = luaL_optstring(L, 3, NULL);

	if (zpool_name != NULL && dev != NULL && new_dev != NULL) {
		int force = luaL_optnumber(L, 4, 0);
		int ret = 0;
		ret = kzpool_attach(zpool_name, dev, new_dev, force);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}

	lua_pushnil(L);
    return 1;
}

static int libzpool_replace(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	const char *dev = luaL_optstring(L, 2, NULL);

	if (zpool_name != NULL && dev != NULL) {
		const char *new_dev = NULL;
		int force = 0;
		new_dev = luaL_optstring(L, 3, NULL);
		force = luaL_optnumber(L, 4, 0);
		int ret = 0;
		ret = kzpool_replace(zpool_name, dev, new_dev, force);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_detach(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	const char *dev = luaL_optstring(L, 2, NULL);
	if (zpool_name != NULL && dev != NULL) {
		int ret = kzpool_detach(zpool_name, dev);
		if (!ret) {
			lua_pushboolean(L, 1);
		    return 1;
		}
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_online(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}
	if (!lua_istable(L, 2)) {
		lua_pushnil(L);
		return 1;
	}

	int expand = luaL_optnumber(L, 3, 0);
	lua_settop(L, 2);

	kzpool_dev_info_t *info = NULL;
	int err = 0;
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		err += kzpool_dev_info_fill(&info, "stripe", 0, 0, (char *)lua_tostring(L, -1));
        lua_pop(L, 1);
	}
	if (err) {
		lua_pushnil(L);
		return 1;
	}
	if (kzpool_online(zpool_name, info, expand)) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	kzpool_free_dev_info(info);
    return 1;
}

static int libzpool_offline(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}
	if (!lua_istable(L, 2)) {
		lua_pushnil(L);
		return 1;
	}

	int temp = luaL_optnumber(L, 3, 0);
	lua_settop(L, 2);

	kzpool_dev_info_t *info = NULL;
	int err = 0;
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		err += kzpool_dev_info_fill(&info, "stripe", 0, 0, (char *)lua_tostring(L, -1));
        lua_pop(L, 1);
	}
	if (err) {
		lua_pushnil(L);
		return 1;
	}
	if (kzpool_offline(zpool_name, info, temp)) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	kzpool_free_dev_info(info);
    return 1;
}

static int libzpool_add(lua_State *L)
{
	if (lua_gettop(L) < 2) {
		lua_pushnil(L);
	    return 1;
	}

	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	if (!lua_istable(L, 2)) {
		lua_pushnil(L);
	    return 1;
	}
	int force = luaL_optnumber(L, 3, 0);
	kzpool_dev_list_t *list = NULL;
	lua_settop(L, 2);
	if (l_fill_zfs_devs(L, &list)) {
		lua_pushnil(L);
		return 1;
	}
	int ret = kzpool_add(zpool_name, list, force);
	if (ret) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	kzpool_free_dev_list(list);
    return 1;
}

static int libzpool_scrub(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	int cmd = luaL_optnumber(L, 2, 0);
	int ret = kzpool_scrub(zpool_name, cmd);
	if (!ret) {
		lua_pushboolean(L, 1);
	    return 1;
	}
	lua_pushnil(L);
    return 1;
}

static int libzpool_split(lua_State *L)
{
	if (lua_gettop(L) != 4) {
		lua_pushnil(L);
		return 1;
	}

	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}
	const char *new_zpool_name = luaL_optstring(L, 2, NULL);
	if (new_zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}

	kzpool_dev_info_t *info = NULL;
	if (lua_istable(L, 4)) {
		int err = 0;
		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
			err += kzpool_dev_info_fill(&info, "stripe", 0, 0, (char *)lua_tostring(L, -1));
	        lua_pop(L, 1);
		}
		if (err) {
			lua_pushnil(L);
			return 1;
		}
	}
	kzfs_propval_list_t *props_list = NULL;
	if (lua_istable(L, 3)) {
		lua_settop(L, 3);
		l_fill_zfs_propsval(L, &props_list);
	}
	if (kzpool_split(zpool_name, new_zpool_name, props_list, info)) {
		lua_pushnil(L);
	} else {
		lua_pushboolean(L, 1);
	}
	kzpool_free_dev_info(info);
	kzfs_free_props_val(props_list);
    return 1;
}

static int libzpool_get_devs(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}

	kzpool_dev_list_t *dlist = kzpool_get_devs(zpool_name);
	if (dlist == NULL) {
		lua_pushnil(L);
	    return 1;
	}
	int i = 0;
	for (; i < dlist->count; i++) {
		if (!i)
			lua_newtable( L );
		lua_pushnumber(L, i + 1);
		lua_newtable( L );
		l_pushtablestring(L, "type", dlist->list[i]->type);
		lua_pushstring(L, "is_log");
		lua_pushinteger(L, dlist->list[i]->is_log);
		lua_settable( L, -3 );

		lua_pushstring(L, "devs");
		lua_newtable( L );
		char **devs = (char **)dlist->list[i]->devs;

		for (int j = 0; j < dlist->list[i]->count; j++) {
			lua_pushnumber(L, j + 1);
			lua_pushstring(L, devs[j]);
			lua_settable( L, -3 );
		}
		lua_settable( L, -3 );
		lua_settable( L, -3 );
	}
	kzpool_free_dev_list(dlist);
	if (!i)
		lua_pushnil(L);
    return 1;
}

static void l_push_zpool_status_config(lua_State *L, kzpool_status_config_t *zsc, int depth)
{
	if (zsc == NULL)
		return;

	lua_newtable( L );

	if (depth) {
		l_pushtablestring(L, "name", zsc->name);
		l_pushtablestring(L, "state", zsc->state);
		l_pushtablestring(L, "rbuf", zsc->rbuf);
		l_pushtablestring(L, "wbuf", zsc->wbuf);
		l_pushtablestring(L, "cbuf", zsc->cbuf);
		l_pushtableinteger(L, "block_size_configured", zsc->block_size.configured);
		l_pushtableinteger(L, "block_size_native", zsc->block_size.native);
		if (zsc->msg) {
			l_pushtablestring(L, "msg", zsc->msg);
		} else {
			l_pushtablenil(L, "msg");
		}
		if (zsc->act) {
			l_pushtablestring(L, "act", zsc->act);
		} else {
			l_pushtablenil(L, "act");
		}
		if (zsc->config_path) {
			l_pushtablestring(L, "config_path", zsc->config_path);
		} else {
			l_pushtablenil(L, "config_path");
		}
	}

	for (int i = 0; i < zsc->count; i++) {
		kzpool_status_config_t *cfg = zsc->config_list[i];
		lua_pushinteger(L, i + 1);
		lua_newtable( L );
		l_pushtablestring(L, "name", cfg->name);
		l_pushtablestring(L, "state", cfg->state);
		l_pushtablestring(L, "rbuf", cfg->rbuf);
		l_pushtablestring(L, "wbuf", cfg->wbuf);
		l_pushtablestring(L, "cbuf", cfg->cbuf);
		l_pushtableinteger(L, "block_size_configured", cfg->block_size.configured);
		l_pushtableinteger(L, "block_size_native", cfg->block_size.native);
		if (cfg->msg) {
			l_pushtablestring(L, "msg", cfg->msg);
		} else {
			l_pushtablenil(L, "msg");
		}
		if (cfg->act) {
			l_pushtablestring(L, "act", cfg->act);
		} else {
			l_pushtablenil(L, "act");
		}
		if (cfg->config_path) {
			l_pushtablestring(L, "config_path", cfg->config_path);
		} else {
			l_pushtablenil(L, "config_path");
		}

		if (cfg->config_list != NULL) {
			lua_pushstring(L, "config_list");
			lua_newtable( L );
			for (int j = 0; j < cfg->count; j++) {
				lua_pushinteger(L, j + 1);
				l_push_zpool_status_config(L, cfg->config_list[j], depth+1);
			}
			lua_settable( L, -3 );
		} else {
			l_pushtablenil(L, "config_list");
		}
		lua_settable( L, -3 );
	}
	lua_settable( L, -3 );
}

static int libzpool_status(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}
	kzpool_status_t *slist = kzpool_status(zpool_name);
	if (slist == NULL) {
		lua_pushnil(L);
	    return 1;
	}

	lua_newtable( L );
	l_pushtablestring(L, "name", slist->name);
	int n = snprintf(NULL, 0, "%lu", slist->guid);
	char tmp_guid[n + 1];
	snprintf(tmp_guid, sizeof(tmp_guid), "%lu", slist->guid);
	l_pushtablestring(L, "guid", tmp_guid);
	l_pushtablestring(L, "state", slist->state);

	if (slist->status) {
		l_pushtablestring(L, "status", slist->status);
	} else {
		l_pushtablenil(L, "status");
	}
	if (slist->action) {
		l_pushtablestring(L, "action", slist->action);
	} else {
		l_pushtablenil(L, "action");
	}
	if (slist->scan) {
		l_pushtablestring(L, "scan", slist->scan);
	} else {
		l_pushtablenil(L, "scan");
	}
	if (slist->scan_warn) {
		l_pushtablestring(L, "scan_warn", slist->scan_warn);
	} else {
		l_pushtablenil(L, "scan_warn");
	}
	if (slist->remove) {
		l_pushtablestring(L, "remove", slist->remove);
	} else {
		l_pushtablenil(L, "remove");
	}
	if (slist->checkpoint) {
		l_pushtablestring(L, "checkpoint", slist->checkpoint);
	} else {
		l_pushtablenil(L, "checkpoint");
	}

	if (slist->config) {
		lua_pushstring(L, "config");
		l_push_zpool_status_config(L, slist->config, 0);
	} else {
		l_pushtablenil(L, "config");
	}
	if (slist->logs) {
		lua_pushstring(L, "logs");
		l_push_zpool_status_config(L, slist->logs, 0);
	} else {
		l_pushtablenil(L, "logs");
	}

	if (slist->cache) {
		lua_pushstring(L, "cache");
		l_push_zpool_status_config(L, slist->cache, 0);
	} else {
		l_pushtablenil(L, "cache");
	}

	if (slist->spares) {
		lua_pushstring(L, "spares");
		l_push_zpool_status_config(L, slist->spares, 0);
	} else {
		l_pushtablenil(L, "spares");
	}


	kzpool_free_status(slist);
	return 1;
}

static void l_push_zpool_import_config(lua_State *L, kzpool_import_config_t *zsc, int depth)
{
	if (zsc == NULL)
		return;

	lua_newtable( L );


	if (depth) {
		l_pushtablestring(L, "name", zsc->name);
		l_pushtablestring(L, "state", zsc->state);
		if (zsc->msg) {
			l_pushtablestring(L, "msg", zsc->msg);
		} else {
			l_pushtablenil(L, "msg");
		}
	}

	for (int i = 0; i < zsc->config_count; i++) {
		kzpool_import_config_t *cfg = zsc->config_list[i];
		lua_pushinteger(L, i + 1);
		lua_newtable( L );
		l_pushtablestring(L, "name", cfg->name);
		l_pushtablestring(L, "state", cfg->state);
		if (cfg->msg) {
			l_pushtablestring(L, "msg", cfg->msg);
		} else {
			l_pushtablenil(L, "msg");
		}
		if (cfg->config_list != NULL) {
			lua_pushstring(L, "config_list");
			lua_newtable( L );
			for (int j = 0; j < cfg->config_count; j++) {
				lua_pushinteger(L, j + 1);
				l_push_zpool_import_config(L, cfg->config_list[j], depth+1);
			}
			lua_settable( L, -3 );
		} else {
			l_pushtablenil(L, "config_list");
		}
		lua_settable( L, -3 );
	}

	if (zsc->cache_count) {
		lua_pushstring(L, "cache_list");
		lua_newtable( L );
		for (int i = 0; i < zsc->cache_count; i++) {
			l_pushtable_numkey_string(L, i + 1, zsc->cache_list[i]);
		}
		lua_settable( L, -3 );
	} else {
		l_pushtablenil(L, "cache_list");
	}

	if (zsc->spares_count) {
		lua_pushstring(L, "spares_list");
		lua_newtable( L );
		for (int i = 0; i < zsc->spares_count; i++) {
			l_pushtable_numkey_string(L, i + 1, zsc->spares_list[i]);
		}
		lua_settable( L, -3 );
	} else {
		l_pushtablenil(L, "spares_list");
	}

	lua_settable( L, -3 );
}

static int libzpool_import_list(lua_State *L)
{
	const char *cachefile = luaL_optstring(L, 1, NULL);
	const char *search = luaL_optstring(L, 2, NULL);
	int do_destroyed = luaL_optnumber(L, 3, 0);
	kzpool_import_list_t *ilist = NULL;
	if (cachefile == NULL && search == NULL) {
		ilist = kzpool_import_list(NULL, NULL, do_destroyed);
	} else {
		char buf[strlen(cachefile) + 1];
		snprintf(buf, sizeof(buf), "%s", cachefile);
		ilist = kzpool_import_list(buf, search, do_destroyed);
	}

	if (ilist == NULL || !ilist->count) {
		lua_pushnil(L);
	    return 1;
	}
	int i = 0;
	for (; i < ilist->count; i++) {
		if (!i)
			lua_newtable( L );
		lua_pushnumber(L, i + 1);
		lua_newtable( L );
		l_pushtablestring(L, "name", ilist->list[i]->name);
		l_pushtablestring(L, "state", ilist->list[i]->state);

		int n = snprintf(NULL, 0, "%lu", ilist->list[i]->guid);
		char tmp_guid[n + 1];
		snprintf(tmp_guid, sizeof(tmp_guid), "%lu", ilist->list[i]->guid);
		l_pushtablestring(L, "guid", tmp_guid);

		l_pushtableinteger(L, "destroyed", ilist->list[i]->destroyed);

		if (ilist->list[i]->reason) {
			l_pushtablestring(L, "reason", ilist->list[i]->reason);
		} else {
			l_pushtablenil(L, "reason");
		}

		if (ilist->list[i]->action) {
			l_pushtablestring(L, "action", ilist->list[i]->action);
		} else {
			l_pushtablenil(L, "action");
		}

		if (ilist->list[i]->comment) {
			l_pushtablestring(L, "comment", ilist->list[i]->comment);
		} else {
			l_pushtablenil(L, "comment");
		}

		if (ilist->list[i]->msg) {
			l_pushtablestring(L, "msg", ilist->list[i]->msg);
		} else {
			l_pushtablenil(L, "msg");
		}

		if (ilist->list[i]->warn) {
			l_pushtablestring(L, "warn", ilist->list[i]->warn);
		} else {
			l_pushtablenil(L, "warn");
		}

		if (ilist->list[i]->config) {
			lua_pushstring(L, "config");
			l_push_zpool_import_config(L, ilist->list[i]->config, 0);
		} else {
			l_pushtablenil(L, "config");
		}

		if (ilist->list[i]->logs) {
			lua_pushstring(L, "logs");
			l_push_zpool_import_config(L, ilist->list[i]->logs, 0);
		} else {
			l_pushtablenil(L, "logs");
		}
		lua_settable( L, -3 );
	}

	kzpool_free_import_list(ilist);
	if (!i)
		lua_pushnil(L);
	return 1;
}

static int libzpool_import(lua_State *L)
{
	const char *zpool_name = luaL_optstring(L, 1, NULL);
	if (zpool_name == NULL) {
		lua_pushnil(L);
		return 1;
	}
	int ret = 0;
	kzfs_propval_list_t *pval = NULL;
	if (lua_istable(L, 2)) {
		ret = l_fill_zfs_propsval(L, &pval);
	}

	if (ret) {
		kzfs_free_props_val(pval);
		lua_pushnil(L);
	    return 1;
	}
	ret = kzpool_import(zpool_name, pval);
	kzfs_free_props_val(pval);
	if (ret)
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
    return 1;
}

static int libzpool_import_all(lua_State *L)
{
	int ret = 0;
	kzfs_propval_list_t *pval = NULL;
	if (lua_istable(L, 1)) {
		ret = l_fill_zfs_propsval(L, &pval);
	}
	if (ret) {
		kzfs_free_props_val(pval);
		lua_pushnil(L);
	    return 1;
	}
	ret = kzpool_import_all(pval);
	kzfs_free_props_val(pval);
	if (ret)
		lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
    return 1;
}

static int libzfs_ds_prop_remove(lua_State *L)
{
	if (lua_gettop(L) <= 1) {
		lua_pushnil(L);
		return 1;
	}
	int ret = 0;
	const char *zname = luaL_optstring(L, 1, NULL);

	if (lua_istable(L, 2)) {
		int i = 0;
		size_t len = lua_rawlen(L, 2);
		if (len <= 0) {
			lua_pushnil(L);
			return 1;
		}
		const char *props[len];

		lua_pushnil(L);
		while(lua_next(L, -2) != 0) {
	        if(lua_isstring(L, -1)) {
	        	props[i] = lua_tostring(L, -1);
	        	i++;
	        }
	        lua_pop(L, 1);
	    }
	    ret = kzfs_ds_props_delete(zname, props, len);
	} else if (lua_isstring(L, 2)) {
		ret = kzfs_ds_prop_delete(zname, luaL_optstring(L, 2, NULL));
	} else {
		lua_pushnil(L);
		return 1;
	}
	if (!ret)
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);
    return 1;
}


static const luaL_Reg zfs[] = {
    /* ZPOOL */
    {"zpool_create", libzpool_create},
    {"zpool_destroy", libzpool_destroy},
    {"zpool_update", libzpool_update},
    {"zpool_list", libzpool_list},
    {"zpool_export", libzpool_export},
    {"zpool_clear", libzpool_clear},
    {"zpool_reguid", libzpool_reguid},
    {"zpool_reopen", libzpool_reopen},
    {"zpool_labelclear", libzpool_labelclear},
    {"zpool_attach", libzpool_attach},
    {"zpool_replace", libzpool_replace},
    {"zpool_detach", libzpool_detach},
    {"zpool_online", libzpool_online},
    {"zpool_offline", libzpool_offline},
    {"zpool_add", libzpool_add},
    {"zpool_scrub", libzpool_scrub},
    {"zpool_split", libzpool_split},
    {"zpool_get_devs", libzpool_get_devs},
    {"zpool_status", libzpool_status},
    {"zpool_import_list", libzpool_import_list},
    {"zpool_import", libzpool_import},
    {"zpool_import_all", libzpool_import_all},

	/* ZFS */
    {"ds_destroy", libzfs_ds_destroy},
    {"ds_rename", libzfs_ds_rename},
    {"ds_create", libzfs_ds_create},
    {"ds_update", libzfs_ds_update},
    {"ds_list", libzfs_ds_list},
    {"snap_destroy", libzfs_snap_destroy},
    {"snap_create", libzfs_snap_create},
    {"snap_rollback", libzfs_snap_rollback},
    {"snap_list", libzfs_snap_list},
    {"snap_rename", libzfs_snap_rename},
    {"snap_update", libzfs_snap_update},
    {"send", libzfs_send},
    {"recv", libzfs_recv},
    {"settings_merge", libzfs_settings_merge},
    {"ds_prop_remove", libzfs_ds_prop_remove},
    // {"unmount", libzfs_unmount},
    // {"unmount_all", libzfs_unmount_all},
    // {"mount", libzfs_mount},
    // {"mount_all", libzfs_mount_all},
    {"mount_list", libzfs_mount_list},

    {"test", libzfs_test},
    {NULL, NULL}
};


#ifdef STATIC_LIB
LUAMOD_API int luaopen_libluazfs(lua_State *L)
#else
LUALIB_API int luaopen_libluazfs(lua_State *L)
#endif
{
    luaL_newlib(L, zfs);
    return 1;
}
