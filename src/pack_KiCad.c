#include "pack_KiCad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

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
  pg_rect,
  pg_circ,
  pg_unknown,
}pg_type_t;

typedef struct{
  pg_type_t type;
  union{
    pg_line_t line;
    pg_arc_t arc;
    pg_line_t rect;
    pg_line_t circ;
  };
}pack_graph_t;

typedef struct{
  pack_pinsh_t *pinshape;
  pack_graph_t *graph;
  size_t graphn;
}intr_t;
#define pintr ((intr_t*)(p->intr))

char pack_show_unknown = 0;

void skip_chank(FILE *pf, int nesting){
  int ch;
  int cnt = nesting;
  do{
    ch = fgetc(pf);
    if(ch == '(')cnt++;
    if(ch == ')')cnt--;
    if(ch == EOF)break;
  }while(cnt > 0);
}

int next_chank(FILE *pf, char *name, int nesting){
  int ch;
  int cnt = nesting;
  do{
    ch = fgetc(pf);
    if(ch == '('){
      cnt++;
      fscanf(pf, "%s", name);
      return cnt;
    }
    if(ch == ')')cnt--;
    if(ch == EOF)break;
  }while(cnt > 0);
  return -1;
}

void find_chank(FILE *pf, char *name){
  static size_t pos = 0;
  if(name != NULL){
    pos = ftell(pf);
  }else{
    fseek(pf, pos, SEEK_SET);
    return;
  }
  int ch;
  int cnt = 1;
  char buf[1000];
  do{
    ch = fgetc(pf);
    if(ch == '('){
      cnt++;
      fscanf(pf, "%s", buf);
      if(strcmp(buf, name)==0){
        return;
      }else{
        skip_chank(pf, 1);
      }
    }
    if(ch == ')')cnt--;
    if(ch == EOF)break;
  }while(cnt > 0);
}

#define setminmax(min, max, x) do{if(x<min)min=x; if(x>max)max=x;}while(0)
void pack_graph_size(pack_t *p, float *x, float *y, float *w, float *h){
  float xmin=1e20, xmax=-1e20, ymin=1e20, ymax=-1e20;
  if(p == NULL)return;
  for(int i=0; i<pintr->graphn; i++){
    switch(pintr->graph[i].type){
      case pg_line:
        setminmax(xmin, xmax, pintr->graph[i].line.x1);
        setminmax(xmin, xmax, pintr->graph[i].line.x2);
        setminmax(ymin, ymax, pintr->graph[i].line.y1);
        setminmax(ymin, ymax, pintr->graph[i].line.y2);
        break;
      case pg_arc:
        setminmax(xmin, xmax, pintr->graph[i].arc.x1);
        setminmax(xmin, xmax, pintr->graph[i].arc.x2);
        setminmax(xmin, xmax, pintr->graph[i].arc.x3);
        setminmax(ymin, ymax, pintr->graph[i].arc.y1);
        setminmax(ymin, ymax, pintr->graph[i].arc.y2);
        setminmax(ymin, ymax, pintr->graph[i].arc.y3);
        break;
      case pg_rect:
        setminmax(xmin, xmax, pintr->graph[i].rect.x1);
        setminmax(xmin, xmax, pintr->graph[i].rect.x2);
        setminmax(ymin, ymax, pintr->graph[i].rect.y1);
        setminmax(ymin, ymax, pintr->graph[i].rect.y2);
        break;
      case pg_circ:
        setminmax(xmin, xmax, pintr->graph[i].circ.x1);
        setminmax(xmin, xmax, pintr->graph[i].circ.x2);
        setminmax(ymin, ymax, pintr->graph[i].circ.y1);
        setminmax(ymin, ymax, pintr->graph[i].circ.y2);
        break;
    }
  }
  for(int i=0; i<p->pinn; i++){
    float v = p->pin[i].x - p->pin[i].w/2;
    if(xmin > v)xmin = v;
    v = p->pin[i].x + p->pin[i].w/2;
    if(xmax < v)xmax = v;
    v = p->pin[i].y - p->pin[i].h/2;
    if(ymin > v)ymin = v;
    v = p->pin[i].y + p->pin[i].h/2;
    if(ymax < v)ymax = v;
  }
  *x = (xmin + xmax)/2;
  *y = (ymin + ymax)/2;
  *w = (xmax - xmin);
  *h = (ymax - ymin);
}

