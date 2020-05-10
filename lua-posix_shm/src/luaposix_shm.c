#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>



static void l_pushtablestring(lua_State* L , char* key , char* value) {
    lua_pushstring(L, key);
    lua_pushstring(L, value);
    lua_settable(L, -3);
} 


static void l_pushtablenumber(lua_State* L , char* key , int value) {
    lua_pushstring(L, key);
    lua_pushnumber(L, value);
    lua_settable(L, -3);
} 


static int libposix_shm_attach(lua_State *L)
{
	const char *name = luaL_optstring(L, 1, NULL);
	int size = luaL_optnumber(L, 2, 0);
	int mode = luaL_optnumber(L, 3, 0660);
	if (name == NULL) {
		lua_pushboolean(L, 0);
	    return 1;
	}

	char shmtmp[strlen(name) + 5];
	char semtmp[strlen(name) + 5];
	snprintf(shmtmp, sizeof(shmtmp), "/shm%s", name);
	snprintf(semtmp, sizeof(semtmp), "/sem%s", name);

	sem_t *sem = NULL;
	int fd_shm = -1;
	char *ptr = NULL;
	int is_first = 0;
	if ((sem = sem_open(semtmp, 0, 0, 0)) == SEM_FAILED) {
		if (size == 0) {
			lua_pushboolean(L, 0);
		    return 1;
		}
		if ((sem = sem_open(semtmp, O_CREAT, 0660, 0)) == SEM_FAILED) {
#ifdef ZDEBUG
			printf("%s\n", "sem_open failed");
#endif
			lua_pushboolean(L, 0);
		    return 1;
		}
		is_first = 1;
	}
	if ((fd_shm = shm_open (shmtmp, 0, 0)) == -1) {
		if (size == 0) {
			sem_close(sem);
			lua_pushboolean(L, 0);
		    return 1;
		}
		if ((fd_shm = shm_open (shmtmp, O_RDWR | O_CREAT, mode)) == -1) {
#ifdef ZDEBUG
			printf("%s\n", "shm_open failed");
#endif
			sem_close(sem);
			lua_pushboolean(L, 0);
		    return 1;
		}
		if (ftruncate (fd_shm, size) == -1) {
#ifdef ZDEBUG
			printf("%s\n", "ftruncate failed");
#endif
			shm_unlink(shmtmp);
			sem_close(sem);
			lua_pushboolean(L, 0);
		    return 1;
		}
		if ((ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
	            fd_shm, 0)) == MAP_FAILED) {
#ifdef ZDEBUG
				printf("%s\n", "mmap failed");
#endif
			shm_unlink(shmtmp);
			sem_close(sem);
			lua_pushboolean(L, 0);
		    return 1;
		}
	}
	if (is_first)
		if (sem_post (sem) == -1) {
#ifdef ZDEBUG
			printf("%s\n", "sem_post failed");
#endif
			shm_unlink(shmtmp);
			sem_close(sem);
			lua_pushboolean(L, 0);
		    return 1;
		}

	lua_newtable(L);
	l_pushtablestring(L, "shm_name", shmtmp);
	l_pushtablestring(L, "sem_name", semtmp);
	l_pushtablenumber(L, "size", size);
	l_pushtablenumber(L, "mode", mode);
    return 1;
}

static int libposix_shm_read(lua_State *L)
{
	lua_settop(L, 1);
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "shm_name");
	lua_getfield(L, 1, "sem_name");
	lua_getfield(L, 1, "size");
	lua_getfield(L, 1, "mode");

	const char *shmtmp = lua_tostring(L, -4);
	const char *semtmp = lua_tostring(L, -3);
	int size = luaL_optnumber(L, -2, 0);
	int mode = luaL_optnumber(L, -1, 0660);
	if (size == 0) {
		lua_pushboolean(L, 0);
	    return 1;
	}
	int fd_shm = -1;
	if ((fd_shm = shm_open (shmtmp, O_RDONLY, 0)) == -1) {
		lua_pushboolean(L, 0);
	    return 1;
	}
	char *ptr = NULL;
	if ((ptr = mmap(NULL, size, PROT_READ, MAP_SHARED,
            fd_shm, 0)) == MAP_FAILED) {
		lua_pushboolean(L, 0);
	    return 1;
	}
	lua_pushstring(L, ptr);
    return 1;
}

