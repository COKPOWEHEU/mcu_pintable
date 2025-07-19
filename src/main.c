#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pack_KiCad.h"

//char *configfile = "config.cfg";
const char *pack_defaultpath[] = {"./"};

int linenum = 0;
char fatalflag = 0;

void help(char *name){
  printf("Usage:\n\t%s inputfile.pintable [outputfile.html]\n", name);
}

char* make_out_name(char *iname){
  char *extpos = strrchr(iname, '.');
  if(extpos == NULL)extpos = iname + strlen(iname);
  size_t sz = extpos - iname;
  char *res = malloc(sz + 10 );
  memcpy(res, iname, sz);
  memcpy(res+sz, ".html", 5);
  res[sz+5] = 0;
  return res;
}

typedef void (*dirfiles_func_t)(char *filename, void *data);
void dirfiles_read(char *dirname, char *extname, dirfiles_func_t callback, void *userdata);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////// Parse input file ///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

pack_t *pack = NULL;
char **pack_altname = NULL;
#define altname(i) pack_altname[ ((size_t)mcu[i].pack - (size_t)pack) / sizeof(pack_t) ]
size_t packnum = 0;
size_t packalloc = 0;
const size_t packalloc_dn = 10;

typedef struct{
  char name[20];
  char num[10];
  char *funcs;
}periphlist_t;

typedef struct{
  char name[100];
  char *packname;
  pack_t *pack;
  periphlist_t *per;
}mcu_t;
mcu_t *mcu = NULL;
int mcun = 0;
int mcualloc = 0;
const int mcualloc_dn = 5;

typedef struct{
  char name[100];
  size_t nlen;
  char altname[20];
  size_t alen;
}periph_t;
periph_t *periph = NULL;
int periphn = 0;
int periphalloc = 0;
const int periphalloc_dn = 5;
periph_t per_srv = {
  .name="Service",
  .nlen=7,
  .altname="SRV",
  .alen=3,
};

void packages_parse(char *buf){
  const char delim[] = "; \t\r\n";
  char *str = strtok(buf, delim);
  int res;
  if(pack_altname == NULL){fprintf(stderr, "package list is empty\n"); return;}
  while(str != NULL){
    char name[100], altname[100];
    res = sscanf(str, "%100[^[][%100[^]]]", altname, name);
    if(res < 1){fprintf(stderr, "Wrong format at %i\n", linenum); fatalflag=1; return;}
    int idx = -1;
    for(int i=0; i<packnum; i++){
      if(strcmp(altname, pack[i].name)==0){idx = i; break;}
      if(res > 1)if(strcmp(name, pack[i].name)==0){idx = i; break;}
    }
    //printf("pack %i [%s|%s]\n", idx, name, altname);
    if(idx >= 0){
      if(pack_altname[idx])free(pack_altname[idx]);
      if(res == 2){
        pack_altname[idx] = strdup(altname);
      }else{
        pack_altname[idx] = strdup(name);
      }
    }
    
    str = strtok(NULL, delim);
  }
}
int pack_search(char *name){
  if(pack == NULL)return -1;
  if(pack_altname == NULL){fprintf(stderr, "pack_altname not inited\n"); return -1;}
  for(int i=0; i<packnum; i++){
    if(strcmp(name, pack[i].name)==0)return i;
    if(pack_altname[i]!=NULL)if(strcmp(name, pack_altname[i])==0)return i;
  }
  return -1;
}
void package_free(){
  if(!pack_altname)return;
  for(int i=0; i<packnum; i++)if(pack_altname[i])free(pack_altname[i]);
  free(pack_altname);
  pack_altname = NULL;
}

void mcu_parse(char *buf){
  const char delim[] = "; \t\r\n";
  char *str = strtok(buf, delim);
  int res;
  while(str != NULL){
    //realloc 'mcu' buffer if needed
    if(mcun+1 >= mcualloc){
      mcu_t *prev = mcu;
      mcu = realloc(mcu, sizeof(mcu_t)*(mcualloc+mcualloc_dn));
      if(mcu == NULL){
        fprintf(stderr, "Not enough memory\n");
        fatalflag = 1;
        free(prev);
        return;
      }
      mcualloc += mcualloc_dn;
    }
    //read next mcu
    mcu_t *m = &(mcu[mcun]);
    m->pack = NULL;
    m->per = NULL;
    m->packname = NULL;
    
    char packname[100];
    res = sscanf(str, "%20[^[][%100[^]]", m->name, packname);
    if(res != 2){
      fprintf(stderr, "%i: Error: wring MCU format [%s]\n", linenum, str);
      fatalflag = 1; return;
    }
    m->packname = strdup(packname);
    
    mcun++;
    
    str = strtok(NULL, delim);
  }
}
void mcu_free(){
  if(mcu == NULL)return;
  for(int i=0; i<mcun; i++){
    if(mcu[i].packname)free(mcu[i].packname);
    if(mcu[i].per){
      for(int j=0; j<mcu[i].pack->pinn; j++){
        //printf("mcu [%s.%s].%i = %s\n", mcu[i].name, altname(i), j, mcu[i].per[j].funcs);
        if(mcu[i].per[j].funcs)free(mcu[i].per[j].funcs);
      }
      free(mcu[i].per);
    }
  }
  free(mcu);
  mcu = NULL; mcun = 0; mcualloc = 0;
}
void mcu_resolv_deps(){
  if(mcu == NULL)return;
  for(int i=0; i<mcun; i++){
    if(mcu[i].packname == NULL)continue;
    int idx = pack_search(mcu[i].packname);
    if(idx >= 0){
      mcu[i].pack = &(pack[idx]);
      free(mcu[i].packname);
      mcu[i].packname = NULL;
      int np = mcu[i].pack->pinn;
      mcu[i].per = malloc(sizeof(periphlist_t) * np);
      if(mcu[i].per == NULL){
        fprintf(stderr, "Not enough memory\n");
        fatalflag = 1; return;
      }
      for(int j=0; j<np; j++){
        mcu[i].per[j].name[0] = 0;
        mcu[i].per[j].num[0] = 0;
        mcu[i].per[j].funcs = NULL;
      }
    }else{
      fprintf(stderr, "MCU [%s]: package [%s] not found\n", mcu[i].name, mcu[i].packname);
    }
  }
}
void mcu_show(){
  if(mcu == NULL)return;
  for(int i=0; i<mcun; i++){
    printf("%s . %s: %s\n", mcu[i].name, altname(i), mcu[i].pack->name);
    if(mcu[i].per == NULL){printf("err\n"); continue;}
    for(int j=0; j<mcu[i].pack->pinn; j++){
      printf("%i: %s | %s\n", j, mcu[i].per[j].name, mcu[i].per[j].funcs);
    }
  }
}
int str_to_pinnum(char *s, int mcunum){
  pack_t *p = mcu[mcunum].pack;
  for(int i=0; i<p->pinn; i++){
    if(strcmp(s, p->pin[i].name)==0)return i;
  }
  return -1;
}

