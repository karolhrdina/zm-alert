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

#ifndef RULE_H_INCLUDED
#define RULE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define RULE_ERROR 255

typedef struct _rule_t rule_t;

//  @interface
//  Create a new rule
FTY_ALERT_FLEXIBLE_PRIVATE rule_t *
    rule_new (void);

//  Destroy the rule
FTY_ALERT_FLEXIBLE_PRIVATE void
    rule_destroy (rule_t **self_p);

//  Self test of this class
FTY_ALERT_FLEXIBLE_PRIVATE void
    rule_test (bool verbose);

//  Self test of this class
FTY_ALERT_FLEXIBLE_PRIVATE void
    vsjson_test (bool verbose);

//  Parse json rule from string
FTY_ALERT_FLEXIBLE_PRIVATE int
    rule_parse (rule_t *self, const char *json);

// get rule name
FTY_ALERT_FLEXIBLE_PRIVATE const char *
    rule_name (rule_t *self);

// get rule asset list
FTY_ALERT_FLEXIBLE_PRIVATE zlist_t *
    rule_assets (rule_t *self);

// get rule group list
FTY_ALERT_FLEXIBLE_PRIVATE zlist_t *
    rule_groups (rule_t *self);

// get rule group list
FTY_ALERT_FLEXIBLE_PRIVATE zlist_t *
    rule_metrics (rule_t *self);

//  Load json rule from file
FTY_ALERT_FLEXIBLE_PRIVATE int
    rule_load (rule_t *self, const char *path);

// evaluate rule
FTY_ALERT_FLEXIBLE_PRIVATE void
    rule_evaluate (rule_t *self, zlist_t *params, const char *name, int *result, char **message);

//  @end

#ifdef __cplusplus
}
#endif

#endif
