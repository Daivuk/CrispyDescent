/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/main/rcs/automap.h $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:29:35 $
 *
 * Prototypes for auto-map stuff.
 *
 */

#ifndef _AUTOMAP_H
#define _AUTOMAP_H

#include "misc/types.h"
#include "segment.h"

extern void do_automap(int key_code);
extern void automap_clear_visited();
extern uint8_t Automap_visited[MAX_SEGMENTS];
extern void modex_print_message(int x, int y, char* str);

void adjust_segment_limit(int SegmentLimit); 
void draw_all_edges();
void automap_build_edge_list();


#endif