void parse_periph(char *buf){
  const char delim[] = "; \t\r\n";
  char *str = strtok(buf, delim);
  int res;
  while(str != NULL){
    //realloc 'periph' buffer if needed
    if(periphn+1 >= periphalloc){
      periph_t *prev = periph;
      periph = realloc(periph, sizeof(periph_t)*(periphalloc+periphalloc_dn));
      if(periph == NULL){
        fprintf(stderr, "Not enough memory\n");
        fatalflag = 1;
        free(prev);
        return;
      }
      periphalloc += periphalloc_dn;
    }
    //read next periph
    res = sscanf(str, "%100[^[][%20[^]]", periph[periphn].name, periph[periphn].altname);
    if(res == 1){
      periph[periphn].altname[0] = 0;
    }else if(res < 1){
      fprintf(stderr, "%i: Error: wring MCU format [%s]\n", linenum, str);
      fatalflag = 1; return;
    }
    periph[periphn].nlen = strlen( periph[periphn].name );
    periph[periphn].alen = strlen( periph[periphn].altname );
    
    periphn++;
    
    str = strtok(NULL, delim);
  }
}
void periph_free(){
  if(periph == NULL)return;
  free(periph);
  periph = NULL; periphn = 0; periphalloc = 0;
}

void test_deps(){
  /*if(packages == NULL){fprintf(stderr, "'Packages' section not found\n"); fatalflag = 1; return;}
  for(int i=0; i<packagenum; i++){
    if(packages[i].npins == 0){
      fprintf(stderr, "Wrong pin number in [%s]\n", packages[i].name);
      fatalflag = 1;
      return;
    }
  }*/
#warning TODO
  if(mcu == NULL){fprintf(stderr, "'MCU' section not found\n");fatalflag = 1; return;}
  for(int i=0; i<mcun; i++){
    if(mcu[i].pack == NULL){
      fprintf(stderr, "MCU [%s]: package [%s] not found\n", mcu[i].name, mcu[i].packname);
      fatalflag = 1;
      return;
    }
  }
  if(periph == NULL){fprintf(stderr, "'Periph' section not found\n");fatalflag = 1; return;}
  
}

void content_skip(char *buf, FILE *pf){
  char sym = '|';
  size_t pos = 0;
  char buffer[4096];
  while(sym == '|'){
    pos = ftell(pf);
    if(fgets(buffer, sizeof(buffer), pf)==NULL)return;
    buf = buffer;
    while(isspace(buf[0]))buf++;
    sym = buf[0];
    linenum++;
  }
  linenum--;
  fseek(pf, pos, SEEK_SET);
}

char mcu_match(char *str, int mcuidx){
  char *prevstr = str;
  mcu_t *m = &(mcu[mcuidx]);
  if(str[0] != '['){
    char *en = strchr(str, '[');
    size_t len;
    if(en != NULL)len = en - str; else len = strlen(str);
    if(strncmp(m->name, str, len)!=0){return 0;}
    str += len;
  }
  if(str[0] == '['){
    str++;
    char *en = strchr(str, ']');
    if(en == NULL){fprintf(stderr, "%i: Wrong MCU format [%s]\n", linenum, prevstr); return 0;}
    size_t len = en - str;
    if((strncmp(m->pack->name, str, len)!=0)&&(strncmp(altname(mcuidx), str, len)!=0)){return 0;}
  }
  return 1;
}

