
#include "ckv.h"

#include <stdlib.h>

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
		fputs(s, stdout);
		lua_pop(L, 1);  /* pop result */
	}
	fputs("\n", stdout);
	return 0;
}

static const luaL_Reg ckvlib[] = {
	{ "tostring", ckv_tostring },
	{ "print", ckv_print },
	{ NULL, NULL }
};

/* opens ckv library */
int open_ckv(lua_State *L) {
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	luaL_register(L, NULL, ckvlib);
	return 2;
}
