
#include "ckv.h"

#include <stdlib.h>

/* STANDARD FUNCTIONS */

/* copied from lua's base library */
static int ckv_tostring(lua_State *L) {
	luaL_checkany(L, 1);
	if (luaL_callmeta(L, 1, "__tostring"))  /* is there a metafield? */
		return 1;  /* use its value */
	switch (lua_type(L, 1)) {
	case LUA_TNUMBER:
		lua_pushstring(L, lua_tostring(L, 1));
		break;
	case LUA_TSTRING:
		lua_pushvalue(L, 1);
		break;
	case LUA_TBOOLEAN:
		lua_pushstring(L, (lua_toboolean(L, 1) ? "true" : "false"));
		break;
	case LUA_TNIL:
		lua_pushliteral(L, "nil");
		break;
	default:
		lua_pushfstring(L, "%s: %p", luaL_typename(L, 1), lua_topointer(L, 1));
		break;
	}
	return 1;
}

/* copied from lua's base library */
static int ckv_print(lua_State *L) {
	int n = lua_gettop(L);  /* number of arguments */
	int i;
	lua_getglobal(L, "tostring");
	for (i=1; i<=n; i++) {
		const char *s;
		lua_pushvalue(L, -1);  /* function to be called */
		lua_pushvalue(L, i);   /* value to print */
		lua_call(L, 1, 1);
		s = lua_tostring(L, -1);  /* get result */
		if (s == NULL)
			return luaL_error(L, LUA_QL("tostring") " must return a string to "
			                     LUA_QL("print"));
		if (i>1) fputs("\t", stdout);
		fputs(s, stdout); /* TODO: make non-blocking if necessary */
		lua_pop(L, 1);  /* pop result */
	}
	fputs("\n", stdout);
	return 0;
}

static int ckv_type(lua_State *L) {
	luaL_checkany(L, 1);
	lua_pushstring(L, luaL_typename(L, 1));
	return 1;
}


static int ckv_next(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
	if (lua_next(L, 1))
		return 2;
	else {
		lua_pushnil(L);
		return 1;
	}
}

static int ckv_pairs (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, lua_upvalueindex(1));  /* return generator, */
  lua_pushvalue(L, 1);  /* state, */
  lua_pushnil(L);  /* and initial value */
  return 3;
}

static int ipairsaux (lua_State *L) {
  int i = luaL_checkint(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);
  i++;  /* next value */
  lua_pushinteger(L, i);
  lua_rawgeti(L, 1, i);
  return (lua_isnil(L, -1)) ? 0 : 2;
}


static int ckv_ipairs (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushvalue(L, lua_upvalueindex(1));  /* return generator, */
  lua_pushvalue(L, 1);  /* state, */
  lua_pushinteger(L, 0);  /* and initial value */
  return 3;
}

static int ckv_now(lua_State *L) {
	lua_pushnumber(L, now(get_thread(L)));
	return 1;
}

static int ckv_fork(lua_State *L) {
	fork_child(get_thread(L));
	return 0;
}

static int ckv_fork_eval(lua_State *L) {
	fork_child_with_eval(get_thread(L));
	return 0;
}

static int ckv_yield(lua_State *L) {
	return lua_yield(L, lua_gettop(L));
}

/* LIBRARY REGISTRATION */

static const luaL_Reg ckvlib[] = {
	{ "tostring", ckv_tostring },
	{ "print", ckv_print },
	{ "type", ckv_type },
	{ "next", ckv_next },
	{ "now", ckv_now },
	{ "fork", ckv_fork },
	{ "fork_eval", ckv_fork_eval },
	{ "yield", ckv_yield },
	{ NULL, NULL }
};

static void auxopen (lua_State *L, const char *name,
                     lua_CFunction f, lua_CFunction u) {
	lua_pushcfunction(L, u);
	lua_pushcclosure(L, f, 1);
	lua_setfield(L, -2, name);
}

/* opens ckv library */
int open_ckv(lua_State *L) {
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	luaL_register(L, NULL, ckvlib);
	
	/* `ipairs' and `pairs' need auxliliary functions as upvalues */
	auxopen(L, "ipairs", ckv_ipairs, ipairsaux);
	auxopen(L, "pairs", ckv_pairs, ckv_next);
	
	return 1;
}