void content_read_header(char *buf, int *npacks, char **tbl){
  const char delim[] = "| \t\n";
  const char delim2[]= "; \t";
  char *str = strtok(buf, delim);
  str = strtok(NULL, delim); //skip 'Name' field
  int np = 0;
  char **names;
  names = malloc(sizeof(char*) * mcun);
  if(names == NULL){fprintf(stderr, "Not enough memory\n"); *npacks=0; fatalflag = 1; return;}
  for(int i=0; i<mcun; i++)names[i] = NULL;
  
  //Read MCU names
  while(str != NULL){
    if(strcmp(str, "Periph")==0)break;
    names[np] = str;
    np++;
    str = strtok(NULL, delim);
  }
  
  if(np == 0){
    fprintf(stderr, "%i: 'MCU' list not found\n", linenum);
    fatalflag = 1;
    *npacks = 0; *tbl = NULL;
  }
  *npacks = np;
  char *res = malloc(sizeof(char) * np * mcun);
  *tbl = res;
  if(res == NULL){
    fprintf(stderr, "Not enough memory\n"); *npacks=0; fatalflag = 1; return;
  }
  for(int i=0; i<(np*mcun); i++)res[i] = 0;
  
  for(int i=0; i<np; i++){
    int addr = i*mcun;
    str = strtok(names[i], delim2);
    while(str != NULL){
      for(int j=0; j<mcun; j++){
        res[addr + j] |= mcu_match(str, j);
      }
      str = strtok(NULL, delim2);
    }
  }
  
  free(names);
}

void content_parse(char *capt, FILE *pf){
  size_t pos = 0;
  char buf[4096];
  int npacks = 0;
  char *mcuflag;
  //read table header
  if(fgets(buf, sizeof(buf), pf)==NULL){
    fprintf(stderr, "%i: Unexpected end of file\n", linenum);
    return;
  }
  linenum++;
  content_read_header(buf, &npacks, &mcuflag);
  
  //read table separator
  if(fgets(buf, sizeof(buf), pf)==NULL){
    fprintf(stderr, "%i: Unexpected end of file\n", linenum);
    if(mcuflag)free(mcuflag);
    return;
  }
  if(mcuflag == NULL)return;
  linenum++;
  //read content
  while(1){
    pos = ftell(pf);
    if(fgets(buf, sizeof(buf), pf)==NULL)break;
    linenum++;
    char *ch = buf;
    while(isspace(ch[0]))ch++;
    if(ch[0] != '|')break;
    
    //find 'Periph' field
    char *per = strrchr(buf, '|');
    per[0] = 0;
    while(per[0] != '|'){
      if(per<=buf){fprintf(stderr, "%i: Wrong table line", linenum); fatalflag = 1; free(mcuflag); return;}
      per--;
    }
    per[0] = 0;
    per++;
    
    //find 'Name' field
    const char delim[] = "| \t\n";
    char *str, *name = strtok(buf, delim);
    if(name == NULL){fprintf(stderr, "%i: Wrong table line", linenum); fatalflag = 1; free(mcuflag); return;}
    
    //'MCU' fields
    for(int i=0; i<npacks; i++){
      str = strtok(NULL, delim);
      if(str == NULL){fprintf(stderr, "%i: Wrong table line", linenum); fatalflag = 1; free(mcuflag); return;}
      if(str[0] == '-')continue;
      int pin;
      
      size_t pos = i*mcun;
      for(int j=0; j<mcun; j++){
        if(mcuflag[pos + j]){
          pin = str_to_pinnum(str, j);
          if((pin < 0) || (pin>=mcu[j].pack->pinn)){
            fprintf(stderr, "MCU [%s.%s]: pin [%s] not found\n", mcu[j].name, mcu[j].pack->name, str);
          }else{
            strncpy(mcu[j].per[pin].name, name, 20);
            strncpy(mcu[j].per[pin].num, str, 10);
            if(mcu[j].per[pin].funcs == NULL){
              mcu[j].per[pin].funcs = strdup(per);
            }else{
              //printf("%i realloc %s.%s([%i]%s) = [%s] + [%s]\n", linenum, mcu[j].name, altname(j), pin, mcu[j].pack->pin[pin].name, mcu[j].per[pin].funcs, per);
              mcu[j].per[pin].funcs = realloc(mcu[j].per[pin].funcs, strlen(mcu[j].per[pin].funcs)+strlen(per)+2);
              strcat(mcu[j].per[pin].funcs, per);
            }
          }
          //printf("%s: %i | %s\n", mcu[j].name, pin, per);
        }
      }
    }
  }
  free(mcuflag);
  linenum--;
  fseek(pf, pos, SEEK_SET);
}

void test_mcu_complete(){
  for(int i=0; i<mcun; i++){
    int pins = 0;
    int pinn = 0;
    for(int j=0; j<mcu[i].pack->pinn; j++){
      if(mcu[i].per && mcu[i].per[j].funcs)pins++;
      if(mcu[i].pack->pin[j].name[0] != 0)pinn++;
    }
    float p = pins;
    p = p*100 / pinn;
    //int idx = ((size_t)mcu[i].pack - (size_t)pack)/sizeof(pack_t);
    //printf("[%s.%s]:\t%i / %i = %.1f%%\n", mcu[i].name, pack_altname[idx], pins, mcu[i].pack->pinn, p);
    printf("[%s.%s]:\t%i / %i = %.1f%%\n", mcu[i].name, altname(i), pins, pinn, p);
  }
}

int test_periph(char *func, periph_t *per){
  if(per == NULL)return -1;
  if((strncmp(func, per->name, per->nlen)==0) && (func[per->nlen] == '.')){
    return per->nlen + 1;    
  }else if((per->alen > 0) && (strncmp(func, per->altname, per->alen)==0) && (func[per->alen] == '.')){
    return per->alen + 1;
  }
  return -1;
}

char* periph_remap(char *str){
  static char buf[4096];
  if(str[0] != '[')return str;
  char *ch = strrchr(str, ']');
  if(ch == NULL)return str;
  int rmnum = 0;
  int res = sscanf(ch+1, "%i", &rmnum);
  ch[0] = 0;
  if(res > 0){
    sprintf(buf, "<span class=\"hltext\">%s<sup>%i</sup></span>", str+1, rmnum);
  }else{
    sprintf(buf, "<span class=\"hltext\">%s</span>", str+1);
  }
  ch[0] = ']';
  return buf;
}

