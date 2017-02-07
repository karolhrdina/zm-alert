/*  =========================================================================
    flexible_alert - Main class for evaluating alerts

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
    flexible_alert - Main class for evaluating alerts
@discuss
@end
*/

#include "fty_alert_flexible_classes.h"

//  Structure of our class

struct _flexible_alert_t {
    zhash_t *rules;
    zhash_t *assets;
    zhash_t *metrics;
    mlm_client_t *mlm;
};

static void rule_freefn (void *rule)
{
    if (rule) {
        rule_t *self = (rule_t *) rule;
        rule_destroy (&self);
    }
}

static void asset_freefn (void *asset)
{
    if (asset) {
        zlist_t *self = (zlist_t *) asset;
        zlist_destroy (&self);
    }
}

void ftymsg_freefn (void *ptr)
{
    if (!ptr) return;
    fty_proto_t *fty = (fty_proto_t *)ptr;
    fty_proto_destroy (&fty);
}

//  --------------------------------------------------------------------------
//  Create a new flexible_alert

flexible_alert_t *
flexible_alert_new (void)
{
    flexible_alert_t *self = (flexible_alert_t *) zmalloc (sizeof (flexible_alert_t));
    assert (self);
    //  Initialize class properties here
    self->rules = zhash_new ();
    self->assets = zhash_new ();
    self->metrics = zhash_new ();
    self->mlm = mlm_client_new ();
    return self;
}

//  --------------------------------------------------------------------------
//  Destroy the flexible_alert

