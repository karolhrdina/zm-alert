/*  =========================================================================
    metrics - List of metrics

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
    metrics - List of metrics
@discuss
@end
*/

#include "fty_alert_flexible_classes.h"

//  Structure of our class

struct _metrics_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new metrics

metrics_t *
metrics_new (void)
{
    metrics_t *self = (metrics_t *) zmalloc (sizeof (metrics_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the metrics

void
metrics_destroy (metrics_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        metrics_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
metrics_test (bool verbose)
{
    printf (" * metrics: ");

    //  @selftest
    //  Simple create/destroy test
    metrics_t *self = metrics_new ();
    assert (self);
    metrics_destroy (&self);
    //  @end
    printf ("OK\n");
}
