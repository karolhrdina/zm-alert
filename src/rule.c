/*  =========================================================================
    rule - class representing one rule

    Copyright (C) 2016 - 2017 Tomas Halman                                 
                                                                           
    This program is free software; you can redistribute it and/or modify   
    it under the terms of the GNU General Public License as published by   
    the Free Software Foundation; either version 2 of the License, or      
    (at your option) any later version.                                    
                                                                           
    This program is distributed in the hope that it will be useful,        
    but WITHOUT ANY WARRANTY; without even the implied warranty of         
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
    GNU General Public License for more details.                           
                                                                           
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.            
    =========================================================================
*/

/*
@header
    rule - class representing one rule
@discuss
@end
*/

#include "fty_alert_flexible_classes.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

//  Structure of our class

struct _rule_t {
    char *name;
    char *description;
    zlist_t *metrics;
    zlist_t *assets;
    zlist_t *groups;
    // results are not interesting
    char *evaluation;
    lua_State *lua;
};


static
int string_comparefn (void *i1, void *i2)
{
    return strcmp ((char *)i1, (char *)i2);
}

//  --------------------------------------------------------------------------
//  Create a new rule

rule_t *
rule_new (void)
{
    rule_t *self = (rule_t *) zmalloc (sizeof (rule_t));
    assert (self);
    //  Initialize class properties here
    self -> metrics = zlist_new ();
    zlist_autofree (self -> metrics);
    zlist_comparefn (self -> metrics, string_comparefn);
    self -> assets = zlist_new ();
    zlist_autofree (self -> assets);
    zlist_comparefn (self -> assets, string_comparefn);
    self -> groups = zlist_new ();
    zlist_autofree (self -> groups);
    zlist_comparefn (self -> groups, string_comparefn);
    return self;
}

//  --------------------------------------------------------------------------
//  Rule loading callback

static int
rule_json_callback (const char *locator, const char *value, void *data)
{
    if (!data) return 1;
    
    rule_t *self = (rule_t *) data;

    if (streq (locator, "name")) {
        self -> name = vsjson_decode_string (value);
    }
    else if (streq (locator, "description")) {
        self -> description = vsjson_decode_string (value);
    }
    else if (strncmp (locator, "metrics/", 7) == 0) {
        char *metric = vsjson_decode_string (value);
        zlist_append (self -> metrics, metric);
        zstr_free (&metric);
    }
    else if (strncmp (locator, "assets/", 7) == 0) {
        char *asset = vsjson_decode_string (value);
        zlist_append (self -> assets, asset);
        zstr_free (&asset);
    }
    else if (strncmp (locator, "groups/", 7) == 0) {
        char *group = vsjson_decode_string (value);
        zlist_append (self -> groups, group);
        // zlist_freefn (self -> groups, group, free, true);
        zstr_free (&group);
    }
    else if (streq (locator, "evaluation")) {
        self -> evaluation = vsjson_decode_string (value);
    }
    return 0;
}

//  --------------------------------------------------------------------------
//  Parse JSON into rule.

int rule_parse (rule_t *self, const char *json)
{
    return vsjson_parse (json, rule_json_callback, self);
}

//  --------------------------------------------------------------------------
//  Rule getters

const char *rule_name (rule_t *self)
{
    if (self) return self->name;
    return NULL;
}

zlist_t *rule_assets (rule_t *self)
{
    if (self) return self->assets;
    return NULL;
}

zlist_t *rule_groups (rule_t *self)
{
    if (self) return self->groups;
    return NULL;
}

zlist_t *rule_metrics (rule_t *self)
{
    if (self) return self->metrics;
    return NULL;
}

//  --------------------------------------------------------------------------
//  Load json rule from file

int rule_load (rule_t *self, const char *path)
{
    int fd = open (path, O_RDONLY);
    if (fd == -1) return -1;

    struct stat rstat;
    fstat (fd, &rstat);

    int capacity = rstat.st_size + 1;
    char *buffer = (char *) malloc (capacity);
    assert (buffer);
    memset (buffer, 0, capacity);

    read (fd, buffer, capacity);
    close (fd);
    int result = rule_parse (self, buffer);
    free (buffer);
    return result;
}

