// ----------------------------------------------------------------------
//
//  Copyright (C) 2010 Fons Adriaensen <fons@linuxaudio.org>
//    
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// ----------------------------------------------------------------------


#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "styles.h"
#include "global.h"
#include "mainwin.h"


Mainwin::Mainwin (X_rootwin *parent, X_resman *xres, int xp, int yp, Jclient *jclient) :
    A_thread ("Main"),
    X_window (parent, xp, yp, XSIZE, YSIZE, XftColors [C_MAIN_BG]->pixel),
    _stop (false),
    _xres (xres),
    _jclient (jclient)
{
    X_hints     H;
    char        s [256];
    int         i, x;

    _atom = XInternAtom (dpy (), "WM_DELETE_WINDOW", True);
    XSetWMProtocols (dpy (), win (), &_atom, 1);
    _atom = XInternAtom (dpy (), "WM_PROTOCOLS", True);

    sprintf (s, "%s", jclient->jname ());
    x_set_title (s);
    H.position (xp, yp);
    H.minsize (XSIZE, YSIZE);
    H.maxsize (XSIZE, YSIZE);
    H.rname (xres->rname ());
    H.rclas (xres->rclas ());
    x_apply (&H); 

    _ambis = xres->getb (".ambisonic", false);
    RotaryCtl::init (disp ());
    x = 0;
    _rotary [R_DELAY] = new Rlinctl (this, this, &r_delay_img, x, 0, 160, 5,  0.02,  0.100,  0.04, R_DELAY);
    _rotary [R_XOVER] = new Rlogctl (this, this, &r_xover_img, x, 0, 200, 5,  50.0, 1000.0, 200.0, R_XOVER);
    _rotary [R_RTLOW] = new Rlogctl (this, this, &r_rtlow_img, x, 0, 200, 5,   1.0,    8.0,   3.0, R_RTLOW);
    _rotary [R_RTMID] = new Rlogctl (this, this, &r_rtmid_img, x, 0, 200, 5,   1.0,    8.0,   2.0, R_RTMID);
    _rotary [R_FDAMP] = new Rlogctl (this, this, &r_fdamp_img, x, 0, 200, 5, 1.5e3, 24.0e3, 6.0e3, R_FDAMP);
    x += 315;
    _rotary [R_EQ1FR] = new Rlogctl (this, this, &r_parfr_img, x, 0, 180, 5,  40.0,  2.5e3, 160.0, R_EQ1FR);
    _rotary [R_EQ1GN] = new Rlinctl (this, this, &r_pargn_img, x, 0, 150, 5, -15.0,   15.0,   0.0, R_EQ1GN);
    x += 110;
    _rotary [R_EQ2FR] = new Rlogctl (this, this, &r_parfr_img, x, 0, 180, 5, 160.0,   10e3, 2.5e3, R_EQ2FR);
    _rotary [R_EQ2GN] = new Rlinctl (this, this, &r_pargn_img, x, 0, 150, 5, -15.0,   15.0,   0.0, R_EQ2GN);
    x += 110;
    _rotary [R_OPMIX] = new Rlinctl (this, this, &r_opmix_img, x, 0, 180, 5,   0.0 ,   1.0,   0.5, R_OPMIX);
    _rotary [R_RGXYZ] = new Rlinctl (this, this, &r_rgxyz_img, x, 0, 180, 5,  -9.0 ,   9.0,   0.0, R_RGXYZ);
    for (i = 0; i < R_OPMIX; i++) _rotary [i]->x_map ();
    if (_ambis) _rotary [R_RGXYZ]->x_map ();
    else        _rotary [R_OPMIX]->x_map ();

    _jclient->reverb ()->set_delay (_rotary [R_DELAY]->value ());
    _jclient->reverb ()->set_xover (_rotary [R_XOVER]->value ());
    _jclient->reverb ()->set_rtlow (_rotary [R_RTLOW]->value ());
    _jclient->reverb ()->set_rtmid (_rotary [R_RTMID]->value ());
    _jclient->reverb ()->set_fdamp (_rotary [R_FDAMP]->value ());
    _jclient->reverb ()->set_opmix (_rotary [R_OPMIX]->value ());
    _jclient->reverb ()->set_rgxyz (_rotary [R_RGXYZ]->value ());
    _jclient->reverb ()->set_eq1 (_rotary [R_EQ1FR]->value (), _rotary [R_EQ1GN]->value ());
    _jclient->reverb ()->set_eq2 (_rotary [R_EQ2FR]->value (), _rotary [R_EQ2GN]->value ());

    x_add_events (ExposureMask); 
    x_map (); 
    set_time (0);
    inc_time (500000);
}

 
Mainwin::~Mainwin (void)
{
    RotaryCtl::fini ();
}

 
int Mainwin::process (void)
{
    int e;

    if (_stop) handle_stop ();

    e = get_event_timed ();
    switch (e)
    {
    case EV_TIME:
        handle_time ();
	break;
    }
    return e;
}


