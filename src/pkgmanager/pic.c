/* The picture class (pic.h). */
#include "pic.h"
#include "../libpkg/media.h"
#include "../libpkg/pngdec.h"
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/cybergraphics.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <stdlib.h>
#include <string.h>

#define PIC_TAG(n)      (TAG_USER | 0x7e510000 | (n))
#define MUIA_Pic_Width  PIC_TAG(1)    /* I.. the place's size */
#define MUIA_Pic_Height PIC_TAG(2)
#define MUIA_Pic_Flex   PIC_TAG(3)    /* I.. the place may shrink to a quarter of its size */
#define MUIA_Pic_Badge  PIC_TAG(4)    /* I.. a short text: the object is a badge, sized by its text */
#define MUIA_Pic_Label  PIC_TAG(5)    /* I.. a word shown as a button's label */
#define MUIA_Pic_Widest PIC_TAG(6)    /* I.. the widest word of its kind */
#define MUIA_Pic_Head   PIC_TAG(7)    /* I.. the label is a heading in a place MUIA_Pic_Width wide */

struct pic_data {
    unsigned char *px;      /* A,R,G,B per pixel, or NULL */
    int w, h;               /* of px */
    int boxw, boxh;
    int shown;              /* between MUIM_Show and MUIM_Hide: it has a place to draw in */
    int flex;               /* the place may be smaller than boxw x boxh; the picture is then scaled down */
    unsigned char *small;   /* px scaled to sw x sh for the place it was last drawn in, or NULL */
    int sw, sh;
    char badge[12];         /* not a picture: this text on a rounded patch of the highlight colour */
    char label[16];         /* not a picture: this word, as on a button */
    char widest[16];        /* ...in a place wide enough for this one, so a column of them lines up */
    int head;               /* the label is a column's heading: bold, at the left, in a place boxw wide */
};

static struct MUI_CustomClass *pic_class;

static IPTR pic_new_m(Class *cl, Object *obj, struct opSet *msg)
{
    struct pic_data *d;
    obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
    if (!obj) return 0;
    d = (struct pic_data *)INST_DATA(cl, obj);
    d->boxw = (int)GetTagData(MUIA_Pic_Width, 16, msg->ops_AttrList);
    d->boxh = (int)GetTagData(MUIA_Pic_Height, 16, msg->ops_AttrList);
    d->flex = (int)GetTagData(MUIA_Pic_Flex, 0, msg->ops_AttrList);
    d->head = (int)GetTagData(MUIA_Pic_Head, 0, msg->ops_AttrList);
    {
        const char *b = (const char *)GetTagData(MUIA_Pic_Badge, 0, msg->ops_AttrList);
        if (b) { strncpy(d->badge, b, sizeof d->badge - 1); d->badge[sizeof d->badge - 1] = 0; }
        b = (const char *)GetTagData(MUIA_Pic_Label, 0, msg->ops_AttrList);
        if (b) { strncpy(d->label, b, sizeof d->label - 1); d->label[sizeof d->label - 1] = 0; }
        b = (const char *)GetTagData(MUIA_Pic_Widest, 0, msg->ops_AttrList);
        if (b) { strncpy(d->widest, b, sizeof d->widest - 1); d->widest[sizeof d->widest - 1] = 0; }
    }
    return (IPTR)obj;
}