static int rule_compile (rule_t *self)
{
    if (!self) return 0;
    // destroy old context
    if (self -> lua) {
        lua_close (self->lua);
        self->lua = NULL;
    }
    // compile
#if LUA_VERSION_NUM > 501
    self -> lua = luaL_newstate();
#else
    self -> lua = lua_open();
#endif
    if (!self->lua) return 0;
    luaL_openlibs(self -> lua); // get functions like print();
    if (luaL_dostring (self -> lua, self -> evaluation) != 0) {
        zsys_error ("rule %s has an error", self -> name);
        lua_close (self -> lua);
        self -> lua = NULL;
        return 0;
    }
    lua_getglobal (self -> lua, "main");
    if (!lua_isfunction (self -> lua, -1)) {
        zsys_error ("main function not found in rule %s", self -> name);
        lua_close (self->lua);
        self -> lua = NULL;
        return 0;
    }
    lua_pushnumber(self -> lua, 0);
    lua_setglobal(self -> lua, "OK");
    lua_pushnumber(self -> lua, 1);
    lua_setglobal(self -> lua, "WARNING");
    lua_pushnumber(self -> lua, 1);
    lua_setglobal(self -> lua, "HIGH_WARNING");
    lua_pushnumber(self -> lua, 2);
    lua_setglobal(self -> lua, "CRITICAL");
    lua_pushnumber(self -> lua, 2);
    lua_setglobal(self -> lua, "HIGH_CRITICAL");
    lua_pushnumber(self -> lua, -1);
    lua_setglobal(self -> lua, "LOW_WARNING");
    lua_pushnumber(self -> lua, -2);
    lua_setglobal(self -> lua, "LOW_CRITICAL");
    return 1;
}    

void rule_evaluate (rule_t *self, zlist_t *params, const char *name, int *result, char **message)
{
    if (!self || !params || !name || !result || !message) return;
    
    *result = RULE_ERROR;
    *message = NULL;
    if (!self -> lua) {
        if (! rule_compile (self)) return;
    }
    lua_pushstring(self -> lua, name);
    lua_setglobal(self -> lua, "NAME");
    lua_settop (self->lua, 0);
    lua_getglobal (self->lua, "main");
    char *value = (char *) zlist_first (params);
    while (value) {
        lua_pushstring (self -> lua, value);
        value = (char *) zlist_next (params);
    }
    if (lua_pcall(self -> lua, zlist_size (params), 2, 0) == 0) {
        // calculated
        if (lua_isnumber (self -> lua, -1)) {
            *result = lua_tointeger(self -> lua, -1);
            const char *msg = lua_tostring (self->lua, -2);
            if (msg) *message = strdup (msg);
        }
        else if (lua_isnumber (self -> lua, -2)) {
            *result = lua_tointeger(self -> lua, -2);
            const char *msg = lua_tostring (self->lua, -1);
            if (msg) *message = strdup (msg);
        }
        lua_pop (self->lua, 2);
    }
}

//  --------------------------------------------------------------------------
//  Destroy the rule

void
rule_destroy (rule_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        rule_t *self = *self_p;
        //  Free class properties here
        zstr_free (&self->name);
        zstr_free (&self->description);
        zstr_free (&self->evaluation);
        if (self->lua) lua_close (self->lua);
        zlist_destroy (&self->metrics);
        zlist_destroy (&self->assets);
        zlist_destroy (&self->groups);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
vsjson_test (bool verbose)
{
    printf (" * vsjson: skip\n");
}

void
rule_test (bool verbose)
{
    printf (" * rule: ");

    //  @selftest
    //  Simple create/destroy test
    rule_t *self = rule_new ();
    assert (self);
    rule_load (self, "./rules/load.rule"); 
    rule_destroy (&self);
    //  @end
    printf ("OK\n");
}
