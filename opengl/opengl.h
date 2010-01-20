
#ifndef OPENGL_H
#define OPENGL_H

#include "../ckvm.h"

typedef struct CKVGL *CKVGL;
CKVGL ckvgl_open(CKVM vm, int width, int height);
int ckvgl_width(CKVGL gl);
int ckvgl_height(CKVGL gl);
void ckvgl_draw(CKVGL gl);
void ckvgl_destroy(CKVGL gl);

#endif
