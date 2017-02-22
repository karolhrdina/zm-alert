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
    zlist_t *models;
    zlist_t *types;
    zhash_t *result_actions;
    zhashx_t *variables;        //  lua context global variables
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
    self -> models = zlist_new ();
    zlist_autofree (self -> models);
    zlist_comparefn (self -> models, string_comparefn);
    self -> types = zlist_new ();
    zlist_autofree (self -> types);
    zlist_comparefn (self -> types, string_comparefn);
    self -> result_actions = zhash_new ();
    zhash_autofree (self -> result_actions);
    //  variables 
    self->variables = zhashx_new ();
    zhashx_set_destructor (self->variables, (zhashx_destructor_fn *) zstr_free);

    return self;
}

//  --------------------------------------------------------------------------
//  Add rule result action
void rule_add_result_action (rule_t *self, const char *result, const char *action)
{
    if (!self || !result || !action) return;
    
    char *item = (char *) zhash_lookup (self->result_actions, result);
    if (item) {
        char *newitem = zsys_sprintf ("%s/%s", item, action);
        zhash_update (self -> result_actions, result, newitem);
        zstr_free (&newitem);
    } else {
        zhash_update (self -> result_actions, result, (void *)action);
    }
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
        zstr_free (&group);
    }
    else if (strncmp (locator, "models/", 7) == 0) {
        char *model = vsjson_decode_string (value);
        zlist_append (self -> models, model);
        zstr_free (&model);
    }
    else if (strncmp (locator, "types/", 6) == 0) {
        char *type = vsjson_decode_string (value);
        zlist_append (self -> types, type);
        zstr_free (&type);
    }
    else if (strncmp (locator, "results/", 8) == 0) {
        // results/[0/]low_warning/action/0
        char *end = strstr (locator, "/action/");
        if (end) {
            char *start = end;
            --start;
            while (*start != '/') --start;
            ++start;
            size_t size = end - start;
            char *key = (char *) zmalloc (size + 1);
            strncpy (key, start, size);
            char *action = vsjson_decode_string (value);
            rule_add_result_action (self, key, action);
            zstr_free (&key);
            zstr_free (&action);
        }
    }
    else if (streq (locator, "evaluation")) {
        self -> evaluation = vsjson_decode_string (value);
    }
    else
    if (strncmp (locator, "variables/", 10) == 0)
    {
        //  locator e.g. variables/low_critical
        char *slash = strchr (locator, '/');
        if (!slash)
            return 0;
        slash = slash + 1;
        char *variable_value = vsjson_decode_string (value);
        if (!variable_value || strlen (variable_value) == 0) {
            zstr_free (&variable_value);
            return 0;
        }
        zhashx_insert (self->variables, slash, variable_value);
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

zlist_t *rule_models (rule_t *self)
{
    if (self) return self->models;
    return NULL;
}

zlist_t *rule_types (rule_t *self)
{
    if (self) return self->types;
    return NULL;
}

const char *rule_result_actions (rule_t *self, int result)
{
    if (self) {
        char *results;
        switch (result) {
        case -2:
            results = "low_critical";
            break;
        case -1:
            results = "low_warning";
            break;
        case 0:
            results = "ok";
            break;
        case 1:
            results = "high_warning";
            break;
        case 2:
            results = "high_critical";
            break;
        default:
            results = "";
            break;
        }
        char *item = (char *) zhash_lookup (self->result_actions, results);
        if (item) {
            return item;
        }
    }
    return "";
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

    if (read (fd, buffer, capacity) == -1) {
        zsys_error ("Error while reading rule %s", path);
    }
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

    //  set global variables
    const char *item = (const char *) zhashx_first (self->variables);
    while (item) {
        const char *key = (const char *) zhashx_cursor (self->variables);
        lua_pushstring (self->lua, item);
        lua_setglobal (self->lua, key);
        item = (const char *) zhashx_next (self->variables);
    }

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
//  Create json from rule

static char * s_string_append (char **string_p, size_t *capacity, const char *append)
{
    if (! string_p) return NULL;
    if (! append) return *string_p;
    
    char *string = *string_p;
    if (!string) {
        string = (char *) zmalloc (512);
        *capacity = 512;
    }
    size_t l1 = strlen (string);
    size_t l2 = strlen (append);
    size_t required = l1+l2+1;
    if (*capacity < required) {
        size_t newcapacity = *capacity;
        while (newcapacity < required) {
            newcapacity += 512;
        }
        char *tmp = (char *) realloc (string, newcapacity);
        if (!tmp) {
            free (string);
            *capacity = 0;
            return NULL;
        }
        string = tmp;
        *capacity = newcapacity;
    }
    strncat (string, append, *capacity);
    *string_p = string;
    return string;
}

static char * s_zlist_to_json_array (zlist_t* list)
{
    if (!list) return strdup("[]");
    char *item = (char *) zlist_first (list);
    char *json = NULL;
    size_t jsonsize = 0;
    s_string_append (&json, &jsonsize, "[");
    while (item) {
        char *encoded = vsjson_encode_string (item);
        s_string_append (&json, &jsonsize, encoded);
        s_string_append (&json, &jsonsize, ", ");
        zstr_free (&encoded);
        item = (char *) zlist_next (list);
    }
    if (zlist_size (list)) {
        size_t x = strlen (json);
        json [x-2] = 0;
    }
    s_string_append (&json, &jsonsize, "]");
    return json;
}

static char * s_actions_to_json_array (const char *actions)
{
    char *array = NULL;
    char *a2 = strdup (actions);
    size_t capacity = 0;

    char *start = a2;
    s_string_append (&array, &capacity, "[");
    while (start) {
        char *end = strchr (start, '/');
        if (end) {
            *end = 0;
            char *action = vsjson_encode_string (start);
            s_string_append (&array, &capacity, action);
            s_string_append (&array, &capacity, ", ");
            zstr_free (&action);
            start = end;
            ++start;
        } else {
            char *action = vsjson_encode_string (start);
            s_string_append (&array, &capacity, action);
            zstr_free (&action);
            start = NULL;
        }
    }
    zstr_free (&a2);
    s_string_append (&array, &capacity, "]");
    return array;
}

char * rule_json (rule_t *self)
{
    if (!self) return NULL;
    
    char *json = NULL;
    size_t jsonsize = 0;
    {
        //json start + name
        char *jname = vsjson_encode_string (self->name);
        s_string_append (&json, &jsonsize, "{\n\"name\":");
        s_string_append (&json, &jsonsize, jname);
        s_string_append (&json, &jsonsize, ",\n");
        zstr_free (&jname);
    }
    {
        char *desc = vsjson_encode_string (self->description);
        s_string_append (&json, &jsonsize, "\"description\":");
        s_string_append (&json, &jsonsize, desc);
        s_string_append (&json, &jsonsize, ",\n");
        zstr_free (&desc);
    }
    {
        //metrics
        char *tmp = s_zlist_to_json_array (self->metrics); 
        s_string_append (&json, &jsonsize, "\"metrics\":");
        s_string_append (&json, &jsonsize, tmp);
        s_string_append (&json, &jsonsize, ",\n");
        zstr_free (&tmp); 
    }
    {
        //assets
        char *tmp = s_zlist_to_json_array (self->assets); 
        s_string_append (&json, &jsonsize, "\"assets\":");
        s_string_append (&json, &jsonsize, tmp);
        s_string_append (&json, &jsonsize, ",\n");
        zstr_free (&tmp); 
    }
    {
        //models
        char *tmp = s_zlist_to_json_array (self->models); 
        s_string_append (&json, &jsonsize, "\"models\":");
        s_string_append (&json, &jsonsize, tmp);
        s_string_append (&json, &jsonsize, ",\n");
        zstr_free (&tmp); 
    }
    {
        //groups
        char *tmp = s_zlist_to_json_array (self->groups); 
        s_string_append (&json, &jsonsize, "\"groups\":");
        s_string_append (&json, &jsonsize, tmp);
        s_string_append (&json, &jsonsize, ",\n");
        zstr_free (&tmp); 
    }
    {
        //results
        s_string_append (&json, &jsonsize, "\"results\": {\n");
        const void *result = zhash_first (self->result_actions);
        bool first = true;
        while (result) {
            if (first) {
                first = false;
            } else {
                s_string_append (&json, &jsonsize, ",\n");
            }
            char *key = vsjson_encode_string (zhash_cursor (self->result_actions));
            s_string_append (&json, &jsonsize, key);
            s_string_append (&json, &jsonsize, ": {\"actions\":");
            char *array = s_actions_to_json_array ( (const char *)result);
            s_string_append (&json, &jsonsize, array);
            s_string_append (&json, &jsonsize, "}");
            zstr_free (&array);
            zstr_free (&key);
            result = zhash_next (self->result_actions);
        }
        s_string_append (&json, &jsonsize, "},\n");
    }
    {
        //variables
        if (zhashx_size (self->variables)) {
            s_string_append (&json, &jsonsize, "\"variables\": {\n");
            char *item = (char *)zhashx_first (self->variables);
            bool first = true;
            while (item) {
                if (first) {
                    first = false;
                } else {
                    s_string_append (&json, &jsonsize, ",\n");
                }
                char *key = vsjson_encode_string((char *)zhashx_cursor (self->variables));
                char *value = vsjson_encode_string (item);
                s_string_append (&json, &jsonsize, key);
                s_string_append (&json, &jsonsize, ":");
                s_string_append (&json, &jsonsize, value);                
                zstr_free (&key);
                zstr_free (&value);
                item = (char *) zhashx_next (self->variables);
            }
            s_string_append (&json, &jsonsize, "},\n");
        }
    }
    {
        //json evaluation
        char *eval = vsjson_encode_string (self->evaluation);
        s_string_append (&json, &jsonsize, "\"evaluation\":");
        s_string_append (&json, &jsonsize, eval);
        s_string_append (&json, &jsonsize, "\n}\n");
        zstr_free (&eval);
    }
    return json;
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
        zlist_destroy (&self->models);
        zlist_destroy (&self->types);
        zhash_destroy (&self->result_actions);
        zhashx_destroy (&self->variables);
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
    printf (" * rule: \n");

    //  @selftest
    //  Simple create/destroy test
    {
        printf ("      Simple create/destroy test ... ");
        rule_t *self = rule_new ();
        assert (self);
        rule_destroy (&self);
        assert (self == NULL);
        printf ("      OK\n");
    }

    //  Load test #1
    {
        printf ("      Load test #1 ... ");
        rule_t *self = rule_new ();
        assert (self);
        rule_load (self, "./rules/load.rule"); 
        rule_destroy (&self);
        assert (self == NULL);
        printf ("      OK\n");
    }

    //  Load test #2 - tests 'variables' section
    {
        printf ("      Load test #2 - 'variables' section ... ");
        rule_t *self = rule_new ();
        assert (self);
        rule_load (self, "./rules/threshold.rule");
        //  prepare expected 'variables' hash 
        zhashx_t *expected = zhashx_new ();
        assert (expected);
        zhashx_set_duplicator (self->variables, (zhashx_duplicator_fn *) strdup);
        zhashx_set_destructor (self->variables, (zhashx_destructor_fn *) zstr_free);

        zhashx_insert (expected, "high_critical", (void *) "60");
        zhashx_insert (expected, "high_warning", (void *) "40");
        zhashx_insert (expected, "low_warning", (void *) "15");
        zhashx_insert (expected, "low_critical", (void *) "5");

        //  compare it against loaded 'variables'
        const char *item = (const char *) zhashx_first (self->variables);
        while (item) {
            const char *key = (const char *) zhashx_cursor (self->variables);
            const char *expected_value = (const char *) zhashx_lookup (expected, key);
            assert (expected_value);
            assert (streq (item, expected_value));
            zhashx_delete (expected, key);
            item = (const char *) zhashx_next (self->variables);
        }
        assert (zhashx_size (expected) == 0);
        zhashx_destroy (&expected);
        rule_destroy (&self);
        assert (self == NULL);
        printf ("      OK\n");
    }
    
    //  Load test #3 - json construction test 
    { 
        printf ("      Load test #3 - json construction test ... ");
        rule_t *self = rule_new ();
        assert (self);
        rule_load (self, "./rules/load.rule"); 
        // test rule to json
        char *json = rule_json (self);
        rule_t *rule = rule_new ();
        rule_parse (rule, json);
        assert (streq (rule_name(rule), rule_name (self)));
        rule_destroy (&rule);
        zstr_free (&json);
        rule_destroy (&self);
        printf ("      OK\n");
    }
    //  @end
    printf ("OK\n");
}