void pack_normalize(pack_t *p){
  float x, y, w, h;
  pack_graph_size(p, &x, &y, &w, &h);
  x -= w/2;
  y -= h/2;
  if(w < h)w = h;
  for(int i=0; i<p->pinn; i++){
    p->pin[i].x = (p->pin[i].x - x) / w;
    p->pin[i].y = (p->pin[i].y - y) / w;
    p->pin[i].w /= w;
    p->pin[i].h /= w;
  }
  for(int i=0; i<pintr->graphn; i++){
    if(pintr->graph[i].type == pg_line){
      pintr->graph[i].line.x1 = (pintr->graph[i].line.x1 - x)/w;
      pintr->graph[i].line.x2 = (pintr->graph[i].line.x2 - x)/w;
      pintr->graph[i].line.y1 = (pintr->graph[i].line.y1 - y)/w;
      pintr->graph[i].line.y2 = (pintr->graph[i].line.y2 - y)/w;
    }else if(pintr->graph[i].type == pg_arc){
      pintr->graph[i].arc.x1 = (pintr->graph[i].arc.x1 - x)/w;
      pintr->graph[i].arc.x2 = (pintr->graph[i].arc.x2 - x)/w;
      pintr->graph[i].arc.x3 = (pintr->graph[i].arc.x3 - x)/w;
      pintr->graph[i].arc.y1 = (pintr->graph[i].arc.y1 - y)/w;
      pintr->graph[i].arc.y2 = (pintr->graph[i].arc.y2 - y)/w;
      pintr->graph[i].arc.y3 = (pintr->graph[i].arc.y3 - y)/w;
    }
  }
}

void trim_quotes(char *str){
  char *ch = str;
  for(; isspace(ch[0]); ch++){}
  if((ch[0] == '"')||(ch[0] == '\'')){
    char *en = strrchr(ch+1, ch[0]);
    if(en != NULL){
      en[0] = 0;
      memmove(str, ch+1, (en-ch));
    }
  }
}

char* pack_search_name(char *filename){
  static char name[4096] = "";
  FILE *pf = fopen(filename, "r");
  if(pf == NULL){fprintf(stderr, "pack_KiCad.pack_load: file [%s] not found\n", filename); return NULL;}
  int res = fscanf(pf, " (%s", name);
  if(res <= 0)return name;
  if((strcmp(name, "footprint")!=0)&&(strcmp(name, "module")!=0)){
    fprintf(stderr, "pack_KiCad.pack_load: wrong file format [%s]\n", filename);
    name[0] = 0;
    fclose(pf);
    return name;
  }
  fscanf(pf, " %s", name);
  fclose(pf);
  trim_quotes(name);
  return name;
}

