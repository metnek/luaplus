#include <sys/param.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <fcntl.h>
#include <db.h>
#include <kenv.h>

#define SPWD_UID_MIN_KENV			"kcs.spwd.start_position"
#define SPWD_UID_MAX				65534

#define SPWD_PASSWD_PREFIX			"$6$"
#define SPWD_PASSWD_SALTLEN			16

typedef struct spwd_user_info {
	struct passwd pstore;
	int32_t pw_expire;
	int32_t pw_change;
} spwd_user_info_t;

#define SPWD_PWD_LEN				106
#define SPWD_RAND_STR_LEN			64
#define PHP_SPWD_CLASSNAME_MAX		64

#define SPWD_ADD_ENTRY				0
#define SPWD_SET_ENTRY				1

#define SPWD_GET_CH					0
#define SPWD_GET_ALL				1

#define SPWD_GET_BY_NAME 			0
#define SPWD_GET_BY_UID 			1
#define SPWD_GET_BY_NUM 			2


#define CURRENT_VERSION(x) 			_PW_VERSIONED(x, 4)
#define EXPAND(e) 					e = dp; while (*dp++) ;
#define SCALAR(v)					memmove(&(v), dp, sizeof(v)); dp += sizeof(v); v = ntohl(v);
#define	COMPACT(e)					lp = e; while ((*dp++ = *lp++));
#define SCALAR1(e)					store = htonl((uint32_t)(e));      \
									memmove(dp, &store, sizeof(store)); \
									dp += sizeof(store);

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

static void
spwd_gen_rand_string(char *str, size_t len, unsigned int offset)
{
	char charset[] = "0123456789"
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (offset < len)
    	str += offset;

	while (len-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *str++ = charset[index];
    }
    *str = '\0';
}

static char *
spwd_get_db_raw(void *find, int type)
{
	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		return NULL;
	}

	if (find == NULL) {
		dbp->close(dbp);
		return NULL;
	}

	int ret = 0;
	char verskey[] = _PWD_VERSION_KEY;
	int use_version;
	DBT key, data;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = verskey;
	key.size = sizeof(verskey)-1;

	char keybuf[MAX(MAXPATHLEN, LINE_MAX * 2)];

	if ((dbp->get)(dbp, &key, &data, 0) == 0)
		use_version = *(unsigned char *)data.data;
	else
		use_version = 3;

	if (type == SPWD_GET_BY_NAME) {
		char *name = (char *)find;
		keybuf[0] = _PW_VERSIONED(_PW_KEYBYNAME, use_version);
		size_t len = strlen(name);
		memmove(keybuf + 1, name, MIN(len, sizeof(keybuf) - 1));
		key.size = len + 1;
	} else if (type == SPWD_GET_BY_UID || type == SPWD_GET_BY_NUM) {
		uint32_t store;

		if (type == SPWD_GET_BY_UID)
			keybuf[0] = _PW_VERSIONED(_PW_KEYBYUID, use_version);
		else
			keybuf[0] = _PW_VERSIONED(_PW_KEYBYNUM, use_version);

		store = htonl(*((uid_t *)find));
		memmove(keybuf + 1, &store, sizeof(store));
		key.size = sizeof(int) + 1;
	} else {
		dbp->close(dbp);
		return NULL;
	}

	key.data = keybuf;
	ret = (dbp->get)(dbp, &key, &data, 0);
	if (ret != 0) {
		dbp->close(dbp);
		return NULL;
	}

	char *buf = malloc(data.size);
	if (buf == NULL) {
		dbp->close(dbp);
		return NULL;
	}

	memmove(buf, data.data, data.size);

	dbp->close(dbp);

	return buf;
}

static int
spwd_get_pwd(spwd_user_info_t *uinfo, void *find, int type)
{
	char *raw = spwd_get_db_raw(find, type);

	if (raw == NULL) {
		return -1;
	}

	char *dp = raw;

	EXPAND(uinfo->pstore.pw_name);
	EXPAND(uinfo->pstore.pw_passwd);
	SCALAR(uinfo->pstore.pw_uid);
	SCALAR(uinfo->pstore.pw_gid);
	SCALAR(uinfo->pw_change);
	EXPAND(uinfo->pstore.pw_class);
	EXPAND(uinfo->pstore.pw_gecos);
	EXPAND(uinfo->pstore.pw_dir);
	EXPAND(uinfo->pstore.pw_shell);
	SCALAR(uinfo->pw_expire);
	bcopy(dp, (char *)&uinfo->pstore.pw_fields, sizeof(uinfo->pstore.pw_fields));

	return 0;
}

