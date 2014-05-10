/*
** LuaProcessCall
** Copyright DarkGod 2007
** lua 5.1/5.2 compat (c) 2014 Timo Teräs
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#if LUA_VERSION_NUM < 502

static int lua_absindex(lua_State *L, int idx) {
	return (idx > 0 || idx <= LUA_REGISTRYINDEX)? idx : lua_gettop(L) + idx + 1;
} /* lua_absindex() */

static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
	int i, t = lua_absindex(L, -1 - nup);

	for (; l->name; l++) {
		for (i = 0; i < nup; i++)
			lua_pushvalue(L, -nup);
		lua_pushcclosure(L, l->func, nup);
		lua_setfield(L, t, l->name);
	}

	lua_pop(L, nup);
} /* luaL_setfuncs() */

#define luaL_newlibtable(L, l) \
	lua_createtable(L, 0, (sizeof (l) / sizeof *(l)) - 1)

#define luaL_newlib(L, l) \
	(luaL_newlibtable((L), (l)), luaL_setfuncs((L), (l), 0))

#ifndef LUA_FILEHANDLE /* Not defined by earlier LuaJIT releases */
#define LUA_FILEHANDLE "FILE*"
#endif

/*
 * Lua 5.1 userdata is a simple FILE *, while LuaJIT is a struct with the
 * first member a FILE *, similar to Lua 5.2.
 */
typedef struct luaL_Stream {
	FILE *f;
} luaL_Stream;

static int luaL_fileresult(lua_State *L, int stat, const char *filename) {
	int en = errno;  /* calls to Lua API may change this value */
	if (stat) {
		lua_pushboolean(L, 1);
		return 1;
	}
	else {
		lua_pushnil(L);
		if (filename)
			lua_pushfstring(L, "%s: %s", filename, strerror(en));
		else
			lua_pushfstring(L, "%s", strerror(en));
		lua_pushinteger(L, en);
		return 3;
	}
}

#define isclosed(p) ((p)->f == NULL)
#define markclosed(p) ((p)->f = NULL)

#else

#define isclosed(p) ((p)->closef == NULL)
#define markclosed(p) ((p)->closef = NULL)

#endif

#define tolstream(L) ((luaL_Stream *)luaL_checkudata(L, 1, LUA_FILEHANDLE))

static FILE *tofile (lua_State *L) {
	luaL_Stream *p = tolstream(L);
	if (isclosed(p))
		luaL_error(L, "attempt to use a closed file");
	return p->f;
}

static int io_fclose (lua_State *L)
{
	luaL_Stream *p = tolstream(L);
	int res = fclose(p->f);
	markclosed(p);
	return luaL_fileresult(L, (res == 0), NULL);
}

static luaL_Stream *newfile_fd(lua_State *L, int fd, const char *mode)
{
	luaL_Stream *p = (luaL_Stream *) lua_newuserdata(L, sizeof(luaL_Stream));
	markclosed(p);
	luaL_getmetatable(L, LUA_FILEHANDLE);
	lua_setmetatable(L, -2);
	p->f = fdopen(fd, mode);
#if LUA_VERSION_NUM >= 502
	p->closef = &io_fclose;
#endif
	return p;
}

static int lpc_run(lua_State *L)
{
	int p_out[2];
	int p_in[2];
	int pid;

	if (pipe(p_out) == -1)		goto err_noclose;
	if (pipe(p_in) == -1)		goto err_closeout;
	if ((pid = fork()) == -1)	goto err_closeinout;

	if (pid == 0) {
		/* child */
		char **args;
		int n = lua_gettop(L);  /* number of arguments */
		int i;
		args = malloc((n + 1) * sizeof(char*));
		for (i = 1; i <= n; i++)
		{
			args[i - 1] = (char*)luaL_checkstring(L, i);
		}
		args[n] = NULL;

		close(p_out[1]);
		close(p_in[0]);
		dup2(p_out[0], 0);
		dup2(p_in[1], 1);
		close(p_out[0]);
		close(p_in[1]);

		execvp(args[0], args);

		perror("LPC child error");
		_exit(1);
		return 0;
	}

	/* Cleanup */
	close(p_out[0]);
	close(p_in[1]);
	lua_pushnumber(L, pid);
	newfile_fd(L, p_out[1], "w");
	newfile_fd(L, p_in[0], "r");
	return 3;

err_closeinout:
	close(p_in[0]);
	close(p_in[1]);
err_closeout:
	close(p_out[0]);
	close(p_out[1]);
err_noclose:
	lua_pushnil(L);
	return 1;
}

static int lpc_wait(lua_State *L)
{
	int ret;
	int pid = luaL_checkinteger(L, 1);
	int nonblock = luaL_optinteger(L, 2, 0);

	if (waitpid(pid, &ret, (nonblock == 1) ? WNOHANG : 0) == pid)
		lua_pushnumber(L, ret);
	else
		lua_pushnil(L);
	return 1;
}

static int f_readable(lua_State *L)
{
	FILE *f = tofile(L);
	struct timeval tv;
	fd_set rfds;
	int retval;

	/* Wait up to five seconds. */
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&rfds);
	FD_SET(fileno(f), &rfds);
	retval = select(1 + fileno(f), &rfds, NULL, NULL, &tv);
	if (retval == 1)
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);
	return 1;
}

static int f_writable(lua_State *L)
{
	FILE *f = tofile(L);
	struct timeval tv;
	fd_set wfds;
	int retval;

	/* Wait up to five seconds. */
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&wfds);
	FD_SET(fileno(f), &wfds);
	retval = select(1 + fileno(f), NULL, &wfds, NULL, &tv);
	if (retval == 1)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);
	return 1;
}

/*
** Assumes the table is on top of the stack.
*/
static void set_info (lua_State *L)
{
	lua_pushliteral (L, "_COPYRIGHT");
	lua_pushliteral (L, "Copyright (C) 2007 DarkGod");
	lua_settable (L, -3);
	lua_pushliteral (L, "_DESCRIPTION");
	lua_pushliteral (L, "LPC allows to run external processes and catch both input and output streams");
	lua_settable (L, -3);
	lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, "LPC 1.0.0");
	lua_settable (L, -3);
}

static const struct luaL_Reg lpclib[] =
{
	{"run", lpc_run},
	{"wait", lpc_wait},
	{NULL, NULL},
};

static const luaL_Reg io_add_flib[] = {
	{"readable", f_readable},
	{"writable", f_writable},
	{NULL, NULL}
};

static void createmeta(lua_State *L)
{
	luaL_getmetatable(L, LUA_FILEHANDLE);  /* get IO's metatable for file handles */
	luaL_setfuncs(L, io_add_flib, 0);  /* file methods */
}

int luaopen_lpc (lua_State *L)
{
	createmeta(L);

#if LUA_VERSION_NUM < 502
	luaL_openlib(L, "lpc", lpclib, 0);

	/* create environment for 'run' */
	lua_getfield(L, -1, "run");
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, io_fclose);
	lua_setfield(L, -2, "__close");
	lua_setfenv(L, -2);
	lua_pop(L, 1);  /* pop 'run' */
#else
	luaL_newlib(L, lpclib);
#endif

	set_info(L);
	return 1;
}