void
flexible_alert_destroy (flexible_alert_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        flexible_alert_t *self = *self_p;
        //  Free class properties here
        zhash_destroy (&self->rules);
        zhash_destroy (&self->assets);
        zhash_destroy (&self->metrics);
        mlm_client_destroy (&self->mlm);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Load all rules in directory. Rule MUST have ".rule" extension.

void
flexible_alert_load_rules (flexible_alert_t *self, const char *path)
{
    if (!self || !path) return;
    char fullpath [PATH_MAX];
    
    DIR *dir = opendir(path);
    if (!dir) {
        zsys_error ("cannot open rule dir '%s'", path);
        return;
    }
    struct dirent * entry;
    while ((entry = readdir(dir)) != NULL) {
        zsys_debug ("checking dir entry %s type %i", entry -> d_name, entry -> d_type);
        if (entry -> d_type == DT_LNK || entry -> d_type == DT_REG || entry -> d_type == 0) {
            // file or link
            int l = strlen (entry -> d_name);
            zsys_debug ("loading rule file: %s", entry -> d_name);
            if ( l > 5 && streq (&(entry -> d_name[l - 5]), ".rule")) {
                // json file
                rule_t *rule = rule_new();
                snprintf (fullpath, PATH_MAX, "%s/%s", path, entry -> d_name);
                if (rule_load (rule, fullpath) == 0) {
                    zsys_debug ("rule %s loaded", entry -> d_name);
                    zhash_update (self->rules, rule_name (rule), rule);
                    zhash_freefn (self->rules, rule_name (rule), rule_freefn);
                } else {
                    zsys_error ("failed to load rule '%s'", entry -> d_name);
                    rule_destroy (&rule);
                }
            }
        }
    }
    closedir(dir);
}

void
flexible_alert_send_alert (flexible_alert_t *self, const char *rulename, const char *asset, int result, const char *message, int ttl)
{
    char *severity = "OK";
    if (result == -1 || result == 1) severity = "WARNING";
    if (result == -2 || result == 2) severity = "CRITICAL";

    // topic
    char *topic;
    asprintf (&topic, "%s/%s@%s", rulename, severity, asset);

    // TTL
    char *ttls;
    asprintf (&ttls, "%i", ttl);
    zhash_t *aux = zhash_new();
    zhash_autofree (aux);
    zhash_insert (aux, "TTL", ttls);
    zstr_free (&ttls);
    
    // message
    zmsg_t *alert = fty_proto_encode_alert (
        aux,
        rulename,
        asset,
        result == 0 ? "RESOLVED" : "ACTIVE",
        severity,
        message,
        time(NULL),
        ""); // action list
    
    mlm_client_send (self -> mlm, topic, &alert);

    zhash_destroy (&aux);
    zstr_free (&topic);
    zmsg_destroy (&alert);
}


void
flexible_alert_evaluate (flexible_alert_t *self, rule_t *rule, const char *assetname)
{
    zlist_t *params = zlist_new ();
    zlist_autofree (params);
    
    // prepare lua function parameters
    zlist_t *rule_param_list = rule_metrics (rule);
    char *param = (char *) zlist_first (rule_param_list);
    int ttl = 0;
    while (param) {
        char *topic;
        asprintf (&topic, "%s@%s", param, assetname);
        fty_proto_t *ftymsg = (fty_proto_t *) zhash_lookup (self->metrics, topic);
        if (!ftymsg) {
            // some metrics are missing
            zlist_destroy (&params);
            zstr_free (&topic);
            zsys_debug ("missing metric %s", topic);
            return;
        }
        // TTL should be set accorning shortest ttl in metric
        if (ttl == 0 || ttl > fty_proto_ttl (ftymsg)) ttl = fty_proto_ttl (ftymsg);
        zstr_free (&topic);
        zlist_append (params, (char *) fty_proto_value (ftymsg));
        param = (char *) zlist_next (rule_param_list);
    }

    // call the lua function
    char *message;
    int result;

    rule_evaluate (rule, params, assetname, &result, &message);
    if (result != RULE_ERROR);
    flexible_alert_send_alert (
        self,
        rule_name (rule),
        assetname,
        result,
        message, ttl * 5 / 2
    );
    zstr_free (&message);
    zlist_destroy (&params);
}

//  --------------------------------------------------------------------------
//  drop expired metrics

void
flexible_alert_clean_metrics (flexible_alert_t *self)
{
    zlist_t *topics = zhash_keys (self->metrics);
    char *topic = (char *) zlist_first (topics);
    while (topic) {
        fty_proto_t *ftymsg = (fty_proto_t *) zhash_lookup (self->metrics, topic);
        if (fty_proto_time (ftymsg) + fty_proto_ttl (ftymsg) < time (NULL)) {
            zhash_delete (self->metrics, topic);
        }
        topic = (char *) zlist_next (topics);
    }
    zlist_destroy (&topics);
}

//  --------------------------------------------------------------------------
//  Function handles infoming metrics, drives lua evaluation

void
flexible_alert_handle_metric (flexible_alert_t *self, fty_proto_t **ftymsg_p)
{
    if (!self || !ftymsg_p || !*ftymsg_p) return;
    fty_proto_t *ftymsg = *ftymsg_p;
    if (fty_proto_id (ftymsg) != FTY_PROTO_METRIC) return;

    if (zhash_lookup (self->metrics, mlm_client_subject (self->mlm))) {
        flexible_alert_clean_metrics (self);
    }
    
    const char *assetname = fty_proto_element_src (ftymsg);
    const char *quantity = fty_proto_type (ftymsg);
    const char *description = fty_proto_aux_string (ftymsg, "description", "");

    // produce nagios style alerts
    if (strncmp (quantity, "nagios.", 7) == 0 && strlen (description)) {
        int ivalue = atoi (fty_proto_value (ftymsg));
        if (ivalue >=0 && ivalue <=2) {
            flexible_alert_send_alert (
                self,
                quantity,
                fty_proto_element_src (ftymsg),
                ivalue,
                description,
                fty_proto_ttl (ftymsg)
            );
            return;
        }
    }
    zlist_t *functions_for_asset = (zlist_t *) zhash_lookup (self->assets, assetname);
    if (! functions_for_asset) return;

    // this asset has some evaluation functions
    char *func = (char *) zlist_first (functions_for_asset);
    bool metric_saved =  false;
    while (func) {
        rule_t *rule = (rule_t *) zhash_lookup (self -> rules, func);
        if (zlist_exists (rule_metrics (rule), (char *)quantity)) {
            // we have to evaluate this function for our asset
            // save metric into cache
            if (! metric_saved) {
                fty_proto_set_time (ftymsg, time (NULL));
                char *topic;
                asprintf (&topic, "%s@%s", quantity, assetname);
                zhash_update (self->metrics, topic, ftymsg);
                zhash_freefn (self->metrics, topic, ftymsg_freefn);
                *ftymsg_p = NULL;
                zstr_free (&topic);
                metric_saved = true;
            }
            // evaluate
            flexible_alert_evaluate (self, rule, assetname);
        }
        func = (char *) zlist_next (functions_for_asset);
    }
}

//  --------------------------------------------------------------------------
//  Function returns true if function should be evaluated for particular asset.
//  This is decided by asset name (json "assets": []) or group (json "groups":[])

static int
is_rule_for_this_asset (rule_t *rule, fty_proto_t *ftymsg)
{
    if (!rule || !ftymsg) return 0;

    char *asset = (char *)fty_proto_name (ftymsg);
    if (zlist_exists (rule_assets(rule), asset)) return 1;

    zhash_t *ext = fty_proto_ext (ftymsg);
    zlist_t *keys = zhash_keys (ext);
    char *key = (char *)zlist_first (keys);
    while (key) {
        if (strncmp ("group.", key, 6) == 0) {
            // this is group
            char * grp = (char *)zhash_lookup (ext, key);
            if (zlist_exists (rule_groups (rule), grp)) {
                zlist_destroy (&keys);
                return 1;
            }
        }
        key = (char *)zlist_next (keys);
    }
    zlist_destroy (&keys);
    
    const char *model = fty_proto_ext_string (ftymsg, "model", NULL);
    if (model && zlist_exists (rule_models (rule), (void *) model)) return 1;
    model = fty_proto_ext_string (ftymsg, "device.part", NULL);
    if (model && zlist_exists (rule_models (rule), (void *) model)) return 1;
    
    return 0;
}

//  --------------------------------------------------------------------------
//  When asset message comes, function checks if we have rule for it and stores
//  list of rules valid for this asset.

void
flexible_alert_handle_asset (flexible_alert_t *self, fty_proto_t *ftymsg)
{
    if (!self || !ftymsg) return;
    if (fty_proto_id (ftymsg) != FTY_PROTO_ASSET) return;

    const char *operation = fty_proto_operation (ftymsg);
    const char *assetname = fty_proto_name (ftymsg);
    
    if (streq (operation, "delete")) {
        if (zhash_lookup (self->assets, assetname)) {
            zhash_delete (self->assets, assetname);
        }
        return;
    }
    if (streq (operation, "inventory") && streq (mlm_client_sender (self -> mlm), "asset-autoupdate")) {
        zlist_t *functions_for_asset = zlist_new ();
        zlist_autofree (functions_for_asset);
        
        rule_t *rule = (rule_t *)zhash_first (self->rules);
        while (rule) {
            if (is_rule_for_this_asset (rule, ftymsg)) {
                zlist_append (functions_for_asset, (char *)rule_name (rule));
                zsys_debug ("rule '%s' is valid for '%s'", rule_name (rule), assetname);
            }
            rule = (rule_t *)zhash_next (self->rules);
        }
        if (! zlist_size (functions_for_asset)) {
            zsys_debug ("no rule for %s", assetname);
            zhash_delete (self->assets, assetname);
            zlist_destroy (&functions_for_asset);
            return;
        }
        zhash_update (self->assets, assetname, functions_for_asset);
        zhash_freefn (self->assets, assetname, asset_freefn);
    }
}

//  --------------------------------------------------------------------------
//  Actor running one instance of flexible alert class

void
flexible_alert_actor (zsock_t *pipe, void *args)
{
    flexible_alert_t *self = flexible_alert_new ();
    assert (self);
    zsock_signal (pipe, 0);

    zpoller_t *poller = zpoller_new (mlm_client_msgpipe(self->mlm), pipe, NULL);
    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, -1);
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (msg);
            if (cmd) {
                if (streq (cmd, "$TERM")) {
                    zstr_free (&cmd);
                    zmsg_destroy (&msg);
                    break;
                }
                else if (streq (cmd, "BIND")) {
                    char *endpoint = zmsg_popstr (msg);
                    char *myname = zmsg_popstr (msg);
                    assert (endpoint && myname);
                    mlm_client_connect (self->mlm, endpoint, 5000, myname);
                    zstr_free (&endpoint);
                    zstr_free (&myname);
                }
                else if (streq (cmd, "PRODUCER")) {
                    char *stream = zmsg_popstr (msg);
                    assert (stream);
                    mlm_client_set_producer (self->mlm, stream);
                    zstr_free (&stream);
                }
                else if (streq (cmd, "CONSUMER")) {
                    char *stream = zmsg_popstr (msg);
                    char *pattern = zmsg_popstr (msg);
                    assert (stream && pattern);
                    mlm_client_set_consumer (self->mlm, stream, pattern);
                    zstr_free (&stream);
                    zstr_free (&pattern);
                }
                else if (streq (cmd, "LOADRULES")) {
                    char *dir = zmsg_popstr (msg);
                    assert (dir);
                    flexible_alert_load_rules (self, dir);
                    zstr_free (&dir);
                }


                zstr_free (&cmd);
            }
            zmsg_destroy (&msg);
        }
        else if (which == mlm_client_msgpipe (self->mlm)) {
            zmsg_t *msg = mlm_client_recv (self->mlm);
            if (is_fty_proto (msg)) {
                fty_proto_t *fmsg = fty_proto_decode (&msg);
                if (fty_proto_id (fmsg) == FTY_PROTO_ASSET) {
                    flexible_alert_handle_asset (self, fmsg);
                }
                if (fty_proto_id (fmsg) == FTY_PROTO_METRIC) {
                    flexible_alert_handle_metric (self, &fmsg);
                }
                fty_proto_destroy (&fmsg);
            }
            zmsg_destroy (&msg);
        }
    }
    
    zpoller_destroy (&poller);
    flexible_alert_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