void Mainwin::handle_event (XEvent *E)
{
    switch (E->type)
    {
    case Expose:
	expose ((XExposeEvent *) E);
	break;  
 
    case ClientMessage:
        clmesg ((XClientMessageEvent *) E);
        break;
    }
}


void Mainwin::expose (XExposeEvent *E)
{
    if (E->count) return;
    redraw ();
}


void Mainwin::clmesg (XClientMessageEvent *E)
{
    if (E->message_type == _atom) _stop = true;
}


void Mainwin::handle_time (void)
{
    inc_time (500000);
//    XFlush (dpy ());
}


void Mainwin::handle_stop (void)
{
    put_event (EV_EXIT, 1);
}


void Mainwin::handle_callb (int type, X_window *W, XEvent *E)
{
    RotaryCtl  *R;
    int         k;

    switch (type)
    {
    case RotaryCtl::PRESS:
	R = (RotaryCtl *) W;
	k = R->cbind ();
	switch (k)
	{
	default:
	    ;
	}
	break;

    case RotaryCtl::DELTA:
	R = (RotaryCtl *) W;
	k = R->cbind ();
	switch (k)
	{
        case R_DELAY:
            _jclient->reverb ()->set_delay (_rotary [R_DELAY]->value ());
	    break;
	case R_XOVER:   
            _jclient->reverb ()->set_xover (_rotary [R_XOVER]->value ());
	    break;
	case R_RTLOW:   
            _jclient->reverb ()->set_rtlow (_rotary [R_RTLOW]->value ());
	    break;
	case R_RTMID:
            _jclient->reverb ()->set_rtmid (_rotary [R_RTMID]->value ());
            break;
	case R_FDAMP:     
            _jclient->reverb ()->set_fdamp (_rotary [R_FDAMP]->value ());
	    break;
	case R_OPMIX:     
            _jclient->reverb ()->set_opmix (_rotary [R_OPMIX]->value ());
	    break;
	case R_RGXYZ:     
            _jclient->reverb ()->set_rgxyz (_rotary [R_RGXYZ]->value ());
	    break;
	case R_EQ1FR:     
	case R_EQ1GN:     
            _jclient->reverb ()->set_eq1 (_rotary [R_EQ1FR]->value (), _rotary [R_EQ1GN]->value ());
	    break;
	case R_EQ2FR:     
	case R_EQ2GN:     
            _jclient->reverb ()->set_eq2 (_rotary [R_EQ2FR]->value (), _rotary [R_EQ2GN]->value ());
	    break;
	}
	break;
    }
}


void Mainwin::redraw (void)
{
    int x;

    x = 0;
    XPutImage (dpy (), win (), dgc (), revsect_img, 0, 0,   x, 0, 310, 75);
    x += 315;
    XPutImage (dpy (), win (), dgc (), eq1sect_img, 0, 0,   x, 0, 110, 75);
    x += 110;
    XPutImage (dpy (), win (), dgc (), eq2sect_img, 0, 0,   x, 0, 110, 75);
    x += 110;
    if (_ambis) XPutImage (dpy (), win (), dgc (), ambsect_img, 0, 0, x, 0, 70, 75);
    else        XPutImage (dpy (), win (), dgc (), mixsect_img, 0, 0, x, 0, 70, 75);
    x += 70;
    XPutImage (dpy (), win (), dgc (), redzita_img, 0, 0,   x, 0, 35, 75);
}