static IPTR pic_dispose_m(Class *cl, Object *obj, Msg msg)
{
    struct pic_data *d = (struct pic_data *)INST_DATA(cl, obj);
    free(d->px); free(d->small);
    d->px = d->small = NULL;
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR pic_minmax_m(Class *cl, Object *obj, struct MUIP_AskMinMax *msg)
{
    struct pic_data *d = (struct pic_data *)INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (d->badge[0] && _font(obj)) {
        /* As high as a line of the font it stands in, and round at both
           ends, so the text has half a height of room on each side. */
        struct RastPort rp;
        InitRastPort(&rp);
        SetFont(&rp, _font(obj));
        d->boxh = _font(obj)->tf_YSize;
        d->boxw = TextLength(&rp, (CONST_STRPTR)d->badge, strlen(d->badge)) + d->boxh;
    }
    if (d->label[0] && _font(obj)) {
        struct RastPort rp;
        const char *w = d->widest[0] ? d->widest : d->label;
        InitRastPort(&rp);
        SetFont(&rp, _font(obj));
        d->boxh = _font(obj)->tf_YSize;
        if (!d->head) d->boxw = TextLength(&rp, (CONST_STRPTR)w, strlen(w)) + 10;
    }
    msg->MinMaxInfo->MinWidth  += d->flex ? d->boxw / 4 : d->boxw; msg->MinMaxInfo->DefWidth  += d->boxw;
    msg->MinMaxInfo->MinHeight += d->flex ? d->boxh / 4 : d->boxh; msg->MinMaxInfo->DefHeight += d->boxh;
    /* A flexible place takes whatever room there is and centres the
       picture in it, so the layout around it never depends on a picture. */
    if (d->flex) { msg->MinMaxInfo->MaxWidth = MUI_MAXMAX; msg->MinMaxInfo->MaxHeight = MUI_MAXMAX; }
    else { msg->MinMaxInfo->MaxWidth += d->boxw; msg->MinMaxInfo->MaxHeight += d->boxh; }
    return 0;
}

/* px scaled down to tw x th, every pixel the average of the ones it covers:
   a screenshot keeps its thin lines and small text readable enough to tell
   what it shows. NULL when there is no memory for it. */
static unsigned char *pic_scale(const unsigned char *px, int w, int h, int tw, int th)
{
    unsigned char *out = (unsigned char *)malloc((size_t)tw * (size_t)th * 4);
    int x, y;
    if (!out) return NULL;
    for (y = 0; y < th; y++) {
        int y0 = y * h / th, y1 = (y + 1) * h / th;
        if (y1 <= y0) y1 = y0 + 1;
        for (x = 0; x < tw; x++) {
            int x0 = x * w / tw, x1 = (x + 1) * w / tw, xx, yy;
            unsigned long a = 0, r = 0, g = 0, b = 0, n;
            if (x1 <= x0) x1 = x0 + 1;
            for (yy = y0; yy < y1; yy++)
                for (xx = x0; xx < x1; xx++) {
                    const unsigned char *p = px + ((size_t)yy * (size_t)w + (size_t)xx) * 4;
                    a += p[0]; r += p[1]; g += p[2]; b += p[3];
                }
            n = (unsigned long)(y1 - y0) * (unsigned long)(x1 - x0);
            out[((size_t)y * (size_t)tw + (size_t)x) * 4 + 0] = (unsigned char)(a / n);
            out[((size_t)y * (size_t)tw + (size_t)x) * 4 + 1] = (unsigned char)(r / n);
            out[((size_t)y * (size_t)tw + (size_t)x) * 4 + 2] = (unsigned char)(g / n);
            out[((size_t)y * (size_t)tw + (size_t)x) * 4 + 3] = (unsigned char)(b / n);
        }
    }
    return out;
}

static IPTR pic_draw_m(Class *cl, Object *obj, struct MUIP_Draw *msg)
{
    struct pic_data *d = (struct pic_data *)INST_DATA(cl, obj);
    const unsigned char *px;
    int w, h;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if ((msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) && d->badge[0] && _font(obj)) {
        struct RastPort *rp = _rp(obj);
        int x0 = _mleft(obj), y0 = _mtop(obj), bw = _mwidth(obj), bh = _mheight(obj), r = bh / 2, y;
        SetAPen(rp, _pens(obj)[MPEN_FILL]);
        SetDrMd(rp, JAM1);
        for (y = 0; y < bh; y++) {
            /* How far the round end is from the edge on this row. */
            int dy = y < r ? r - y : (y >= bh - r ? y - (bh - 1 - r) : 0), in = 0;
            while (in < r && (r - in) * (r - in) + dy * dy > r * r + r) in++;
            if (bw - 2 * in > 0) RectFill(rp, x0 + in, y0 + y, x0 + bw - 1 - in, y0 + y);
        }
        SetFont(rp, _font(obj));
        SetAPen(rp, _pens(obj)[MPEN_SHINE]);
        Move(rp, x0 + (bw - TextLength(rp, (CONST_STRPTR)d->badge, strlen(d->badge))) / 2,
             y0 + (bh - _font(obj)->tf_YSize) / 2 + _font(obj)->tf_Baseline);
        Text(rp, (CONST_STRPTR)d->badge, strlen(d->badge));
        return 0;
    }
    if ((msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) && d->label[0] && _font(obj)) {
        struct RastPort *rp = _rp(obj);
        SetFont(rp, _font(obj));
        SetAPen(rp, _pens(obj)[MPEN_TEXT]);
        SetDrMd(rp, JAM1);
        if (d->head) SetSoftStyle(rp, FSF_BOLD, AskSoftStyle(rp));
        Move(rp, d->head ? _mleft(obj) : _mleft(obj) + (_mwidth(obj) - TextLength(rp, (CONST_STRPTR)d->label, strlen(d->label))) / 2,
             _mtop(obj) + (_mheight(obj) - _font(obj)->tf_YSize) / 2 + _font(obj)->tf_Baseline);
        Text(rp, (CONST_STRPTR)d->label, strlen(d->label));
        if (d->head) SetSoftStyle(rp, FS_NORMAL, AskSoftStyle(rp));
        return 0;
    }
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) || !d->px) return 0;
    px = d->px; w = d->w; h = d->h;
    if (w > _mwidth(obj) || h > _mheight(obj)) {
        /* The place is smaller than the picture (a small screen): the whole
           picture, scaled to fit, kept until the place changes size. */
        int tw = _mwidth(obj), th = (int)((long)h * tw / w);
        if (th > _mheight(obj)) { th = _mheight(obj); tw = (int)((long)w * th / h); }
        if (tw < 1 || th < 1) return 0;
        if (!d->small || d->sw != tw || d->sh != th) {
            free(d->small);
            d->small = pic_scale(d->px, d->w, d->h, tw, th);
            d->sw = tw; d->sh = th;
        }
        if (!d->small) return 0;
        px = d->small; w = tw; h = th;
    }
    WritePixelArrayAlpha((APTR)px, 0, 0, w * 4, _rp(obj), _mleft(obj) + (_mwidth(obj) - w) / 2,
                         _mtop(obj) + (_mheight(obj) - h) / 2, w, h, 0xffffffff);
    return 0;
}

