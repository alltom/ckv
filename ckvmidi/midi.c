
#include "midi.h"

#include <stdlib.h>

struct _CKVMIDI {
	CKVM vm;
};

static int set_midi_controller(lua_State *L);

CKVMIDI
ckvmidi_open(CKVM vm)
{
	CKVMIDI midi;
	lua_State *L;
	
	midi = (CKVMIDI) malloc(sizeof(struct _CKVMIDI));
	midi->vm = vm;
	
	L = ckvm_global_state(vm);
	
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "midi_listeners");
	
	lua_pushcfunction(L, set_midi_controller);
	lua_setglobal(L, "set_midi_controller");
	
	(void) luaL_dostring(L,
	"return function(listeners, channel, msg, ...)"
	"  if listeners[channel] and listeners[channel][msg] then"
	"    listeners[channel][msg](...);"
	"  end "
	"end"
	);
	lua_setfield(L, LUA_REGISTRYINDEX, "midi_dispatch");
	
	return midi;
}

static
int
set_midi_controller(lua_State *L)
{
	int channel = luaL_checkint(L, 1); /* channel */
	
	lua_getfield(L, LUA_REGISTRYINDEX, "midi_listeners");
	lua_pushnumber(L, channel);
	lua_pushvalue(L, 2); /* controller */
	lua_rawset(L, -3);
	
	lua_pushvalue(L, 2); /* controller */
	return 1;
}

void
ckvmidi_dispatch_note_on(CKVMIDI midi, int channel, int note, float velocity)
{
	lua_State *L = ckvm_global_state(midi->vm);
	
	lua_getfield(L, LUA_REGISTRYINDEX, "midi_dispatch");
	lua_getfield(L, LUA_REGISTRYINDEX, "midi_listeners");
	lua_pushnumber(L, channel);
	lua_pushstring(L, "key_on");
	lua_pushnumber(L, note);
	lua_pushnumber(L, velocity);
	lua_call(L, 5, 0);
}

void
ckvmidi_dispatch_key_pressure(CKVMIDI midi, int channel, int note, float velocity)
{
	lua_State *L = ckvm_global_state(midi->vm);
	
	lua_getfield(L, LUA_REGISTRYINDEX, "midi_dispatch");
	lua_getfield(L, LUA_REGISTRYINDEX, "midi_listeners");
	lua_pushnumber(L, channel);
	lua_pushstring(L, "key_pressure");
	lua_pushnumber(L, note);
	lua_pushnumber(L, velocity);
	lua_call(L, 5, 0);
}

void
ckvmidi_dispatch_note_off(CKVMIDI midi, int channel, int note)
{
	lua_State *L = ckvm_global_state(midi->vm);
	
	lua_getfield(L, LUA_REGISTRYINDEX, "midi_dispatch");
	lua_getfield(L, LUA_REGISTRYINDEX, "midi_listeners");
	lua_pushnumber(L, channel);
	lua_pushstring(L, "key_off");
	lua_pushnumber(L, note);
	lua_call(L, 4, 0);
}

void
ckvmidi_dispatch_control(CKVMIDI midi, int controller, float value)
{
}

void
ckvmidi_dispatch_channel_pressure(CKVMIDI midi, float value)
{
}

void
ckvmidi_dispatch_pitch_bend(CKVMIDI midi, float value)
{
}