pack_t* pack_load(char *filename){
  FILE *pf = fopen(filename, "r");
  if(pf == NULL){fprintf(stderr, "pack_KiCad.pack_load: file [%s] not found\n", filename); return NULL;}
  
  char name[1000];
  fscanf(pf, " (%s", name);
  if((strcmp(name, "footprint")!=0)&&(strcmp(name, "module")!=0)){
    fprintf(stderr, "pack_KiCad.pack_load: wrong file format [%s]\n", filename);
    fclose(pf);
    return NULL;
  }
  fscanf(pf, " %s", name);
  trim_quotes(name);
  
  pack_t *p = malloc(sizeof(pack_t));
  if(p == NULL){fprintf(stderr, "pack_KiCad.pack_load: not enougn memory\n"); fclose(pf); return NULL;}
  p->descr = NULL;
  p->pin = NULL;
  p->pinn = 0;
  p->intr = NULL;
  p->name = strdup(name);
  
  int nst = 1;
  size_t pos = ftell(pf);
  size_t graphn = 0;
  do{
    nst = next_chank(pf, name, nst);
    if(nst <= 0)break;
    if((strcmp(name, "fp_line")==0)||(strcmp(name, "fp_arc")==0)||(strcmp(name, "fp_rect")==0)||(strcmp(name, "fp_circle")==0)){
      graphn++;
    }else if(strcmp(name, "pad")==0){
      p->pinn++;
    }else if(pack_show_unknown){
      if((strcmp(name, "version")==0)||(strcmp(name, "generator")==0)||(strcmp(name, "layer")==0)||(strcmp(name, "tedit")==0)||(strcmp(name, "descr")==0)||(strcmp(name, "tags")==0)||(strcmp(name, "attr")==0)||(strcmp(name, "fp_text")==0)||(strcmp(name, "model")==0)||(strcmp(name, "zone")==0)||(strncmp(name, "solder_",sizeof("solder_")-1)==0)||(strcmp(name, "fp_poly")==0)){
        //ignore
      }else{
        printf("[%s] unknown chank [%s]\n", filename, name);
      }
    }
    skip_chank(pf, 1);
    nst--;
  }while(nst > 0);
  
  fseek(pf, pos, SEEK_SET);
  
  p->pin = malloc(sizeof(pack_pin_t) * p->pinn);
  p->intr = malloc(sizeof(intr_t));
  if(p->intr == NULL){
    pack_free(p);
    fclose(pf);
    fprintf(stderr, "pack_KiCad.pack_load: not enough memory\n");
    return NULL;
  }
  pintr->graphn = graphn;
  pintr->graph = malloc(sizeof(pack_graph_t) * graphn);
  pintr->pinshape = malloc(sizeof(pack_pinsh_t) * p->pinn);
  
  if((pintr->pinshape==NULL)||(pintr->graph==NULL)||(p->pin==NULL)){
    pack_free(p);
    fclose(pf);
    fprintf(stderr, "pack_KiCad.pack_load: not enough memory\n");
    return NULL;
  }
  
  for(int i=0; i<p->pinn; i++){
    p->pin[i].name[0] = 0;
    p->pin[i].vis = 1;
    pintr->pinshape[i] = pin_unknown;
  }
  
  int gr = 0, pin=0;
  nst = 1;
  do{
    nst = next_chank(pf, name, nst);
    if(nst <= 0)break;
    if(strcmp(name, "fp_line")==0){
      find_chank(pf, "start");
      fscanf(pf, "%f%f", &(pintr->graph[gr].line.x1), &(pintr->graph[gr].line.y1));
      find_chank(pf, NULL); find_chank(pf, "end");
      fscanf(pf, "%f%f", &(pintr->graph[gr].line.x2), &(pintr->graph[gr].line.y2));
      find_chank(pf, NULL);
      pintr->graph[gr].type = pg_line; gr++;
    }else if(strcmp(name, "fp_arc")==0){
      find_chank(pf, "start");
      fscanf(pf, "%f%f", &(pintr->graph[gr].arc.x1), &(pintr->graph[gr].arc.y1));
      find_chank(pf, NULL); find_chank(pf, "mid");
      fscanf(pf, "%f%f", &(pintr->graph[gr].arc.x2), &(pintr->graph[gr].arc.y2));
      find_chank(pf, NULL); find_chank(pf, "end");
      fscanf(pf, "%f%f", &(pintr->graph[gr].arc.x3), &(pintr->graph[gr].arc.y3));
      find_chank(pf, NULL);
      pintr->graph[gr].type = pg_arc; gr++;
    }else if(strcmp(name, "fp_rect")==0){
      find_chank(pf, "start");
      fscanf(pf, "%f%f", &(pintr->graph[gr].rect.x1), &(pintr->graph[gr].rect.y1));
      find_chank(pf, NULL); find_chank(pf, "end");
      fscanf(pf, "%f%f", &(pintr->graph[gr].rect.x2), &(pintr->graph[gr].rect.y2));
      find_chank(pf, NULL);
      pintr->graph[gr].type = pg_rect; gr++;
    }else if(strcmp(name, "fp_circle")==0){
      find_chank(pf, "start");
      fscanf(pf, "%f%f", &(pintr->graph[gr].circ.x1), &(pintr->graph[gr].circ.y1));
      find_chank(pf, NULL); find_chank(pf, "end");
      fscanf(pf, "%f%f", &(pintr->graph[gr].circ.x2), &(pintr->graph[gr].circ.y2));
      find_chank(pf, NULL);
      pintr->graph[gr].type = pg_circ; gr++;
    }else if(strcmp(name, "pad")==0){
      fscanf(pf, " %s", p->pin[pin].name);
      trim_quotes(p->pin[pin].name);
      fscanf(pf, "%*s"); //smd / thru_hole / ...
      fscanf(pf, "%s", name);
      if(strcmp(name, "roundrect")==0){
        pintr->pinshape[pin] = pin_rect;
      }else if(strcmp(name, "rect")==0){
        pintr->pinshape[pin] = pin_rect;
      }else if(strcmp(name, "oval")==0){
        pintr->pinshape[pin] = pin_oval;
      }else if(strcmp(name, "circle")==0){
        pintr->pinshape[pin] = pin_oval;
      }else{
        pintr->pinshape[pin] = pin_unknown;
      }
      find_chank(pf, "at");
      float alp = 0;
      if(fscanf(pf, "%f%f%f", &(p->pin[pin].x), &(p->pin[pin].y), &alp) == 2)alp = 0;
      find_chank(pf, NULL); find_chank(pf, "size");
      fscanf(pf, "%f%f", &(p->pin[pin].w), &(p->pin[pin].h));
      if( fabs(alp-90) < 10){
        alp = p->pin[pin].w;
        p->pin[pin].w = p->pin[pin].h;
        p->pin[pin].h = alp;
      }
      //p->pin[pin].w *= 1.5; p->pin[pin].h *= 1.5;
      find_chank(pf, NULL);
      pin++;
    }else{
      //ignore
    }
    skip_chank(pf, 1);
    nst--;
  }while(nst > 0);
  pack_normalize(p);
  
  fclose(pf);
  
  return p;
}

