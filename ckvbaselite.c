
#include "ckv.h"

#include <stdlib.h>

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
	fflush(stdout);
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

/* LIBRARY REGISTRATION */

static const luaL_Reg ckvbaselite[] = {
	{ "tostring", ckv_tostring },
	{ "print", ckv_print },
	{ "type", ckv_type },
	{ "next", ckv_next },
	{ NULL, NULL }
};

static void auxopen (lua_State *L, const char *name,
                     lua_CFunction f, lua_CFunction u) {
	lua_pushcfunction(L, u);
	lua_pushcclosure(L, f, 1);
	lua_setfield(L, -2, name);
}

/* opens library */
int open_ckvbaselite(lua_State *L) {
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	luaL_register(L, NULL, ckvbaselite);
	
	/* `ipairs' and `pairs' need auxliliary functions as upvalues */
	auxopen(L, "ipairs", ckv_ipairs, ipairsaux);
	auxopen(L, "pairs", ckv_pairs, ckv_next);
	
	return 1;
}