static int libposix_shm_write(lua_State *L)
{
	const char *data = luaL_optstring(L, 1, NULL);
	lua_settop(L, 2);
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_getfield(L, 2, "shm_name");
	lua_getfield(L, 2, "sem_name");
	lua_getfield(L, 2, "size");
	lua_getfield(L, 2, "mode");

	const char *shmtmp = lua_tostring(L, -4);
	const char *semtmp = lua_tostring(L, -3);
	int size = luaL_optnumber(L, -2, 0);
	int mode = luaL_optnumber(L, -1, 0660);
	if (size == 0) {
		lua_pushboolean(L, 0);
	    return 1;
	}
	sem_t *sem = NULL;
	int val = 0;
	if ((sem = sem_open(semtmp, 0, 0, 0)) == SEM_FAILED) {
#ifdef ZDEBUG
		printf("%s\n", "sem_open failed");
#endif
		lua_pushboolean(L, 0);
	    return 1;
	}
	if (sem_wait(sem) == -1) {
#ifdef ZDEBUG
		printf("%s\n", "sem_wait failed");
#endif
		sem_close(sem);
		lua_pushboolean(L, 0);
	    return 1;
	}
	int fd_shm = -1;
	if ((fd_shm = shm_open (shmtmp, O_RDWR, 0)) == -1) {
#ifdef ZDEBUG
		printf("%s\n", "shm_open failed");
#endif
		sem_close(sem);
		lua_pushboolean(L, 0);
	    return 1;
	}
	char *ptr = NULL;
	if ((ptr = mmap(NULL, size, PROT_WRITE, MAP_SHARED,
            fd_shm, 0)) == MAP_FAILED) {
#ifdef ZDEBUG
		printf("%s\n", "mmap failed");
#endif
		sem_close(sem);
		lua_pushboolean(L, 0);
	    return 1;
	}
	snprintf(ptr, size, "%s", data);

	sem_post(sem);
	lua_pushboolean(L, 1);
    return 1;
}

static int libposix_shm_close(lua_State *L)
{
	lua_settop(L, 1);
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "shm_name");
	lua_getfield(L, 1, "sem_name");
	lua_getfield(L, 1, "size");
	lua_getfield(L, 1, "mode");

	const char *shmtmp = lua_tostring(L, -4);
	const char *semtmp = lua_tostring(L, -3);
	int size = luaL_optnumber(L, -2, 0);
	int mode = luaL_optnumber(L, -1, 0660);
	if (size == 0) {
		lua_pushboolean(L, 0);
	    return 1;
	}
	sem_t *sem = NULL;
	if ((sem = sem_open(semtmp, 0, 0, 0)) == SEM_FAILED) {
		lua_pushboolean(L, 0);
	    return 1;
	}
	if (sem_wait(sem) == -1) {
		sem_close(sem);
		lua_pushboolean(L, 0);
	    return 1;
	}

	shm_unlink(shmtmp);
	sem_post (sem);
	sem_unlink(semtmp);
	lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg posix_shm[] = {
    {"attach", libposix_shm_attach},
    {"read", libposix_shm_read},
    {"write", libposix_shm_write},
    {"close", libposix_shm_close},
    {NULL, NULL}
};

#ifdef STATIC
LUAMOD_API int luaopen_libluaposix_shm(lua_State *L)
#else
LUALIB_API int luaopen_libluaposix_shm(lua_State *L)
#endif
{
    luaL_newlib(L, posix_shm);
    return 1;
}

// {

// 	const char *cmd = luaL_optstring(L, 1, NULL);
// 	if (cmd != NULL) {
// 		char *result = base_execs_out(cmd);
// 		if (result != NULL) {
// 		    lua_pushstring(L, result);
// 		    return 1;
// 		}
// 	}
// }