static int
spwd_fill_user_info(lua_State *L, void *find, int type)
{
	spwd_user_info_t uinfo;
	memset(&uinfo, 0, sizeof(spwd_user_info_t));
	if (!spwd_get_pwd(&uinfo, find, type)) {
		l_pushtablestring(L, "name", uinfo.pstore.pw_name);
		l_pushtableinteger(L, "uid", uinfo.pstore.pw_uid);
		l_pushtableinteger(L, "gid", uinfo.pstore.pw_gid);
		l_pushtableinteger(L, "change", uinfo.pw_change);
		l_pushtablestring(L, "class", uinfo.pstore.pw_class);
		l_pushtablestring(L, "gecos", uinfo.pstore.pw_gecos);
		l_pushtablestring(L, "dir", uinfo.pstore.pw_dir);
		l_pushtablestring(L, "shell", uinfo.pstore.pw_shell);
		l_pushtableinteger(L, "uid", uinfo.pstore.pw_uid);
		l_pushtableinteger(L, "expire", uinfo.pw_expire);
		free(uinfo.pstore.pw_name);
		return 0;
	}
	return -1;
}

static char *
spwd_get_password(const char *name)
{
	spwd_user_info_t uinfo;
	memset(&uinfo, 0, sizeof(spwd_user_info_t));
	if (spwd_get_pwd(&uinfo, (char *)name, SPWD_GET_BY_NAME)) {
		return NULL;
	}
	char *ret = malloc(strlen(uinfo.pstore.pw_passwd) + 1);
	if (ret == NULL) {
		free(uinfo.pstore.pw_name);
		return NULL;
	}
	snprintf(ret, strlen(uinfo.pstore.pw_passwd) + 1, "%s", uinfo.pstore.pw_passwd);
	free(uinfo.pstore.pw_name);
	return ret;
}

static int
spwd_del(const char *pw_name, int pw_uid, int num)
{
	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		return -1;
	}

	uint32_t store;
	char tbuf[1024];
	DBT key;
	unsigned int len;

	key.data = (u_char *)tbuf;

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNAME);
	len = strlen(pw_name);
	memmove(tbuf + 1, pw_name, len);
	key.size = len + 1;
	int ret = 0;
	if ((ret = (dbp->del)(dbp, &key, 0)) == -1) {
		dbp->close(dbp);
		return -1;
	}
	if (pw_uid != -1) {
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYUID);
		store = htonl(pw_uid);
		memmove(tbuf + 1, &store, sizeof(store));
		key.size = sizeof(store) + 1;
		if ((dbp->del)(dbp, &key, 0) == -1) {
			dbp->close(dbp);
			return -1;
		}
	}
	if (num != -1) {
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
		store = htonl(num);
		memmove(tbuf + 1, &store, sizeof(store));
		key.size = sizeof(store) + 1;
		if ((dbp->del)(dbp, &key, 0) == -1) {
			dbp->close(dbp);
			return -1;
		}
		DBT k, d;
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
		store = htonl(++num);
		memmove(tbuf + 1, &store, sizeof(store));
		k.size = sizeof(store) + 1;
		k.data = (u_char *)tbuf;

		while (dbp->get(dbp, &k, &d, 0) == 0) {
			memset(tbuf, 0, sizeof(tbuf));
			tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
			store = htonl(num-1);
			memmove(tbuf + 1, &store, sizeof(store));
			k.size = sizeof(store) + 1;
			dbp->put(dbp, &k, &d, 0);
			memset(tbuf, 0, sizeof(tbuf));
			tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
			store = htonl(++num);
			memmove(tbuf + 1, &store, sizeof(store));
			k.size = sizeof(store) + 1;
			k.data = (u_char *)tbuf;
		}
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
		store = htonl(num-1);
		memmove(tbuf + 1, &store, sizeof(store));
		k.size = sizeof(store) + 1;
		k.data = (u_char *)tbuf;
		(dbp->del)(dbp, &k, 0);
	}
	dbp->close(dbp);
	return 0;
}

