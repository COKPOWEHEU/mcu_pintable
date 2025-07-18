#ifndef __PACK_KICAD_H__
#define __PACK_KICAD_H__
#include <stdio.h>

typedef struct{
  char name[20];
  float x, y;
  float w, h;
}pack_pin_t;

typedef struct{
  char *name;
  char *descr;
  pack_pin_t *pin;
  int pinn;
  void *intr;
}pack_t;

pack_t* pack_load(char *filename);
void pack_free(pack_t *p);
void pack_test(pack_t *p);
void pack_html_export(pack_t *p, FILE *pf);

#endif
#if 0
typedef enum{
  pin_rect = 0,
  pin_oval,
  pin_unknown,
}pack_pinsh_t;

typedef struct{
  float x1,y1,x2,y2;
}pg_line_t;
typedef struct{
  float x1,y1, x2,y2, x3,y3;
}pg_arc_t;
typedef enum{
  pg_line,
  pg_arc,
}pg_type_t;

typedef struct{
  pg_type_t type;
  union{
    pg_line_t line;
    pg_arc_t arc;
  };
}pack_graph_t;

typedef struct{
  pack_pinsh_t *pinshape;
  pack_graph_t *graph;
  size_t graphn;
}intr_t;

#define pintr ((intr_t*)(p->internal))

#endif