char* match_periph(char *funcs, periph_t *per){
  static char buf[1000];
  char inp[4094];
  strcpy(inp, funcs);
  char delim[] = "; \t";
  char *str = strtok(inp, delim);
  buf[0] = 0;
  while(str != NULL){
    int pos = -1;
    if(per != NULL){
      pos = test_periph(str, per);
    }else{
      for(int i=0; i<periphn; i++){
        pos = test_periph(str, &periph[i]);
        if(pos >= 0)break;
      }
      if(pos < 0)pos = 0; else pos = -1;
      if(pos == 0){if(test_periph(str, &per_srv)>=0)pos=-1;}
    }
    if(pos >= 0){
      str += pos;
      strcat(buf, periph_remap(str));
      strcat(buf, ", ");
    }
    
    str = strtok(NULL, delim);
  }
  str = strrchr(buf, ',');
  if(str != NULL)str[0] = 0;
  return buf;
}

void pack_import(char *filename, void *data){
  //printf("[%s]\n", filename);
  pack_t *p = pack_load(filename);
  if(p == NULL)return;
  if(packnum+1 >= packalloc){
    //printf("realloc %i\n", packalloc + packalloc_dn);
    pack_t *prev = pack;
    pack = realloc(pack, sizeof(pack_t)*(packalloc + packalloc_dn));
    if(pack == NULL){
      fprintf(stderr, "Not enough memory\n");
      fatalflag = 1;
      for(int i=0; i<packnum; i++)pack_free(&prev[i]);
      free(prev);
      return;
    }
    packalloc += packalloc_dn;
  }
  memcpy(&(pack[packnum]), p, sizeof(pack_t));
  packnum++;
  free(p);
}
void packs_import(char *dirname){
  dirfiles_read(dirname, ".kicad_mod", pack_import, NULL);
  char **prev = pack_altname;
  pack_altname = realloc(pack_altname, sizeof(char*)*packnum);
  if(pack_altname == NULL){
    fprintf(stderr, "Not enough memory\n");
    fatalflag = 1;
    free(prev);
  }
  for(int i=0; i<packnum; i++)pack_altname[i] = strdup("");
}
void packs_show(){
  if(pack == NULL)return;
  for(int i=0; i<packnum; i++){
    //printf("%i: [%s](%i)\n", i, pack[i].name, pack[i].pinn);
    //pack_test(&pack[i]);
  }
}
void packs_free(){
  if(pack == NULL)return;
  for(int i=0; i<packnum; i++){pack_free(&(pack[i]));}
  free(pack);
  pack = NULL; packnum = 0; packalloc = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////// HTML export ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void html_write_style(FILE *pf){
  fprintf(pf, 
    "  table{\n"
    "    border-collapse: collapse; /* Убираем двойные линии между ячейками */ \n"
    "  }\n"
    "  td, th{\n"
    "    border: 1px solid black; /* Параметры рамки */ \n"
    "    width:auto;\n"
    "    text-align: center;\n"
    "  }\n"
    "  th{\n"
    "    background: #b0e0e6; /* Цвет фона */ \n"
    "    position: sticky; top: 0; /*Зафиксировать заголовок*/\n"
    "  }\n"
    "  .noBorder, .noBorder th, .noBorder td {\n"
    "    border: none !important; /* В результате границы становятся абсолютно незаметными */ \n"
    "  }\n"
    "  .hltext{\n"
    "    color: #000000;\n"
    "    background-color:#ff808080;\n"
    "  }\n"
    "  input[type=\"checkbox\"]{\n"
    "    margin-right: 20px;\n"
    "  }\n"
    "input[type=\"color\"]{\n"
    "  box-sizing     : border-box;\n"
    "  border: none;\n"
    "  height: 20px;\n"
    "  width: 20px;\n"
    "}\n"
    "input[type=\"text\"]{\n"
    "  box-sizing: border-box;\n"
    "  border: none;\n"
    "  height: 20px;\n"
    "}\n"
    "#col-1 {\n"
    "  position: relative;\n"
    "  width: 70%%;\n"
    "  float: left;\n"
    "  height: 100%%;\n"
    "}\n"
    "\n"
    "#col-2 {\n"
    "  position: relative;\n"
    "  width: 30%%;\n"
    "  float: right;\n"
    "  height: 100%%;\n"
    "}\n"
    ".table_wrapper{\n"
    "  display: block;\n"
    "  overflow-x: auto;\n"
    "  height: 80%%;\n"
    "  white-space: nowrap;\n"
    "}\n"
  );
}