static int libspwd_get(lua_State *L)
{
	if (lua_gettop(L) != 1) {
	    lua_pushnil(L);
	    return 1;
	}

	int type = SPWD_GET_BY_NAME;
	const void *find = NULL;
	uid_t uid = -1;

	if (lua_isinteger(L, 1)) {
		type = SPWD_GET_BY_UID;
		uid = luaL_optnumber(L, 1, 0);
		find = &uid;
	} else if (lua_isstring(L, 1)) {
		find = luaL_optstring(L, 1, NULL);
	} else {
	    lua_pushnil(L);
	    return 1;
	}

	lua_newtable( L );
	int ret = spwd_fill_user_info(L, (char *)find, type);

	if (ret) {
        lua_pop(L, 1);
	    lua_pushnil(L);
	}
    return 1;
}

static int libspwd_get_all(lua_State *L)
{
	if (lua_gettop(L)) {
	    lua_pushnil(L);
	    return 1;
	}
	
	int count = 1;
	lua_newtable( L );
	int ret = 0;
	do {
		lua_pushinteger(L, count);
		lua_newtable( L );

		ret = spwd_fill_user_info(L, &count, SPWD_GET_BY_NUM);
		if (ret && count == 1) {
			lua_pop(L, 3);
		    lua_pushnil(L);
			return 1;
		} else if (ret) {
			lua_pop(L, 2);
		} else {
		    lua_settable(L, -3);
		}
		count++;
	} while (!ret);

    return 1;
}

static int libspwd_verify(lua_State *L)
{
	const char *username = luaL_optstring(L, 1, NULL);
	const char *pwd = luaL_optstring(L, 2, NULL);

	if (username != NULL && pwd != NULL) {
		char *db_pwd = spwd_get_password(username);
		if (db_pwd != NULL) {
			if (strcmp(db_pwd, crypt(pwd, db_pwd)) == 0) {
				free(db_pwd);
				lua_pushboolean(L, 1);
				return 1;
			}
		}
	}
    lua_pushnil(L);
    return 1;
}

static int libspwd_delete(lua_State *L)
{
	int pw_uid = luaL_optnumber(L, 1, -1);
	if (pw_uid == -1) {
	    lua_pushnil(L);
	    return 1;
	}

	int count = 1;
	int ret = 0;
	spwd_user_info_t uinfo;
	uinfo.pstore.pw_name = NULL;
	do {
		if (uinfo.pstore.pw_name != NULL)
			free(uinfo.pstore.pw_name);
		memset(&uinfo, 0, sizeof(spwd_user_info_t));
		ret = spwd_get_pwd(&uinfo, &count, SPWD_GET_BY_NUM);
		count++;
	} while (!ret && pw_uid != uinfo.pstore.pw_uid);

	if (ret) {
	    lua_pushnil(L);
	    return 1;
	}
	count--;
	if (spwd_del(uinfo.pstore.pw_name, pw_uid, count)) {
	    lua_pushnil(L);
	    return 1;
	}
	lua_pushboolean(L, 1);
	return 1;
}

