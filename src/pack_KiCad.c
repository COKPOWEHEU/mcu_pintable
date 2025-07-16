#include "pack_KiCad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef enum{
  pin_rect = 0,
  pin_oval,
  pin_unknown,
}pack_pinsh_t;

struct pack_pin{
  char name[20];
  pack_pinsh_t shape;
  float x, y;
  float w, h;
};

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

struct pack_graph{
  pg_type_t type;
  union{
    pg_line_t line;
    pg_arc_t arc;
  };
};

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
  for(int i=0; i<p->graphn; i++){
    switch(p->graph[i].type){
      case pg_line:
        setminmax(xmin, xmax, p->graph[i].line.x1);
        setminmax(xmin, xmax, p->graph[i].line.x2);
        setminmax(ymin, ymax, p->graph[i].line.y1);
        setminmax(ymin, ymax, p->graph[i].line.y2);
        break;
      case pg_arc:
        setminmax(xmin, xmax, p->graph[i].arc.x1);
        setminmax(xmin, xmax, p->graph[i].arc.x2);
        setminmax(xmin, xmax, p->graph[i].arc.x3);
        setminmax(ymin, ymax, p->graph[i].arc.y1);
        setminmax(ymin, ymax, p->graph[i].arc.y2);
        setminmax(ymin, ymax, p->graph[i].arc.y3);
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
  for(int i=0; i<p->graphn; i++){
    if(p->graph[i].type == pg_line){
      p->graph[i].line.x1 = (p->graph[i].line.x1 - x)/w;
      p->graph[i].line.x2 = (p->graph[i].line.x2 - x)/w;
      p->graph[i].line.y1 = (p->graph[i].line.y1 - y)/w;
      p->graph[i].line.y2 = (p->graph[i].line.y2 - y)/w;
    }else if(p->graph[i].type == pg_arc){
      p->graph[i].arc.x1 = (p->graph[i].arc.x1 - x)/w;
      p->graph[i].arc.x2 = (p->graph[i].arc.x2 - x)/w;
      p->graph[i].arc.x3 = (p->graph[i].arc.x3 - x)/w;
      p->graph[i].arc.y1 = (p->graph[i].arc.y1 - y)/w;
      p->graph[i].arc.y2 = (p->graph[i].arc.y2 - y)/w;
      p->graph[i].arc.y3 = (p->graph[i].arc.y3 - y)/w;
    }
  }
}

pack_t* pack_load(char *filename){
  FILE *pf = fopen(filename, "r");
  if(pf == NULL){fprintf(stderr, "pack_KiCad.pack_load: file [%s] not found\n", filename); return NULL;}
  
  pack_t *p = malloc(sizeof(pack_t));
  if(p == NULL)return NULL;
  p->name = NULL;
  p->descr = NULL;
  p->pin = NULL;
  p->pinn = 0;
  p->graph = NULL;
  p->graphn = 0;
  
  char name[1000];
  fscanf(pf, " (%s", name);
  if(strcmp(name, "footprint")!=0){
    fprintf(stderr, "pack_KiCad.pack_load: wrong file format [%s]\n", filename);
    free(p);
    return NULL;
  }
  fscanf(pf, " \"%[^\"]\"", name);
  p->name = strdup(name);
  
  int nst = 1;
  size_t pos = ftell(pf);
  do{
    nst = next_chank(pf, name, nst);
    if(nst <= 0)break;
    if((strcmp(name, "fp_line")==0)||(strcmp(name, "fp_arc")==0)){
      p->graphn++;
    }else if(strcmp(name, "pad")==0){
      p->pinn++;
      //printf("pad\n");
    }else if((strcmp(name, "version")==0)||(strcmp(name, "generator")==0)||(strcmp(name, "layer")==0)||(strcmp(name, "tedit")==0)||(strcmp(name, "descr")==0)||(strcmp(name, "tags")==0)||(strcmp(name, "attr")==0)||(strcmp(name, "fp_text")==0)||(strcmp(name, "model")==0)){
      //ignore
    }else{
      printf("unknown chank [%s]\n", name);
    }
    skip_chank(pf, 1);
    nst--;
  }while(nst > 0);
  
  fseek(pf, pos, SEEK_SET);
  p->graph = malloc(sizeof(pack_graph_t) * p->graphn);
  p->pin = malloc(sizeof(pack_pin_t) * p->pinn);
  if((p->graph==NULL)||(p->pin==NULL)){
    if(p->graph == NULL)free(p->graph);
    if(p->pin == NULL)free(p->pin);
    fclose(pf);
    fprintf(stderr, "pack_KiCad.pack_load: not enough memory\n");
    return NULL;
  }
  
  int gr = 0, pin=0;
  nst = 1;
  do{
    nst = next_chank(pf, name, nst);
    if(nst <= 0)break;
    if(strcmp(name, "fp_line")==0){
      find_chank(pf, "start");
      fscanf(pf, "%f%f", &(p->graph[gr].line.x1), &(p->graph[gr].line.y1));
      find_chank(pf, NULL); find_chank(pf, "end");
      fscanf(pf, "%f%f", &(p->graph[gr].line.x2), &(p->graph[gr].line.y2));
      find_chank(pf, NULL);
      p->graph[gr].type = pg_line; gr++;
    }else if(strcmp(name, "fp_arc")==0){
      find_chank(pf, "start");
      fscanf(pf, "%f%f", &(p->graph[gr].arc.x1), &(p->graph[gr].arc.y1));
      find_chank(pf, NULL); find_chank(pf, "mid");
      fscanf(pf, "%f%f", &(p->graph[gr].arc.x2), &(p->graph[gr].arc.y2));
      find_chank(pf, NULL); find_chank(pf, "end");
      fscanf(pf, "%f%f", &(p->graph[gr].arc.x3), &(p->graph[gr].arc.y3));
      find_chank(pf, NULL);
      p->graph[gr].type = pg_arc; gr++;
    }else if(strcmp(name, "pad")==0){
      fscanf(pf, " \"%[^\"]\"", p->pin[pin].name);
      fscanf(pf, "%*s"); //smd / thru_hole / ...
      fscanf(pf, "%s", name);
      if(strcmp(name, "roundrect")==0){
        p->pin[pin].shape = pin_rect;
      }else if(strcmp(name, "rect")==0){
        p->pin[pin].shape = pin_rect;
      }else if(strcmp(name, "oval")==0){
        p->pin[pin].shape = pin_oval;
      }else if(strcmp(name, "circle")==0){
        p->pin[pin].shape = pin_oval;
      }else{
        p->pin[pin].shape = pin_unknown;
      }
      find_chank(pf, "at");
      fscanf(pf, "%f%f", &(p->pin[pin].x), &(p->pin[pin].y));
      find_chank(pf, NULL); find_chank(pf, "size");
      fscanf(pf, "%f%f", &(p->pin[pin].w), &(p->pin[pin].h));
      //p->pin[pin].w *= 1.5; p->pin[pin].h *= 1.5;
      find_chank(pf, NULL);
      pin++;
    }else if((strcmp(name, "version")==0)||(strcmp(name, "generator")==0)||(strcmp(name, "layer")==0)||(strcmp(name, "tedit")==0)||(strcmp(name, "descr")==0)||(strcmp(name, "tags")==0)||(strcmp(name, "attr")==0)||(strcmp(name, "fp_text")==0)||(strcmp(name, "model")==0)){
      //ignore
    }else{
      printf("unknown chank [%s]\n", name);
    }
    skip_chank(pf, 1);
    nst--;
  }while(nst > 0);
  pack_normalize(p);
  
  fclose(pf);
  
  printf("Graph: %i\tpads: %i\n", p->graphn, p->pinn);
  
  return p;
}

void pack_free(pack_t *p){
  if(p == NULL)return;
  if(p->name){free(p->name); p->name = NULL;}
  if(p->descr){free(p->descr); p->descr = NULL;}
  if(p->pin){free(p->pin); p->pin = NULL;}
  if(p->graph){free(p->graph); p->graph = NULL;}
  free(p);
}

void pack_test(pack_t *p){
  float x, y, w, h;
  const char *pinsh_name[] = {"rect", "oval", "???"};
  if(p == NULL)return;
  printf("name = [%s]%i\t%i\n", p->name, p->pinn, p->graphn);
  pack_graph_size(p, &x, &y, &w, &h);
  printf("[%f ; %f]\t(%f x %f)\n", x, y, w, h);
  for(int i=0; i<p->pinn; i++){
    printf("%i (%s): %s (%f,%f) (%f,%f)\n", i, p->pin[i].name, pinsh_name[p->pin[i].shape], p->pin[i].x, p->pin[i].y, p->pin[i].w, p->pin[i].h);
  }
  for(int i=0; i<p->graphn; i++){
    switch(p->graph[i].type){
      case pg_line: printf("%i: line(%f,%f...%f,%f)\n", i, p->graph[i].line.x1, p->graph[i].line.y1, p->graph[i].line.x2, p->graph[i].line.y2); break;
      case pg_arc: printf("%i: arc(%f,%f %f,%f, %f,%f)\n", i, p->graph[i].arc.x1, p->graph[i].arc.y1, p->graph[i].arc.x2, p->graph[i].arc.y2, p->graph[i].arc.x3, p->graph[i].arc.y3); break;
    }
  }
}


void pack_html_export(pack_t *p, FILE *pf, float size){
  float S = size;
  fprintf(pf, "  const patternCanvas = document.createElement(\"canvas\");\n"
              "  const patternContext = patternCanvas.getContext(\"2d\");\n"
              "  patternCanvas.width = 10;\n"
              "  patternCanvas.height = 10;\n"
              "  patternContext.fillRect(0, 0, 5, 5);\n"
              "  const pattern = ctx.createPattern(patternCanvas, \"repeat\");\n"
              "  ctx.fillStyle =\"rgb(200 200 200)\";\n"
              "\n");
  
  //Рисуем контактные площадки
  fprintf(pf, "  const pins = [\n");
  float sz = 1e20;
  for(int i=0; i<p->pinn; i++){
    float w = p->pin[i].w*S;
    float h = p->pin[i].h*S;
    float x = p->pin[i].x*S;
    float y = p->pin[i].y*S;
    if(p->pin[i].name[0]){
      if(w < sz)sz = w;
      if(h < sz)sz = h;
    }
    if(p->pin[i].shape == pin_rect){
      x -= w/2; y-=h/2;
      fprintf(pf, "    ['r', %f, %f, %f, %f],//%i\n", x,y, w,h, i);
    }else if(p->pin[i].shape == pin_oval){
      fprintf(pf, "    ['r', %f, %f, %f, %f],//%i\n", x,y, w/2,h/2, i);
    }
  }
  fprintf(pf, "  ];\n"
              "\n"
              "  for(let i=0; i<pins.length; i++){\n"
              "    let col = \"rgb(200 200 200)\";\n"
              "    if(i < tbl.length){\n"
              "      if(tbl[i].bgcolor != \"\"){\n"
              "        col = tbl[i].bgColor;\n"
              "      }\n"
              "    }\n"
              "    ctx.fillStyle = col;\n"
              "    if(table_selected == i){\n"
              "      ctx.fillStyle = pattern;\n"
              "    }\n"
              "    if(pins[i][0] == 'r'){\n"
              "      ctx.fillRect(pins[i][1], pins[i][2], pins[i][3], pins[i][4]);\n"
              "    }else{\n"
              "      ctx.beginPath();\n"
              "      ctx.ellipse(pins[i][1], pins[i][2], pins[i][3], pins[i][4], 0, 0, Math.PI*2);\n"
              "      ctx.fill();\n"
              "    }\n"
              "    ctx.fillStyle = \"rgb(200 200 200)\";\n"
              "  }\n"
        );
  
  //Рисуем текст на площадках
  fprintf(pf, "  ctx.fillStyle =\"rgb(0 0 0)\";\n"
              "  ctx.textBaseline = \"middle\";\n"
              "  ctx.textAlign = \"center\";\n");
  fprintf(pf, "  ctx.font = \"%fpx serif\";\n", sz);
  fprintf(pf, "  const pin_names = [\n");
  for(int i=0; i<p->pinn; i++){
    float x = p->pin[i].x*S;
    float y = p->pin[i].y*S;
    char orient = 'h';
    if(p->pin[i].w*1.1 < p->pin[i].h)orient = 'v';
    
    fprintf(pf, "    ['%c', %f, %f, \"%s\"], //%i\n", orient, x, y, p->pin[i].name, i);
  }
  fprintf(pf,"  ];\n"
             "\n"
             "  let idx = -1;\n"
             "  let hdr = tbl[0].parentElement.parentElement.children[0].children[0].children;\n"
             "  for(let i=0; i<hdr.length; i++){\n"
             "    if(hdr[i].textContent == \"Comm\"){idx = i; break;}\n"
             "  }\n"
             "  for(let i=0; i<pin_names.length; i++){\n"
             "    let name = pin_names[i][3];\n"
             "    if((idx >= 0) && (i < tbl.length)){\n"
             "      let val = tbl[i].children[idx].children[0].value;\n"
             "      if(val != \"\")name = val;\n"
             "    }\n"
             "    if(pin_names[i][0] == 'h'){\n"
             "      ctx.strokeText(name, pin_names[i][1], pin_names[i][2]);\n"
             "    }else{\n"
             "      ctx.save(); ctx.translate(pin_names[i][1], pin_names[i][2]);\n"
             "      ctx.rotate(-Math.PI/2); ctx.strokeText(name, 0, 0);\n"
             "      ctx.restore();\n"
             "    }\n"
             "  }\n"
  );
  
  //Рисуем всю прочую графику
  fprintf(pf, "  ctx.beginPath();\n");
  for(int i=0; i<p->graphn; i++){
    if(p->graph[i].type == pg_line){
      fprintf(pf, "  ctx.moveTo(%f, %f);", p->graph[i].line.x1*S, p->graph[i].line.y1*S);
      fprintf(pf, " ctx.lineTo(%f, %f); //%i\n", p->graph[i].line.x2*S, p->graph[i].line.y2*S, i);
    }else if(p->graph[i].type == pg_arc){
      float x1=p->graph[i].arc.x1;
      float x2=p->graph[i].arc.x2;
      float x3=p->graph[i].arc.x3;
      float y1=p->graph[i].arc.y1;
      float y2=p->graph[i].arc.y2;
      float y3=p->graph[i].arc.y3;
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
      X *=S; Y *=S; R *= S;
      char dir = 0;
      fprintf(pf, "  ctx.arc(%f, %f, %f, %f, %f, %s); //%i\n", X, Y, R, a1, a3, dir?"true":"false", i);
    }
  }
  fprintf(pf, "  ctx.stroke();\n");
  
}