void html_write_drawfuncs(FILE *pf, float size);
void html_write_scripts(FILE *pf){
  fprintf(pf, "function SelectPeriph(name){\n");
  fprintf(pf, "  let mcu = document.getElementById(\"mcuselected\").value;\n");
  fprintf(pf, "  let tbl = document.getElementsByName(mcu);\n");
  fprintf(pf, "  let ctl = document.getElementsByName(\"Per.\"+name);\n");
  fprintf(pf, "  let hdr = tbl[0].children[0].children[0];\n");
  fprintf(pf, "  let idx = -1;\n");
  fprintf(pf, "  let vis = \"\";\n");
  fprintf(pf, "  for(let i=0; i<hdr.children.length; i++){\n");
  fprintf(pf, "    if(hdr.children[i].innerText == name){idx = i; break;}\n");
  fprintf(pf, "  }\n");
  fprintf(pf, "  if(idx < 0)return;\n");
  fprintf(pf, "  hid = ! ctl[0].checked;\n");
  fprintf(pf, "  hdr.children[idx].hidden = hid;\n");
  fprintf(pf, "  \n");
  fprintf(pf, "  hdr = tbl[0].children[1];\n");
  fprintf(pf, "  for(let i=0; i<hdr.children.length; i++){\n");
  fprintf(pf, "    if(hdr.children[i].children.length < 2)continue;\n");
  fprintf(pf, "    hdr.children[i].children[idx].hidden = hid;\n");
  fprintf(pf, "  }\n");
  fprintf(pf, "}\n\n");

  fprintf(pf, "function SrvVisible(){\n"
              "  let mcu = document.getElementById(\"mcuselected\").value;\n"
              "  let tbl = document.getElementsByName(mcu);\n"
              "  let ctl = document.getElementById(\"srvsel\").value;\n"
              "  let sh_F=true, sh_O=true;\n"
              "  let check = document.getElementsByName(\"Per.Service\")[0];\n"
              "  if(ctl == \"srv_show\"){sh_F=true; sh_O=true;}\n"
              "  if(ctl == \"srv_opt\"){sh_F=false; sh_O=true;}\n"
              "  if(ctl == \"srv_hide\"){sh_F=false; sh_O=false;}\n"
              "  tbl = tbl[0].children[1].children;\n"
              "  for(let i=0; i<tbl.length; i++){\n"
              "    if(tbl[i].className == \"tbl_srv_F\"){\n"
              "      tbl[i].hidden = !sh_F;\n"
              "    }else if(tbl[i].className == \"tbl_srv_O\"){\n"
              "      tbl[i].hidden = !sh_O;\n"
              "    }else{\n"
              "      tbl[i].hidden = false;\n"
              "    }\n"
              "  }\n"
              "  check.checked = sh_O;\n"
              "  SelectPeriph('Service');\n"
              "}\n\n");
  
  fprintf(pf, "function SelectColor(linenum){\n"
              "  var mcu = document.getElementById(\"mcuselected\").value;\n"
              "  var tbl = document.getElementsByName(mcu);\n"
              "  var lin = tbl[0].children[1];\n"
              "  var col = -1;\n"
              "  lin = tbl[0].children[1].children[linenum].getElementsByTagName(\"input\");\n"
              "  for(let i=0; i<lin.length; i++){\n"
              "    if(lin[i].name == \"usersel\"){\n"
              "      col = lin[i].value;\n"
              "      break;\n"
              "    }\n"
              "  }\n"
              "  if(col == -1)return;\n"
              "  lin = tbl[0].children[1].children[linenum];\n"
              "  if(col == \"#ffffff\")col=\"\";\n"
              "  lin.bgColor = col;\n"
              "}\n\n");
  
  fprintf(pf, "function SelectMCU(){\n"
              "  var mcu = document.getElementById(\"mcuselected\").value;\n"
              "  var tables = document.getElementsByTagName(\"table\");\n"
              "  for(let i=0; i<tables.length; i++){\n"
              "    tables[i].hidden = (tables[i].attributes.name.value != mcu);\n"
              "  }\n"
              "  Periph_Vis_update();\n"
              "  package_draw();\n"
              "}\n\n");
  
  fprintf(pf, "function Periph_Vis_update(){\n"
              "  SrvVisible();\n");
  for(int i=0; i<periphn; i++){
    fprintf(pf, "  SelectPeriph('%s');\n", periph[i].name);
  }
  fprintf(pf, "  SelectPeriph('Other');\n"
              "  SelectPeriph('Service');\n"
              "}\n");
  
  
  fprintf(pf, "function OnLoad(){\n"
              "  SelectMCU();\n"
              "  Periph_Vis_update()\n"
              "}\n");
  
  html_write_drawfuncs(pf, 500);
}

void html_write_ui(FILE *pf){
  fprintf(pf, "<select id=\"mcuselected\" onchange=\"SelectMCU()\">\n");
  for(int i=0; i<mcun; i++){
    //fprintf(pf, "  <option value=\"%s.%s\">%s.%s</option>\n", mcu[i].name, mcu[i].pack->name, mcu[i].name, mcu[i].pack->name);
    fprintf(pf, "  <option value=\"%s.%s\">%s.%s</option>\n", mcu[i].name, altname(i), mcu[i].name, altname(i));
  }
  fprintf(pf, "</select>\n\n<br>\n");
  for(int i=0; i<periphn; i++){
    fprintf(pf, "<label for=\"Per.%s\">%s</label><input type=\"checkbox\" id=\"Per.%s\" name=\"Per.%s\" checked onclick=\"SelectPeriph('%s');\"/>\n", periph[i].name, periph[i].name, periph[i].name, periph[i].name, periph[i].name);
  }
  fprintf(pf, "<label for=\"Per.%s\">%s</label><input type=\"checkbox\" id=\"Per.%s\" name=\"Per.%s\" checked onclick=\"SelectPeriph('%s');\"/>\n", "Other", "Other", "Other", "Other", "Other");
  fprintf(pf, "<input type=\"checkbox\" id=\"Per.Service\" name=\"Per.Service\" checked onclick=\"SelectPeriph('Service');\" hidden/>\n");
  fprintf(pf, "\n<br>\n");
  
  fprintf(pf, "<select id=\"srvsel\" onchange=\"SrvVisible()\">\n");
  fprintf(pf, "  <option value=\"srv_show\">Service lines: Show</option>\n");
  fprintf(pf, "  <option value=\"srv_opt\">Service lines: Remap only</option>\n");
  fprintf(pf, "  <option value=\"srv_hide\">Service lines: Hide</option>\n");
  fprintf(pf, "</select>\n");
}

