/*
** LuaProcessCall
** Copyright DarkGod 2007
**
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

#define topfile(L)	((FILE **)luaL_checkudata(L, 1, LUA_FILEHANDLE))

static FILE *tofile (lua_State *L) {
  FILE **f = topfile(L);
  if (*f == NULL)
    luaL_error(L, "attempt to use a closed file");
  return *f;
}

static int pushresult (lua_State *L, int i, const char *filename) {
  int en = errno;  /* calls to Lua API may change this value */
  if (i) {
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

static int io_fclose (lua_State *L) {
  FILE **p = topfile(L);
  int ok = (fclose(*p) == 0);
  *p = NULL;
  return pushresult(L, ok, NULL);
}

static FILE **newfile_fd(lua_State *L, int fd, const char *mode)
{
	FILE **pf = (FILE **)lua_newuserdata(L, sizeof(FILE *));
	*pf = NULL;  /* file handle is currently `closed' */
	luaL_getmetatable(L, LUA_FILEHANDLE);
	lua_setmetatable(L, -2);
	*pf = fdopen(fd, mode);
	return pf;
}

static int lpc_run(lua_State *L)
{
	int p_out[2];
	int p_in[2];
	int pid;

	if (pipe(p_out) == -1) { lua_pushnil(L); return 1; }
	if (pipe(p_in) == -1)  { lua_pushnil(L); return 1; }

	if ((pid = fork()) == -1) { lua_pushnil(L); return 1; }
	else if (pid == 0)
	{
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
	else
	{
		FILE **in;
		FILE **out;

		/* Cleanup */
		close(p_out[0]);
		close(p_in[1]);

		lua_pushnumber(L, pid);
		out = newfile_fd(L, p_out[1], "w");
		in = newfile_fd(L, p_in[0], "r");
		return 3;
	}
}

static int lpc_wait(lua_State *L)
{
	int ret;
	int pid = luaL_checkinteger(L, 1);
	int nonblock = luaL_optinteger(L, 2, 0);

	if (waitpid(pid, &ret, (nonblock == 1) ? WNOHANG : 0) == pid)
	{
		lua_pushnumber(L, ret);
	}
	else
	{
		lua_pushnil(L);
	}
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
	{
		lua_pushboolean(L, 1);
	}
	else
	{
		lua_pushnil(L);
	}
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
	{
		lua_pushboolean(L, 1);
	}
	else
	{
		lua_pushboolean(L, 0);
	}
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

static const struct luaL_reg lpclib[] =
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
  luaL_register(L, NULL, io_add_flib);  /* file methods */
}


int luaopen_lpc (lua_State *L)
{
	createmeta(L);

	luaL_openlib(L, "lpc", lpclib, 0);

	/* create environment for 'run' */
	lua_getfield(L, -1, "run");
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, io_fclose);
	lua_setfield(L, -2, "__close");
	lua_setfenv(L, -2);
	lua_pop(L, 1);  /* pop 'run' */

	set_info(L);
	return 1;
}
