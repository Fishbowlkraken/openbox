// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 2; -*-

#include "config.h"

#include "truerendercontrol.hh"
#include "display.hh"
#include "screeninfo.hh"
#include "surface.hh"
#include "rendertexture.hh"

extern "C" {
#include "../src/gettext.h"
#define _(str) gettext(str)
}

#include <cstdlib>

namespace otk {

TrueRenderControl::TrueRenderControl(int screen)
  : RenderControl(screen),
    _red_offset(0),
    _green_offset(0),
    _blue_offset(0)
{
  printf("Initializing TrueColor RenderControl\n");

  const ScreenInfo *info = display->screenInfo(_screen);
  XImage *timage = XCreateImage(**display, info->visual(), info->depth(),
                                ZPixmap, 0, NULL, 1, 1, 32, 0);

  unsigned long red_mask, green_mask, blue_mask;

  // find the offsets for each color in the visual's masks
  red_mask = timage->red_mask;
  green_mask = timage->green_mask;
  blue_mask = timage->blue_mask;

  while (! (red_mask & 1))   { _red_offset++;   red_mask   >>= 1; }
  while (! (green_mask & 1)) { _green_offset++; green_mask >>= 1; }
  while (! (blue_mask & 1))  { _blue_offset++;  blue_mask  >>= 1; }

  _red_shift = _green_shift = _blue_shift = 8;
  while (red_mask)   { red_mask   >>= 1; _red_shift--;   }
  while (green_mask) { green_mask >>= 1; _green_shift--; }
  while (blue_mask)  { blue_mask  >>= 1; _blue_shift--;  }
  XFree(timage);
}

TrueRenderControl::~TrueRenderControl()
{
  printf("Destroying TrueColor RenderControl\n");
}

void TrueRenderControl::reduceDepth(Surface &sf, XImage *im) const
{
  // since pixel32 is the largest possible pixel size, we can share the array
  int r, g, b;
  int x,y;
  pixel32 *data = sf.pixelData();
  pixel32 *ret = (pixel32*)malloc(im->width * im->height * 4);
  pixel16 *p = (pixel16*) ret;
  switch (im->bits_per_pixel) {
  case 32:
    if ((_red_offset != default_red_shift) ||
        (_blue_offset != default_blue_shift) ||
        (_green_offset != default_green_shift)) {
      printf("cross endian conversion\n");
      for (y = 0; y < im->height; y++) {
        for (x = 0; x < im->width; x++) {
          r = (data[x] >> default_red_shift) & 0xFF;
          g = (data[x] >> default_green_shift) & 0xFF;
          b = (data[x] >> default_blue_shift) & 0xFF;
          ret[x] = (r << _red_offset) + (g << _green_offset) +
            (b << _blue_offset);
        }
        data += im->width;
      } 
    } else {
      memcpy(ret, data, im->width * im->height * 4);
    }
    break;
  case 16:
    for (y = 0; y < im->height; y++) {
      for (x = 0; x < im->width; x++) {
        r = (data[x] >> default_red_shift) & 0xFF;
        r = r >> _red_shift;
        g = (data[x] >> default_green_shift) & 0xFF;
        g = g >> _green_shift;
        b = (data[x] >> default_blue_shift) & 0xFF;
        b = b >> _blue_shift;
        p[x] = (r << _red_offset) + (g << _green_offset) + (b << _blue_offset);
      }
      data += im->width;
      p += im->bytes_per_line/2;
    }
    break;
  default:
    printf("your bit depth is currently unhandled\n");
  }
  im->data = (char*)ret;
}

void TrueRenderControl::allocateColor(XColor *color) const
{
  const ScreenInfo *info = display->screenInfo(_screen);
  if (!XAllocColor(**display, info->colormap(), color)) {
    fprintf(stderr, "TrueRenderControl: color alloc error: rgb:%x/%x/%x\n",
            color->red & 0xff, color->green & 0xff, color->blue & 0xff);
    color->pixel = 0;
  }
}

}
