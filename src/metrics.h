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

#ifndef METRICS_H_INCLUDED
#define METRICS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _metrics_t metrics_t;

//  @interface
//  Create a new metrics
FTY_ALERT_FLEXIBLE_PRIVATE metrics_t *
    metrics_new (void);

//  Destroy the metrics
FTY_ALERT_FLEXIBLE_PRIVATE void
    metrics_destroy (metrics_t **self_p);

//  Self test of this class
FTY_ALERT_FLEXIBLE_PRIVATE void
    metrics_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