char pack_equal(pack_t *p1, pack_t *p2){
  if((p1==NULL)||(p2==NULL))return 0;
  if(p1->pinn != p2->pinn)return 0;
  if(strcmp(p1->name, p2->name)!=0)return 0;
  if( (p1->descr != NULL) && (p2->descr != NULL) ){
    if(strcmp(p1->descr, p2->descr)!=0)return 0;
  }else if( (p1->descr != NULL) || (p2->descr != NULL) ){
    return 0;
  }
  if(((intr_t*)(p1->intr))->graphn != ((intr_t*)(p2->intr))->graphn)return 0;
  for(int i=0; i<p1->pinn; i++){
    if(strcmp(p1->pin[i].name, p2->pin[i].name)!=0)return 0;
  }
  return 1;
}

pack_t* pack_dummy(char *name, int npins){
  pack_t *p = malloc(sizeof(pack_t));
  if(p == NULL){fprintf(stderr, "pack_KiCad.pack_load: not enougn memory\n"); return NULL;}
  p->name = strdup(name);
  p->descr = strdup("Dummy");
  
  p->intr = malloc(sizeof(intr_t));
  pintr->graph = NULL;
  pintr->graphn = 0;
  
  p->pinn = npins;
  p->pin = malloc(sizeof(pack_pin_t)*npins);
  pintr->pinshape = malloc(sizeof(pack_pinsh_t)*npins);
  for(int i=0; i<npins; i++){
    p->pin[i].name[0] = 0;
    p->pin[i].x = p->pin[i].y = 1;
    p->pin[i].w = p->pin[i].h = 1;
    pintr->pinshape[i] = pin_unknown;
  }
  return p;
}

