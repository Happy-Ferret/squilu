//
// "$Id: Fl_Tile.H 11727 2016-05-09 11:29:59Z AlbrechtS $"
//
// Tile header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2016 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file. If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

#ifndef Fl_Tile_H
#define Fl_Tile_H

#include "Fl_Group.H"

/*
  The Fl_Tile class lets you resize its children by dragging
  the border between them.
*/

class FL_EXPORT Fl_Tile : public Fl_Group {
public:
  int handle(int event);
  Fl_Tile(int X, int Y, int W, int H, const char *L=0);
  void resize(int X, int Y, int W, int H);
  void position(int oldx, int oldy, int newx, int newy);

DECLARE_CLASS_CHEAP_RTTI_2(Fl_Tile, Fl_Group)
};

#endif

//
// End of "$Id: Fl_Tile.H 11727 2016-05-09 11:29:59Z AlbrechtS $".
//
