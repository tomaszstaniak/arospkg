/* A picture in the window: an icon in a list row, an icon beside a name, a
 * screenshot. One small class of its own instead of Dtpic, for three
 * measured reasons: a list of two hundred rows would start two hundred
 * datatype objects; Dtpic kept the previous picture on screen when given a
 * new name (AROS One 1.3, 2026-10-12); and the picture datatype did not load
 * palette PNGs there. Here the file is decoded by libpkg (pngdec.h) and
 * drawn as it is, with its transparency, in a place of fixed size. */
#ifndef PM_PIC_H
#define PM_PIC_H

#include <exec/types.h>
#include <intuition/classusr.h>

/* 1 when the class could be made. Without it pic_new returns NULL and the
   window shows text only. */
int  pic_init(void);
void pic_exit(void);

/* A place of w x h pixels. `press`: it can be clicked (MUIA_Pressed).
   `in_list`: it is drawn inside a list row and leaves the row's own
   background alone. `flex`: the place may be given less room, down to a
   quarter of its size, and the picture is then scaled down to fit. */
Object *pic_new(int w, int h, int press, int in_list, int flex);

/* Show the PNG at `path`, or nothing when it is "" or cannot be shown.
   0 when a picture is now shown. `why` (may be NULL) says why not. */
int  pic_set(Object *o, const char *path, char *why, int whyn);

/* Show a small drawing made of rows of text: '#' is a dark pixel, anything
   else is clear. For the few glyphs the window draws itself. */
void pic_set_glyph(Object *o, const char *const *rows, int w, int h);

/* A count in a list row: `text` in the bright pen on a patch of the
   highlight colour with round ends, as high as the row's font. */
Object *pic_badge(const char *text);

/* A place of w x h pixels in a button's frame, for a drawing that is to
   look pressable where a real button cannot stand (a list row). */
Object *pic_new_button(int w, int h);

/* A word in a button's frame, for a list row: `label`, in a place wide
   enough for `widest`, so that a column of them has one width. */
Object *pic_label_button(const char *label, const char *widest);

/* A column's heading for a list's title row: `label` in bold in a place
   exactly `width` pixels wide. A list's column is as wide as its widest
   cell, so this is how a column is given a width. */
Object *pic_heading(const char *label, int width);
#endif
