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
    mlm_client_t *mlm;
};


//  --------------------------------------------------------------------------
//  Create a new flexible_alert

flexible_alert_t *
flexible_alert_new (void)
{
    flexible_alert_t *self = (flexible_alert_t *) zmalloc (sizeof (flexible_alert_t));
    assert (self);
    //  Initialize class properties here
    self->rules = zhash_new ();
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
        mlm_client_destroy (&self->mlm);
        //  Free object itself
        free (self);
        *self_p = NULL;
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

                zstr_free (&cmd);
            }
            zmsg_destroy (&msg);
        }
        else if (which == mlm_client_msgpipe (self->mlm)) {
            zmsg_t *msg = mlm_client_recv (self->mlm);
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
    zstr_sendx (fs, "PRODUCER", FTY_PROTO_STREAM_ALERTS, NULL);
    zstr_sendx (fs, "CONSUMER", FTY_PROTO_STREAM_METRICS, ".*", NULL);

    // destroy actor
    zactor_destroy (&fs);
    //destroy malamute
    zactor_destroy (&malamute);
    //  @end
    printf ("OK\n");
}