void html_write_table(FILE *pf, int idx){
  mcu_t *m = &(mcu[idx]);
  if(m->per == NULL)return;
  fprintf(pf, "<table name=\"%s.%s\" onmouseleave=\"table_onmouse(-1);\" hidden>\n", m->name, altname(idx));
  fprintf(pf, "  <thead>\n    <tr>\n");
  
  fprintf(pf, "      <th><div>Pin</div></th>\n");
  fprintf(pf, "      <th><div>Name</div></th>\n");
  for(int i=0; i<periphn; i++){
    fprintf(pf, "      <th><div>%s</div></th>\n", periph[i].name);
  }
  fprintf(pf, "      <th><div>Service</div></th>\n");
  fprintf(pf, "      <th><div>Other</div></th>\n");
  fprintf(pf, "      <th><div>Col</div></th>\n");
  fprintf(pf, "      <th class=\"tbl_pinname\"><div>PN</div></th>\n");
  fprintf(pf, "      <th><div>Comment</div></th>\n");
  fprintf(pf, "    </tr>\n  </thead>\n");
  fprintf(pf, "  <tbody>\n");
  int ln = 0;
  for(int i=0; i<m->pack->pinn; i++){
    if(m->per[i].name[0] == 0)continue;
    char *srv = match_periph(m->per[i].funcs, &per_srv);
    if(srv[0]==0){
      fprintf(pf, "    <tr onmousemove=\"table_onmouse(%i);\">\n", i);
    }else{
      fprintf(pf, "    <tr class=\"tbl_srv_%c\" onmousemove=\"table_onmouse(%i);\">\n", srv[0], i);
    }
    //Fixed fields: num, name
    fprintf(pf, "      <td><div>%s</div></td>\n", m->per[i].num);
    fprintf(pf, "      <td><div>%s</div></td>\n", m->per[i].name);
    //Variable fields: periph
    for(int j=0; j<periphn; j++){
      fprintf(pf, "      <td><div>%s</div></td>\n", match_periph(m->per[i].funcs, &periph[j]));
    }
    //Fixed field: SRV
    srv = match_periph(m->per[i].funcs, &per_srv);
    if(srv[0]!=0){
      srv+=2;
      for(char *ch = srv+2; ch[0]!=0; ch++){
        if(((ch[0] == 'F')||(ch[0]=='O'))&&(ch[1]=='.')){ch[0]=' '; ch[1]=' '; ch+=2;}
      }
    }
    fprintf(pf, "      <td><div>%s</div></td>\n", srv);
    //Fixed field: 'Other'
    fprintf(pf, "      <td><div>%s</div></td>\n", match_periph(m->per[i].funcs, NULL)); //Other
    //Fixed fields: Col, PN, Comment
    fprintf(pf, "      <td><input name=\"usersel\" type=\"color\" value=\"#ffffff\" onchange=\"SelectColor(%i);\"/></td>\n", ln); //Sel
    ln++;
    fprintf(pf, "      <td><input type=\"text\" value=\"\" size=\"2\"/></td>\n"); //PN
    fprintf(pf, "      <td><div><input type=\"text\" value=\"\"/></div></td>\n"); //Comment
  }
  fprintf(pf, "  </tbody>\n</table>\n");
}

void html_write_tables(FILE *pf){
  fprintf(pf, "<div id=\"col-1\">\n");
  fprintf(pf, "<div class=\"table_wrapper\">\n");
  for(int i=0; i<mcun; i++){
    html_write_table(pf, i);
  }
  fprintf(pf, "</div>\n");
  fprintf(pf, "</div>\n");
}

void html_write_canvas(FILE *pf, float size){
  fprintf(pf, "<div id=\"col-2\">\n");
  fprintf(pf, "  <canvas style=\"position:fixed; top:100px; left:70%%\" id=\"canvas\" width=\"%f\" height=\"%f\">Package</canvas>\n", size, size);
  fprintf(pf, "</div>\n");
}

void html_write_drawfuncs(FILE *pf, float size){
  fprintf(pf, "function package_draw(){\n"
              "  const funcs = [\n");
  for(int i=0; i<mcun; i++){
    fprintf(pf, "    [\"%s.%s\", package_draw_%s_%s],\n", mcu[i].name, altname(i), mcu[i].name, altname(i));
  }
  fprintf(pf, "  ];\n"
              "  const canvas = document.getElementById(\"canvas\");\n"
              "  if( !canvas.getContext )return;\n"
              "  const ctx = canvas.getContext(\"2d\");\n"
              "  let mcu = document.getElementById(\"mcuselected\").value \n"
              "  let tbl = document.getElementsByName(mcu);\n");
  fprintf(pf, "  let x=0, y=0, scale=%f;\n", size);
  fprintf(pf, "  var pkg_draw_prev = -1;\n"
              "  for(let i=0; i<funcs.length; i++){\n"
              "    if(funcs[i][0] == mcu){\n"
              "      if(pkg_draw_prev != i){\n"
              "        ctx.save();\n"
              "        ctx.fillStyle =\"#ffffff\";\n"
              "        ctx.fillRect(0,0,5000,5000);\n"
              "        ctx.restore();\n"
              "        pkg_draw_prev = i;\n"
              "      }\n"
              "      funcs[i][1](ctx, tbl[0].children[1].children, x, y, scale); \n"
              "      break;\n"
              "    }\n"
              "  }\n"
              "  //draw_mcu(ctx, tbl[0].children[1].children, x, y, scale); \n"
              "}\n"
              "var table_selected = -1;\n"
              "window.addEventListener(\"load\", package_draw);\n"
              "function table_onmouse(idx){\n"
              "  table_selected = idx;\n"
              "  package_draw();\n"
              "}\n"
              "\n");
  for(int i=0; i<mcun; i++){
    fprintf(pf, "function package_draw_%s_%s(ctx, tbl, x, y, scale){\n", mcu[i].name, altname(i));
    pack_html_export(mcu[i].pack, pf);
    fprintf(pf, "}\n");
  }
}