BOOPSI_DISPATCHER(IPTR, pic_dispatcher, cl, obj, msg)
{
    switch (msg->MethodID) {
    case OM_NEW:         return pic_new_m(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:     return pic_dispose_m(cl, obj, msg);
    case MUIM_AskMinMax: return pic_minmax_m(cl, obj, (struct MUIP_AskMinMax *)msg);
    case MUIM_Draw:      return pic_draw_m(cl, obj, (struct MUIP_Draw *)msg);
    case MUIM_Show:      ((struct pic_data *)INST_DATA(cl, obj))->shown = 1; break;
    case MUIM_Hide:      ((struct pic_data *)INST_DATA(cl, obj))->shown = 0; break;
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

int pic_init(void)
{
    if (!pic_class)
        pic_class = MUI_CreateCustomClass(NULL, (ClassID)MUIC_Area, NULL, sizeof(struct pic_data), (APTR)pic_dispatcher);
    return pic_class != NULL;
}

void pic_exit(void)
{
    if (pic_class) MUI_DeleteCustomClass(pic_class);
    pic_class = NULL;
}

Object *pic_new(int w, int h, int press, int in_list, int flex)
{
    if (!pic_class) return NULL;
    return NewObject(pic_class->mcc_Class, NULL,
                     MUIA_Pic_Width, w, MUIA_Pic_Height, h, MUIA_Pic_Flex, flex,
                     MUIA_Frame, MUIV_Frame_None,
                     MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                     in_list ? MUIA_FillArea : TAG_IGNORE, FALSE,
                     press ? MUIA_InputMode : TAG_IGNORE, MUIV_InputMode_RelVerify,
                     TAG_DONE);
}

int pic_set(Object *o, const char *path, char *why, int whyn)
{
    struct pic_data *d;
    unsigned char *px = NULL;
    int w = 0, h = 0, rc = -1;
    if (!o || !pic_class) return -1;
    d = (struct pic_data *)INST_DATA(pic_class->mcc_Class, o);
    if (path && path[0])
        /* A place smaller than a screenshot still takes one: it is drawn
           scaled down. */
        rc = pkg_png_load(path, PKG_MEDIA_MAXBYTES, d->boxw > PKG_SHOT_W ? d->boxw : PKG_SHOT_W,
                          d->boxh > PKG_SHOT_H ? d->boxh : PKG_SHOT_H, &px, &w, &h, why, why ? (size_t)whyn : 0);
    free(d->px); free(d->small);
    d->small = NULL;
    d->px = px; d->w = w; d->h = h;
    if (d->shown) MUI_Redraw(o, MADF_DRAWOBJECT);
    return rc;
}

void pic_set_glyph(Object *o, const char *const *rows, int w, int h)
{
    struct pic_data *d;
    unsigned char *px;
    int x, y;
    if (!o || !pic_class || !(px = (unsigned char *)calloc((size_t)w * (size_t)h, 4))) return;
    d = (struct pic_data *)INST_DATA(pic_class->mcc_Class, o);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            if (rows[y][x] == '#') px[((size_t)y * (size_t)w + (size_t)x) * 4] = 255;   /* black, opaque */
    free(d->px); free(d->small);
    d->small = NULL;
    d->px = px; d->w = w; d->h = h;
    if (d->shown) MUI_Redraw(o, MADF_DRAWOBJECT);
}

Object *pic_badge(const char *text)
{
    if (!pic_class) return NULL;
    return NewObject(pic_class->mcc_Class, NULL,
                     MUIA_Pic_Badge, (IPTR)text,
                     MUIA_Frame, MUIV_Frame_None,
                     MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                     MUIA_FillArea, FALSE,
                     TAG_DONE);
}

Object *pic_new_button(int w, int h)
{
    if (!pic_class) return NULL;
    return NewObject(pic_class->mcc_Class, NULL,
                     MUIA_Pic_Width, w, MUIA_Pic_Height, h,
                     MUIA_Frame, MUIV_Frame_Button,
                     MUIA_Background, MUII_ButtonBack,
                     TAG_DONE);
}

Object *pic_label_button(const char *label, const char *widest)
{
    if (!pic_class) return NULL;
    return NewObject(pic_class->mcc_Class, NULL,
                     MUIA_Pic_Label, (IPTR)label, MUIA_Pic_Widest, (IPTR)widest,
                     MUIA_Frame, MUIV_Frame_Button,
                     MUIA_Background, MUII_ButtonBack,
                     TAG_DONE);
}

Object *pic_heading(const char *label, int width)
{
    if (!pic_class) return NULL;
    return NewObject(pic_class->mcc_Class, NULL,
                     MUIA_Pic_Label, (IPTR)label, MUIA_Pic_Width, width, MUIA_Pic_Head, 1,
                     MUIA_Frame, MUIV_Frame_None,
                     MUIA_InnerLeft, 0, MUIA_InnerRight, 0, MUIA_InnerTop, 0, MUIA_InnerBottom, 0,
                     MUIA_FillArea, FALSE,
                     TAG_DONE);
}