pack_t* pack_dup(pack_t *src){
  pack_t *p = malloc(sizeof(pack_t));
  if(p == NULL){fprintf(stderr, "pack_KiCad.pack_load: not enougn memory\n"); return NULL;}
  p->name = strdup(src->name);
  if(src->descr)p->descr = strdup(src->descr); else p->descr = NULL;
  p->pinn = src->pinn;
  p->pin = malloc(sizeof(pack_pin_t) * p->pinn);
  
  p->intr = malloc(sizeof(intr_t));
  pintr->graphn = ((intr_t*)(src->intr))->graphn;
  pintr->graph = malloc(sizeof(pack_graph_t) * pintr->graphn);
  pintr->pinshape = malloc(sizeof(pack_pinsh_t) * p->pinn);
  
  if((pintr->pinshape==NULL)||(pintr->graph==NULL)||(p->pin==NULL)){
    pack_free(p);
    fprintf(stderr, "pack_KiCad.pack_load: not enough memory\n");
    return NULL;
  }
  memcpy(p->pin, src->pin, sizeof(pack_pin_t) * p->pinn);
  memcpy(pintr->graph, ((intr_t*)(src->intr))->graph, sizeof(pack_graph_t) * pintr->graphn);
  memcpy(pintr->pinshape, ((intr_t*)(src->intr))->pinshape, sizeof(pack_pinsh_t) * p->pinn);
  
  return p;
}

void pack_free(pack_t *p){
  if(p == NULL)return;
  if(p->name){free(p->name); p->name = NULL;}
  if(p->descr){free(p->descr); p->descr = NULL;}
  if(p->pin){free(p->pin); p->pin = NULL;}
  if(pintr->graph){free(pintr->graph); pintr->graph = NULL;}
  if(pintr->pinshape){free(pintr->pinshape); pintr->pinshape = NULL;}
  if(p->intr){free(p->intr); p->intr = NULL;}
}

void pack_test(pack_t *p){
  float x, y, w, h;
  const char *pinsh_name[] = {"rect", "oval", "???"};
  if(p == NULL)return;
  int pinn = 0;
  for(int i=0; i<p->pinn; i++)if(p->pin[i].name[0] != 0)pinn++;
  printf("name = [%s]%i (%i)\t%i\n", p->name, p->pinn, pinn, pintr->graphn);
  pack_graph_size(p, &x, &y, &w, &h);
  printf("[%f ; %f]\t(%f x %f)\n", x, y, w, h);
  for(int i=0; i<p->pinn; i++){
    printf("%i (%s): %s (%f,%f) (%f,%f)\n", i, p->pin[i].name, pinsh_name[pintr->pinshape[i]], p->pin[i].x, p->pin[i].y, p->pin[i].w, p->pin[i].h);
  }
  for(int i=0; i<pintr->graphn; i++){
    switch(pintr->graph[i].type){
      case pg_line: printf("%i: line(%f,%f...%f,%f)\n", i, pintr->graph[i].line.x1, pintr->graph[i].line.y1, pintr->graph[i].line.x2, pintr->graph[i].line.y2); break;
      case pg_arc: printf("%i: arc(%f,%f %f,%f, %f,%f)\n", i, pintr->graph[i].arc.x1, pintr->graph[i].arc.y1, pintr->graph[i].arc.x2, pintr->graph[i].arc.y2, pintr->graph[i].arc.x3, pintr->graph[i].arc.y3); break;
      case pg_rect: printf("%i: rect(%f,%f...%f,%f)\n", i, pintr->graph[i].rect.x1, pintr->graph[i].rect.y1, pintr->graph[i].rect.x2, pintr->graph[i].rect.y2); break;
      case pg_circ: printf("%i: rect(%f,%f...%f,%f)\n", i, pintr->graph[i].circ.x1, pintr->graph[i].circ.y1, pintr->graph[i].circ.x2, pintr->graph[i].circ.y2); break;
    }
  }
}