void html_write(FILE *pf){
  fprintf(pf, "<head>\n\n<style type=\"text/css\">\n");
  html_write_style(pf);
  fprintf(pf, "\n</style>\n\n");
  fprintf(pf, "\n<script>\n\n");
  html_write_scripts(pf);
  fprintf(pf, "\n</script>\n\n");
  fprintf(pf, "\n\n</head>\n\n<body onload=\"OnLoad();\">\n\n");
  //fprintf(pf, "<form style=\"position:fixed; top:10px; left:10px%%\">\n\n");
  fprintf(pf, "<form>\n\n");
  html_write_ui(pf);
  fprintf(pf, "\n\n</form>\n\n");
  html_write_tables(pf);
  html_write_canvas(pf, 500);
  fprintf(pf, "\n\n</body>\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////// filesystem  ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)\
    || defined(__APPLE__) || defined(__MACH__)
    
#include <dirent.h>
#include <sys/stat.h>
void dirfiles_read(char *dirname, char *extname, dirfiles_func_t callback, void *userdata){
  DIR *dir;
  struct dirent *entry;
  size_t rootlen = strlen(dirname);
  size_t extlen = strlen(extname);
  size_t leaflen;
  
  dir = opendir(dirname);
  if(!dir){
    leaflen = strlen(dirname);
    if((leaflen > extlen)&&(strcasecmp(&(dirname[leaflen-extlen]), extname) == 0)){
      callback(dirname, userdata);
      return;
    }else{
      struct stat st;
      stat(dirname, &st);
      if( (st.st_mode & S_IFMT) == S_IFDIR )fprintf(stderr, "Unable to open [%s]\n", dirname);
      return;
    }
  }
  
  while( (entry = readdir(dir)) != NULL){
    char *name = entry->d_name;
    if(name[0] == '.')continue;
    leaflen = strlen(name);
    char *newname = malloc(rootlen + leaflen + 2);
    memcpy(newname, dirname, rootlen);
    newname[rootlen] = '/';
    memcpy(&newname[rootlen+1], name, leaflen);
    newname[rootlen + 1 + leaflen] = 0;
    dirfiles_read(newname, extname, callback, userdata);
    free(newname);
  };
  closedir(dir);
}

#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)

#include <windows.h>
void dirfiles_read(char *dirname, char *extname, dirfiles_func_t callback, void *userdata){
  size_t rootlen = strlen(dirname);
  size_t extlen = strlen(extname);
  char *path = malloc(rootlen + 5);
  memcpy(path, dirname, rootlen);
  path[rootlen] = '/';
  path[rootlen+1] = '*';
  path[rootlen+2] = 0;
  WIN32_FIND_DATA ffd;
  HANDLE hFind = FindFirstFile(path, &ffd);
  do{
    if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
      if(ffd.cFileName[0] == '.')continue;
      size_t leaflen = strlen(ffd.cFileName);
      char *newname = malloc(rootlen + leaflen + 5);
      memcpy(newname, dirname, rootlen);
      newname[rootlen] = '/';
      memcpy(&newname[rootlen+1], ffd.cFileName, leaflen);
      newname[rootlen + 1 + leaflen] = 0;
      dirfiles_read(newname, extname, callback, userdata);
      free(newname);
    }else{
      size_t leaflen = strlen(ffd.cFileName);
      if(leaflen < extlen)continue;
      if(strcasecmp(&(ffd.cFileName[leaflen-extlen]), extname) != 0)continue;
      char *newname = malloc(rootlen + leaflen + 2);
      memcpy(newname, dirname, rootlen);
      newname[rootlen] = '/';
      memcpy(&newname[rootlen+1], ffd.cFileName, leaflen);
      newname[rootlen + 1 + leaflen] = 0;
      callback(newname, userdata);
      free(newname);
    }
  }while(FindNextFile(hFind, &ffd) != 0);

  FindClose(hFind);
  free(path);        
}

#else
  #error "Unsupported platform"
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////// main ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


void test(pack_t *p){
  FILE *pf = fopen("gr.html", "w");
  float size = 500;
  fprintf(pf, "<head> \n"
              "\n"
              "<style type=\"text/css\">\n"
              "table{\n"
             "    border-collapse: collapse;\n"
             "}\n"
             "td, th{\n"
             "    border: 1px solid black;\n"
             "    width:auto;\n"
             "    text-align: center;\n"
             "}\n"
             "th{\n"
             "    background: #b0e0e6;\n"
             "}\n"
              "</style>\n"
              "\n"
              "<script>\n"
              "function draw(){\n"
              "  const canvas = document.getElementById(\"canvas\");\n"
              "  if( !canvas.getContext )return;\n"
              "  const ctx = canvas.getContext(\"2d\");\n"
              "  let tbl = document.getElementsByName(\"Table_test\");\n");
  fprintf(pf, "  let x=0, y=0, scale=%f\n", size);
  fprintf(pf, "  draw_mcu(ctx, tbl[0].children[1].children, x, y, scale); \n"
              "}\n"
              "var table_selected = -1;\n"
              "\n"
              "function draw_mcu(ctx, tbl, x, y, scale){\n");
  
  pack_html_export(p, pf);
  
  fprintf(pf, "}\n"
              "window.addEventListener(\"load\", draw);\n"
              "\n"
              "function table_onmouse(idx){\n"
              "  table_selected = idx;\n"
              "  draw();\n"
              "}\n"
              "\n"
              "</script>\n"
              "</head>\n<body>\n"
              "<canvas id=\"canvas\" width=\"%f\" height=\"%f\"></canvas>\n", size, size);
              
  fprintf(pf, "<table name=\"Table_test\"  onmouseleave=\"table_onmouse(-1);\">\n "
              "  <thead>\n"
              "    <tr>\n"
              "      <th>Pin</th>\n"
              "      <th>Name</th>\n"
              "      <th>UART</th>\n"
              "      <th>SPI</th>\n"
              "      <th class=\"tbl_pinname\">Comm</th>\n"
              "    </tr>\n  </thead>\n  <tbody>\n"
              "    <tr onmousemove=\"table_onmouse(0);\">\n"
              "      <td>1</td>\n"
              "      <td>GND</td>\n"
              "      <td></td>\n"
              "      <td></td>\n"
              "      <td><input type=\"text\" onchange=\"table_onmouse(-1);\"/></td>\n"
              "    </tr>\n    <tr onmousemove=\"table_onmouse(1);\"  bgcolor=\"red\">\n"
              "      <td>2</td>\n"
              "      <td>PA1</td>\n"
              "      <td>Tx</td>\n"
              "      <td></td>\n"
              "      <td><input type=\"text\" onchange=\"table_onmouse(-1);\"/></td>\n"
              "    </tr>\n    <tr onmousemove=\"table_onmouse(2);\">\n"
              "      <td>3</td>\n"
              "      <td>PA2</td>\n"
              "      <td>Rx</td>\n"
              "      <td>SCK</td>\n"
              "      <td><input type=\"text\" onchange=\"table_onmouse(-1);\"/></td>\n"
              "    </tr>\n"
              "  </tbody>\n"
              "</table>\n");
  
  fprintf(pf, "</body>");
  fclose(pf);
}

int main1(int argc, char **argv){
  if(argc < 2){printf("select file name\n"); return 0;}
  pack_t *p;
  p = pack_load(argv[1]);
  test(p);
  pack_test(p);
  pack_free(p);
  return 0;
}

#define StrEq(str, templ) (strncmp(str, templ, sizeof(templ)-1)==0)
int main(int argc, char **argv){
  char *inputfile = NULL;
  char *outputfile = NULL;
  char alloc_out = 0;
  if(argc < 2){help(argv[0]); return 0;}
  for(int i=1; i<argc; i++){
    if(StrEq(argv[i], "--config=")){
      //configfile = argv[i] + sizeof("--config=") - 1;
    }else if(StrEq(argv[i], "--packages=")){
      packs_import(argv[i] + sizeof("--packages=") - 1);
    }else{
      if(inputfile == NULL){
        inputfile = argv[i];
      }else{
        outputfile = argv[i];
      }
    }
  }
  for(int i=0; i<sizeof(pack_defaultpath)/sizeof(pack_defaultpath[0]); i++){
    packs_import((char*)pack_defaultpath[i]);
  }
  packs_show();
  if(inputfile == NULL){
    help(argv[0]); return 0;
  }
  if(outputfile == NULL){outputfile = make_out_name(inputfile); alloc_out = 1;}
  
  FILE *pf = fopen(inputfile, "r");
  char buf[4096];
  while( fgets(buf, sizeof(buf), pf) != NULL ){
    linenum++;
    char *ch = buf;
    while( isspace(ch[0]) )ch++;
    if(ch[0] == '#')continue; //comment
    if(StrEq(ch, "Packages:")){packages_parse(buf + sizeof("Packages:")-1); continue;}
    if(StrEq(ch, "MCU:")){mcu_parse(buf + sizeof("MCU:")-1); continue;}
    if(StrEq(ch, "Periph:")){parse_periph(buf + sizeof("Periph:")-1); continue;}
    if(StrEq(ch, "Content:")){content_skip(buf + sizeof("Content:")-1, pf); continue;}
    //printf("Unknown: [%s]\n", buf);
  }
  
  mcu_resolv_deps();
  test_deps();
  
  if(fatalflag)goto destroy_all;
  
  rewind(pf);
  linenum = 0;
  while( fgets(buf, sizeof(buf), pf) != NULL ){
    linenum++;
    char *ch = buf;
    while( isspace(ch[0]) )ch++;
    if(StrEq(ch, "Content:")){content_parse(buf + sizeof("Content:")-1, pf); continue;}
    //printf("Unknown: [%s]\n", buf);
  }
  fclose(pf);
  
  //package_show();
  //mcu_show();
  test_mcu_complete();
  
  pf = fopen(outputfile, "w");
  html_write(pf);
  fclose(pf);
  
  
destroy_all:
  periph_free();
  mcu_free();
  //package_free();
  //packs_free();
  
  if(alloc_out){free(outputfile);}
  return 0;
}