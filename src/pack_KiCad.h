#ifndef __PACK_KICAD_H__
#define __PACK_KICAD_H__
#include <stdio.h>

struct pack_pin;
typedef struct pack_pin pack_pin_t;
struct pack_graph;
typedef struct pack_graph pack_graph_t;

typedef struct{
  char *name;
  char *descr;
  pack_pin_t *pin;
  int pinn;
  pack_graph_t *graph;
  int graphn;
}pack_t;

pack_t* pack_load(char *filename);
void pack_free(pack_t *p);
void pack_test(pack_t *p);
void pack_graph_size(pack_t *p, float *x, float *y, float *w, float *h);
void pack_html_export(pack_t *p, FILE *pf);

#endif