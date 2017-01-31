/*  =========================================================================
    vsjson - JSON parser

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
    vsjson - JSON parser
@discuss
@end
*/

#include "fty_alert_flexible_classes.h"

//  Structure of our class

struct _vsjson_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new vsjson

vsjson_t *
vsjson_new (void)
{
    vsjson_t *self = (vsjson_t *) zmalloc (sizeof (vsjson_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the vsjson

void
vsjson_destroy (vsjson_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        vsjson_t *self = *self_p;
        //  Free class properties here
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
    printf (" * vsjson: ");

    //  @selftest
    //  Simple create/destroy test
    vsjson_t *self = vsjson_new ();
    assert (self);
    vsjson_destroy (&self);
    //  @end
    printf ("OK\n");
}