void pack_html_common(FILE *pf){
  fprintf(pf, "function pack_html_draw(ctx, tbl, x, y, scale, font_sz, pins, pin_names, drawnames, graph){\n"
              "  const patternCanvas = document.createElement(\"canvas\");\n"
              "  const patternContext = patternCanvas.getContext(\"2d\");\n"
              "  patternCanvas.width = 10;\n"
              "  patternCanvas.height = 10;\n"
              "  patternContext.fillStyle =\"rgb(200 200 200)\";\n"
              "  patternContext.fillRect(0, 0, patternCanvas.width, patternCanvas.height);\n"
              "  patternContext.fillStyle =\"rgb(0 0 0)\";\n"
              "  patternContext.fillRect(0, 0, 5, 5);\n"
              "  const pattern = ctx.createPattern(patternCanvas, \"repeat\");\n"
              "  ctx.fillStyle =\"rgb(200 200 200)\";\n"
              "\n"
              "//draw pads\n"
              "  for(let i=0; i<pins.length; i++){\n"
              "    let col = \"rgb(220 220 220)\";\n"
              "    let idx = pin_names[i][4];\n"
              "    if((idx >= 0)&&(idx < tbl.length)){\n"
              "      if((tbl[idx].bgColor)&&(tbl[idx].bgcolor != \"\")){\n"
              "        col = tbl[idx].bgColor;\n"
              "      }\n"
              "    }\n"
              "    ctx.fillStyle = col;\n"
              "    if(table_selected == i){\n"
              "      ctx.fillStyle = pattern;\n"
              "    }\n"
              "    if(pins[i][0] == 'r'){\n"
              "      ctx.fillRect(x+pins[i][1]*scale, y+pins[i][2]*scale, pins[i][3]*scale, pins[i][4]*scale);\n"
              "    }else{\n"
              "      ctx.beginPath();\n"
              "      ctx.ellipse(x+pins[i][1]*scale, y+pins[i][2]*scale, pins[i][3]*scale, pins[i][4]*scale, 0, 0, Math.PI*2);\n"
              "      ctx.fill();\n"
              "    }\n"
              "    ctx.fillStyle = \"rgb(200 200 200)\";\n"
              "  }\n"
              "\n"
              "//draw pads text\n"
              "  ctx.fillStyle =\"rgb(0 0 0)\";\n"
              "  ctx.textBaseline = \"middle\";\n"
              "  ctx.textAlign = \"center\";\n"
              "  ctx.font = scale*font_sz + \"px serif\";\n"
              "\n"
              "  let colnum = -1;\n"
              "  let hdr = tbl[0].parentElement.parentElement.children[0].children[0].children;\n"
              "  for(let i=0; i<hdr.length; i++){\n"
              "    if(hdr[i].className == \"tbl_pinname\"){colnum = i; break;}\n"
              "  }\n"
              "  for(let i=0; i<pin_names.length; i++){\n"
              "    let name = pin_names[i][3];\n"
              "    let idx = pin_names[i][4];\n"
              "    if(drawnames && (idx>=0)&&(idx<tbl.length)){name = tbl[idx].children[1].children[0].textContent;}\n"
              "    if((colnum >= 0) && (idx >= 0) && (idx < tbl.length)){\n"
              "      let val = tbl[idx].children[colnum].children[0].value;\n"
              "      if(val != \"\")name = val;\n"
              "    }\n"
              "    if(pin_names[i][0] == 'h'){\n"
              "      ctx.strokeText(name, x + pin_names[i][1]*scale, y + pin_names[i][2]*scale);\n"
              "    }else{\n"
              "      ctx.save(); ctx.translate(x + pin_names[i][1]*scale, y + pin_names[i][2]*scale);\n"
              "      ctx.rotate(-Math.PI/2); ctx.strokeText(name, 0, 0);\n"
              "      ctx.restore();\n"
              "    }\n"
              "  }\n"
              "\n"
              "//draw graphics\n"
              "  ctx.beginPath();\n"
              "  for(let i=0; i<graph.length; i++){\n"
              "    if(graph[i][0] == 'l'){\n"
              "      ctx.moveTo(x + graph[i][1]*scale, y + graph[i][2]*scale); ctx.lineTo(x + graph[i][3]*scale, y + graph[i][4]*scale);\n"
              "    }else if(graph[i][0] == 'a'){\n"
              "      ctx.moveTo(x + graph[i][1]*scale, y + graph[i][2]*scale); ctx.arc(x+graph[i][1]*scale, y+graph[i][2]*scale, graph[i][3]*scale, graph[i][4], graph[i][5], graph[i][6]);\n"
              "    }else if(graph[i][0] == 'r'){\n"
              "      ctx.fillRect(x + graph[i][1]*scale, y + graph[i][2]*scale, graph[i][3]*scale, graph[i][4]*scale);\n"
              "    }else if(graph[i][0] == 'c'){\n"
              "      ctx.ellipse(x + graph[i][1]*scale, y + graph[i][2]*scale, graph[i][3]*scale, graph[i][4]*scale, 0,0,Math.PI*2); ctx.fill();\n"
              "    }\n"
              "  }\n"
              "  ctx.stroke();\n"
              "}\n"
              "\n");
}
void pack_html_export(pack_t *p, FILE *pf){
  if(p == NULL)return;
  if( (p->descr!=NULL)&&(strcmp(p->descr, "Dummy")==0) )return;
  //Рисуем контактные площадки
  fprintf(pf, "  const pins = [\n");
  float sz = 1e20;
  char cpin_found = 0;
  for(int i=0; i<p->pinn; i++){
    float w = p->pin[i].w;
    float h = p->pin[i].h;
    float x = p->pin[i].x;
    float y = p->pin[i].y;
    if(p->pin[i].name[0]){
      if(w < sz)sz = w;
      if(h < sz)sz = h;
    }
    if(pintr->pinshape[i] == pin_rect){
      x -= w/2; y-=h/2;
      fprintf(pf, "    ['r', %f, %f, %f, %f],//%i\n", x,y, w,h, i);
    }else if(pintr->pinshape[i] == pin_oval){
      fprintf(pf, "    ['c', %f, %f, %f, %f],//%i\n", x,y, w/2,h/2, i);
      cpin_found = 1;
    }
  }
  fprintf(pf, "  ];\n");
  fprintf(pf, "  const pin_names = [\n");
  int tblnum = 0;
  for(int i=0; i<p->pinn; i++){
    float x = p->pin[i].x;
    float y = p->pin[i].y;
    char orient = 'h';
    if(p->pin[i].w*1.1 < p->pin[i].h)orient = 'v';
    int pnum = tblnum;
    if((p->pin[i].name[0] != 0)&&(p->pin[i].vis))tblnum++; else pnum = -1;
    fprintf(pf, "    ['%c', %f, %f, \"%s\", %i], //%i\n", orient, x, y, p->pin[i].name, pnum, i);
  }
  fprintf(pf, "  ];\n");
  
  fprintf(pf, "  const graph = [\n");
  for(int i=0; i<pintr->graphn; i++){
    if(pintr->graph[i].type == pg_line){
      fprintf(pf, "    ['l', %f, %f, %f, %f], //%i\n", pintr->graph[i].line.x1, pintr->graph[i].line.y1, pintr->graph[i].line.x2, pintr->graph[i].line.y2, i);
      //fprintf(pf, "  ctx.moveTo(x + %f*scale, x + %f*scale);", pintr->graph[i].line.x1, pintr->graph[i].line.y1);
      //fprintf(pf, " ctx.lineTo(x + %f*scale, y + %f*scale); //%i\n", pintr->graph[i].line.x2, pintr->graph[i].line.y2, i);
    }else if(pintr->graph[i].type == pg_arc){
      float x1=pintr->graph[i].arc.x1;
      float x2=pintr->graph[i].arc.x2;
      float x3=pintr->graph[i].arc.x3;
      float y1=pintr->graph[i].arc.y1;
      float y2=pintr->graph[i].arc.y2;
      float y3=pintr->graph[i].arc.y3;
      float y21 = y2-y1;
      float y13 = y1-y3;
      float y32 = y3-y2;
      float X, Y, R;
      X = ( (x3*x3+y3*y3)*y21 + (x2*x2+y2*y2)*y13 + (x1*x1+y1*y1)*y32 ) / (x3*y21 + x2*y13 + x1*y32) / 2;
      if(fabs(y21) > 1e-10){
        Y = (x2*x2 - x1*x1 - 2*X*(x2-x1))/y21/2 + (y2+y1)/2;
      }else if(fabs(y13) > 1e-10){
        Y = (x1*x1 - x3*x3 - 2*X*(x1-x3))/y13/2 + (y1+y3)/2;
      }else{
        Y = 0;
        printf("err\n");
      }
      R = sqrt((X-x1)*(X-x1) + (Y-y1)*(Y-y1));
      float a1, a2, a3;
      a1 = atan2f( y1-Y, x1-X );
      a2 = atan2f( y2-Y, x2-X );
      a3 = atan2f( y3-Y, x3-X );
      char dir = 0;
      fprintf(pf, "    ['a', %f, %f, %f, %f, %f, %s], //%i\n", X, Y, R, a1, a3, dir?"true":"false", i);
      //fprintf(pf, "  ctx.moveTo(x + %f*scale, x + %f*scale);", X, Y);
      //fprintf(pf, " ctx.arc(x+%f*scale, y+%f*scale, %f*scale, %f, %f, %s); //%i\n", X, Y, R, a1, a3, dir?"true":"false", i);
    }else if(pintr->graph[i].type == pg_rect){
      fprintf(pf, "    ['r', %f, %f, %f, %f], //%i\n", pintr->graph[i].rect.x1, pintr->graph[i].line.y1, pintr->graph[i].line.x2 - pintr->graph[i].rect.x1, pintr->graph[i].rect.y2 - pintr->graph[i].rect.y1, i);
      //fprintf(pf, "  ctx.fillRect(x + %f*scale, x + %f*scale, %f*scale, %f*scale); //%i", pintr->graph[i].rect.x1, pintr->graph[i].line.y1, pintr->graph[i].line.x2 - pintr->graph[i].rect.x1, pintr->graph[i].rect.y2 - pintr->graph[i].rect.y1, i);
    }else if(pintr->graph[i].type == pg_circ){
      fprintf(pf, "    ['c', %f, %f, %f, %f], //%i\n", (pintr->graph[i].circ.x1+pintr->graph[i].circ.x2)/2, (pintr->graph[i].circ.y1+pintr->graph[i].circ.y2)/2, (pintr->graph[i].circ.x2-pintr->graph[i].circ.x1)/2, (pintr->graph[i].circ.y2-pintr->graph[i].circ.y1)/2, i);
      //fprintf(pf, "  ctx.ellipse(x + %f*scale, x + %f*scale, %f*scale, %f*scale, 0,0,Math.PI*2); ctx.fill(); //%i", (pintr->graph[i].circ.x1+pintr->graph[i].circ.x2)/2, (pintr->graph[i].circ.y1+pintr->graph[i].circ.y2)/2, (pintr->graph[i].circ.x2-pintr->graph[i].circ.x1)/2, (pintr->graph[i].circ.y2-pintr->graph[i].circ.y1)/2, i);
    }
  }
  fprintf(pf, "  ];\n");
  
  if(cpin_found) sz *= 0.7;
  fprintf(pf, "  pack_html_draw(ctx, tbl, x, y, scale, %f, pins, pin_names, drawnames, graph);\n", sz);
}