flexible_alert_test (bool verbose)
{
    printf (" * flexible_alert: ");

    //  @selftest
    //  Simple create/destroy test
    flexible_alert_t *self = flexible_alert_new ();
    assert (self);
    flexible_alert_destroy (&self);

    // start malamute
    static const char *endpoint = "inproc://fty-metric-snmp";
    zactor_t *malamute = zactor_new (mlm_server, (void*) "Malamute");
    zstr_sendx (malamute, "BIND", endpoint, NULL);
    if (verbose) zstr_send (malamute, "VERBOSE");

    // create flexible alert actor
    zactor_t *fs = zactor_new (flexible_alert_actor, NULL);
    assert (fs);
    zstr_sendx (fs, "BIND", endpoint, "me", NULL);
    zstr_sendx (fs, "PRODUCER", FTY_PROTO_STREAM_ALERTS_SYS, NULL);
    zstr_sendx (fs, "CONSUMER", FTY_PROTO_STREAM_ASSETS, ".*", NULL);
    zstr_sendx (fs, "CONSUMER", FTY_PROTO_STREAM_METRICS, ".*", NULL);
    zstr_sendx (fs, "LOADRULES", "./rules", NULL);

    // create mlm client for interaction with actor
    mlm_client_t *asset = mlm_client_new ();
    mlm_client_connect (asset, endpoint, 5000, "asset-autoupdate");
    mlm_client_set_producer (asset, FTY_PROTO_STREAM_ASSETS);
    mlm_client_set_consumer (asset, FTY_PROTO_STREAM_ALERTS_SYS, ".*");

    // metric client
    mlm_client_t *metric = mlm_client_new ();
    mlm_client_connect (metric, endpoint, 5000, "metric");
    mlm_client_set_producer (metric, FTY_PROTO_STREAM_METRICS);
    
    // let malamute establish everything
    zclock_sleep (200);
    {
        zhash_t *ext = zhash_new();
        zhash_autofree (ext);
        zhash_insert (ext, "group.1", "all-upses");
        zmsg_t *assetmsg = fty_proto_encode_asset (
            NULL,
            "mydevice",
            "inventory",
            ext
        );
        mlm_client_send (asset, "myasset", &assetmsg);
        zhash_destroy (&ext);
        zmsg_destroy (&assetmsg);
    }
    zclock_sleep (200);
    {
        // send metric, receive alert
        zmsg_t *msg = fty_proto_encode_metric (
            NULL,
            "status.ups",
            "mydevice",
            "0",
            "",
            60);
        mlm_client_send (metric, "status.ups@mydevice", &msg);

        zmsg_t *alert = mlm_client_recv (asset);
        assert (is_fty_proto (alert));
        fty_proto_t *ftymsg = fty_proto_decode (&alert);
        fty_proto_print (ftymsg);
        fty_proto_destroy (&ftymsg);
        zmsg_destroy (&alert);
    }
    zclock_sleep (200);
    mlm_client_destroy (&metric);
    mlm_client_destroy (&asset);
    // destroy actor
    zactor_destroy (&fs);
    //destroy malamute
    zactor_destroy (&malamute);
    //  @end
    printf ("OK\n");
}