#define SPWD_CHECK_TABLE_STR(v, e) \
	if (strcmp(#v, lua_tostring(L, -2)) == 0) \
		e.pw_##v = (char *)lua_tostring(L, -1);

#define SPWD_CHECK_TABLE_NUM1(v, e) \
	if (strcmp(#v, lua_tostring(L, -2)) == 0) \
		e.pw_##v = lua_tonumber(L, -1);

#define SPWD_CHECK_TABLE_NUM2(v, e) \
	if (strcmp(#v, lua_tostring(L, -2)) == 0) \
		e->pw_##v = lua_tonumber(L, -1);

static void
spwd_fill_user_info_from_table(lua_State *L, spwd_user_info_t *uinfo)
{
    lua_pushnil(L);
    while(lua_next(L, -2) != 0) {
    	SPWD_CHECK_TABLE_STR(name, uinfo->pstore);
    	SPWD_CHECK_TABLE_STR(passwd, uinfo->pstore);
    	SPWD_CHECK_TABLE_NUM1(uid, uinfo->pstore);
    	SPWD_CHECK_TABLE_NUM1(gid, uinfo->pstore);
    	SPWD_CHECK_TABLE_NUM2(change, uinfo);
    	SPWD_CHECK_TABLE_STR(class, uinfo->pstore);
    	SPWD_CHECK_TABLE_STR(gecos, uinfo->pstore);
    	SPWD_CHECK_TABLE_STR(dir, uinfo->pstore);
    	SPWD_CHECK_TABLE_STR(shell, uinfo->pstore);
    	SPWD_CHECK_TABLE_NUM2(expire, uinfo);
        lua_pop(L, 1);
    }

	uinfo->pstore.pw_fields = _PWF_NAME | _PWF_PASSWD | _PWF_UID | _PWF_GID | _PWF_EXPIRE | _PWF_CHANGE;
	if (uinfo->pstore.pw_class[0])
		uinfo->pstore.pw_fields |= _PWF_CLASS;
	if (uinfo->pstore.pw_gecos[0])
		uinfo->pstore.pw_fields |= _PWF_GECOS;
	if (uinfo->pstore.pw_dir[0])
		uinfo->pstore.pw_fields |= _PWF_DIR;
	if (uinfo->pstore.pw_shell[0])
		uinfo->pstore.pw_fields |= _PWF_SHELL;

}

static int
spwd_write(spwd_user_info_t *uinfo, int num)
{
	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		return -1;
	}

	uint32_t store;
	const char *lp;
	char *dp;
	DBT key, data;
	char verskey[] = _PWD_VERSION_KEY;
	int use_version;
	char buf[MAX(MAXPATHLEN, LINE_MAX * 2)];
	char tbuf[1024];
	unsigned int len;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = verskey;
	key.size = sizeof(verskey)-1;

	if ((dbp->get)(dbp, &key, &data, 0) == 0)
		use_version = *(unsigned char *)data.data;
	else
		use_version = 3;

	dp = buf;
	COMPACT(uinfo->pstore.pw_name);
	COMPACT(uinfo->pstore.pw_passwd);
	SCALAR1(uinfo->pstore.pw_uid);
	SCALAR1(uinfo->pstore.pw_gid);
	SCALAR1(uinfo->pstore.pw_change);
	COMPACT(uinfo->pstore.pw_class);
	COMPACT(uinfo->pstore.pw_gecos);
	COMPACT(uinfo->pstore.pw_dir);
	COMPACT(uinfo->pstore.pw_shell);
	SCALAR1(uinfo->pstore.pw_expire);
	SCALAR1(uinfo->pstore.pw_fields);

	data.size = dp - buf;
	data.data = (u_char *)buf;
	key.data = (u_char *)tbuf;

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNAME);
	len = strlen(uinfo->pstore.pw_name);
	memmove(tbuf + 1, uinfo->pstore.pw_name, len);
	key.size = len + 1;
	if ((dbp->put)(dbp, &key, &data, 0) == -1) {
		dbp->close(dbp);
		return -1;
	}

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYUID);
	store = htonl(uinfo->pstore.pw_uid);
	memmove(tbuf + 1, &store, sizeof(store));
	key.size = sizeof(store) + 1;
	if ((dbp->put)(dbp, &key, &data, 0) == -1) {
		dbp->close(dbp);
		return -1;
	}

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
	store = htonl(num);
	memmove(tbuf + 1, &store, sizeof(store));
	key.size = sizeof(store) + 1;
	if ((dbp->put)(dbp, &key, &data, 0) == -1) {
		dbp->close(dbp);
		return -1;
	}

	dbp->close(dbp);
	return 0;
}

static int libspwd_add(lua_State *L)
{
	char kenv_buf[KENV_MVALLEN + 1];
	if (kenv(KENV_GET, SPWD_UID_MIN_KENV, kenv_buf, sizeof(kenv_buf)) <= 0) {
	    lua_pushnil(L);
	    return 1;
	}
	int kenv_uid = (int)strtol(kenv_buf, NULL, 10);
	if (kenv_uid <= 0) {
	    lua_pushnil(L);
	    return 1;
	}

	spwd_user_info_t uinfo;
	memset(&uinfo, 0, sizeof(spwd_user_info_t));

	/*set default values*/
	uinfo.pstore.pw_name = NULL;
	uinfo.pstore.pw_passwd = "*";
	uinfo.pstore.pw_shell = "/usr/sbin/nologin";
	uinfo.pstore.pw_dir = "/nonexistent";
	uinfo.pstore.pw_gecos = "";
	uinfo.pstore.pw_class = "";
	uinfo.pstore.pw_uid = SPWD_UID_MAX;
	uinfo.pstore.pw_gid = SPWD_UID_MAX;
	uinfo.pw_expire = 0;
	uinfo.pw_change = 0;

	spwd_fill_user_info_from_table(L, &uinfo);

	if (uinfo.pstore.pw_name == NULL) {				/* name is required*/
	    lua_pushnil(L);
	    return 1;
	}

	int ret = 0;

	spwd_user_info_t tmp_uinfo;

	ret = spwd_get_pwd(&tmp_uinfo, uinfo.pstore.pw_name, SPWD_GET_BY_NAME);

	if (!ret) { /*already exists*/
		free(tmp_uinfo.pstore.pw_name);
	    lua_pushnil(L);
	    return 1;
	}

	int max_uid = -1;
	int count = 0;
	do {
		count++;
		if (tmp_uinfo.pstore.pw_name != NULL)
			free(tmp_uinfo.pstore.pw_name);
		memset(&tmp_uinfo, 0, sizeof(spwd_user_info_t));
		ret = spwd_get_pwd(&tmp_uinfo, &count, SPWD_GET_BY_NUM);
		if (!ret) {
			if (tmp_uinfo.pstore.pw_uid < SPWD_UID_MAX && max_uid < (int)tmp_uinfo.pstore.pw_uid)
				max_uid = tmp_uinfo.pstore.pw_uid;
		}
	} while (!ret);

	max_uid++;
	if (max_uid < kenv_uid)
		max_uid = kenv_uid;

	if (uinfo.pstore.pw_uid == SPWD_UID_MAX) {
		uinfo.pstore.pw_uid = max_uid;
	}

	if (uinfo.pstore.pw_gid == SPWD_UID_MAX) { 				/*set gid to uid by default*/
		uinfo.pstore.pw_gid = uinfo.pstore.pw_uid;
	}

	char salt[21];
	snprintf(salt, sizeof(salt), "%s", SPWD_PASSWD_PREFIX);
	spwd_gen_rand_string(salt, SPWD_PASSWD_SALTLEN, strlen(SPWD_PASSWD_PREFIX));
	snprintf(salt, sizeof(salt), "%s$", salt);

	if (*uinfo.pstore.pw_passwd != '*' || strlen(uinfo.pstore.pw_passwd) != 1) {
		uinfo.pstore.pw_passwd = crypt(uinfo.pstore.pw_passwd, salt);
	}

#if 0
	printf("uinfo.pstore.pw_name %s\n", uinfo.pstore.pw_name);
	printf("uinfo.pstore.pw_passwd %s\n", uinfo.pstore.pw_passwd);
	printf("uinfo.pstore.pw_shell %s\n", uinfo.pstore.pw_shell);
	printf("uinfo.pstore.pw_dir %s\n", uinfo.pstore.pw_dir);
	printf("uinfo.pstore.pw_gecos %s\n", uinfo.pstore.pw_gecos);
	printf("uinfo.pstore.pw_class %s\n", uinfo.pstore.pw_class);
	printf("uinfo.pstore.pw_uid %d\n", uinfo.pstore.pw_uid);
	printf("uinfo.pstore.pw_gid %d\n", uinfo.pstore.pw_gid);
	printf("uinfo.pw_expire %d\n", uinfo.pw_expire);
	printf("uinfo.pw_change %d\n", uinfo.pw_change);
#endif

	ret = spwd_write(&uinfo, count); /*count already last+1, no need to increment*/
	if (ret)
	    lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
    return 1;
}

static int libspwd_update(lua_State *L)
{
	int pw_uid = luaL_optnumber(L, 1, -1);
	if (pw_uid == -1) { /*uid is required*/
	    lua_pushnil(L);
	    return 1;
	}

	if (lua_gettop(L) == 1) { /*nothing to do*/
		lua_pushboolean(L, 1);
	    return 1;
	}

	int ret = 0;
	spwd_user_info_t uinfo;
	spwd_user_info_t old_uinfo;
	memset(&uinfo, 0, sizeof(spwd_user_info_t));
	memset(&old_uinfo, 0, sizeof(spwd_user_info_t));

	/*get old values*/
	ret = spwd_get_pwd(&old_uinfo, &pw_uid, SPWD_GET_BY_UID);
	if (ret) { /*not found, must use add instead*/
	    lua_pushnil(L);
	    return 1;
	}
	uinfo.pstore.pw_name = old_uinfo.pstore.pw_name;
	uinfo.pstore.pw_passwd = old_uinfo.pstore.pw_passwd;
	uinfo.pstore.pw_shell = old_uinfo.pstore.pw_shell;
	uinfo.pstore.pw_dir = old_uinfo.pstore.pw_dir;
	uinfo.pstore.pw_gecos = old_uinfo.pstore.pw_gecos;
	uinfo.pstore.pw_class = old_uinfo.pstore.pw_class;
	uinfo.pstore.pw_uid = pw_uid;
	uinfo.pstore.pw_gid = old_uinfo.pstore.pw_gid;
	uinfo.pw_expire = old_uinfo.pw_expire;
	uinfo.pw_change = old_uinfo.pw_change;


	spwd_fill_user_info_from_table(L, &uinfo);
	if ((int)uinfo.pstore.pw_uid != pw_uid) { /*uid can not be changed*/
		printf("%s %u.\n", "warn: ignoring new uid", uinfo.pstore.pw_uid);
		uinfo.pstore.pw_uid = pw_uid;
	}

	if (strcmp(uinfo.pstore.pw_name, old_uinfo.pstore.pw_name) != 0) {
		if (spwd_del(old_uinfo.pstore.pw_name, -1, -1)) { /*delete where key is old name*/
			free(old_uinfo.pstore.pw_name);
		    lua_pushnil(L);
		    return 1;
		}
	}
	char salt[21];
	snprintf(salt, sizeof(salt), "%s", SPWD_PASSWD_PREFIX);
	spwd_gen_rand_string(salt, SPWD_PASSWD_SALTLEN, strlen(SPWD_PASSWD_PREFIX));
	snprintf(salt, sizeof(salt), "%s$", salt);

	if ((*uinfo.pstore.pw_passwd != '*' || strlen(uinfo.pstore.pw_passwd) != 1) &&
		 uinfo.pstore.pw_passwd != old_uinfo.pstore.pw_passwd) { /*not still look at old_uinfo.pstore.pw_passwd address*/
		uinfo.pstore.pw_passwd = crypt(uinfo.pstore.pw_passwd, salt);
	}

	spwd_user_info_t tmp_uinfo;
	memset(&tmp_uinfo, 0, sizeof(spwd_user_info_t));
	int count = 0;
	do {
		count++;
		if (tmp_uinfo.pstore.pw_name != NULL)
			free(tmp_uinfo.pstore.pw_name);
		memset(&tmp_uinfo, 0, sizeof(spwd_user_info_t));
		ret = spwd_get_pwd(&tmp_uinfo, &count, SPWD_GET_BY_NUM);
	} while (!ret && uinfo.pstore.pw_uid != tmp_uinfo.pstore.pw_uid);

	if (ret) {
		free(old_uinfo.pstore.pw_name);
	    lua_pushnil(L);
	    return 1;
	}
	free(tmp_uinfo.pstore.pw_name);

#if 0
	printf("uinfo.pstore.pw_name %s\n", uinfo.pstore.pw_name);
	printf("uinfo.pstore.pw_passwd %s\n", uinfo.pstore.pw_passwd);
	printf("uinfo.pstore.pw_shell %s\n", uinfo.pstore.pw_shell);
	printf("uinfo.pstore.pw_dir %s\n", uinfo.pstore.pw_dir);
	printf("uinfo.pstore.pw_gecos %s\n", uinfo.pstore.pw_gecos);
	printf("uinfo.pstore.pw_class %s\n", uinfo.pstore.pw_class);
	printf("uinfo.pstore.pw_uid %d\n", uinfo.pstore.pw_uid);
	printf("uinfo.pstore.pw_gid %d\n", uinfo.pstore.pw_gid);
	printf("uinfo.pw_expire %d\n", uinfo.pw_expire);
	printf("uinfo.pw_change %d\n", uinfo.pw_change);
#endif

	ret = spwd_write(&uinfo, count);
	free(old_uinfo.pstore.pw_name);
	if (ret)
	    lua_pushnil(L);
	else
		lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg spwd[] = {
    {"get", libspwd_get},
    {"get_all", libspwd_get_all},
    {"verify", libspwd_verify},
    {"delete", libspwd_delete},
    {"add", libspwd_add},
    {"update", libspwd_update},
    {NULL, NULL}
};

#ifdef STATIC
LUAMOD_API int luaopen_libluaspwd(lua_State *L)
#else
LUALIB_API int luaopen_libluaspwd(lua_State *L)
#endif
{
    luaL_newlib(L, spwd);
    return 1;
}