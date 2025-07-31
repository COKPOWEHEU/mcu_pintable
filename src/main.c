#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pack_KiCad.h"

char **pack_path = NULL;
size_t pack_path_num = 0;

void pack_path_append(char *str){
#warning TODO
  pack_path = realloc(pack_path, sizeof(char*)*(pack_path_num + 1));
  pack_path[pack_path_num] = strdup(str);
  pack_path_num++;
}
void pack_path_free(){
  if(pack_path == NULL)return;
  for(int i=0; i<pack_path_num; i++)free(pack_path[i]);
  free(pack_path);
}


int linenum = 0;
int packs_maxpins = 0;
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
void pack_add(pack_t *p);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////// Parse input file ///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

pack_t *pack = NULL;
size_t packnum = 0;
size_t packalloc = 0;
const size_t packalloc_dn = 10;

typedef struct{
  char *name;
  char *path;
  int pack_idx;
}pack_list_t;
pack_list_t *pack_list = NULL;
size_t pack_list_num = 0;
size_t pack_list_alloc = 0;
const size_t pack_list_dn = 5;

typedef struct{
  char name[20];
  char num[10];
  char *funcs;
  char *comment;
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

int *mcu_baseperiph = NULL;

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

typedef struct{
  char num[10];
  char *data;
}comment_t;
comment_t *comment = NULL;
size_t comment_n = 0;
size_t comment_alloc = 0;
const size_t comment_allocdn = 5;

void packages_parse(char *buf){
  const char delim[] = "; \t\r\n";
  char *str = strtok(buf, delim);
  int res;
  
  while(str != NULL){
    char name[100], altname[100];
    res = sscanf(str, "%100[^[][%100[^]]]", altname, name);
    if(res < 1){fprintf(stderr, "Wrong format at %i\n", linenum); fatalflag=1; return;}
    
    if(pack_list_num+1 >= pack_list_alloc){
#warning TODO
      pack_list = realloc(pack_list, sizeof(pack_list_t)*(pack_list_alloc + pack_list_dn));
      pack_list_alloc += pack_list_dn;
    }
    pack_list[pack_list_num].name = strdup(altname);
    if(res == 2)pack_list[pack_list_num].path = strdup(name); else pack_list[pack_list_num].path = strdup(altname);
    pack_list[pack_list_num].pack_idx = -1;
    pack_list_num++;
    
    str = strtok(NULL, delim);
  }
}

int pack_search(char *name){
  if(pack == NULL)return -1;
  for(int i=0; i<packnum; i++){
    if(strcmp(name, pack[i].name)==0){return i;}
  }
  if(pack_list != NULL){
    for(int i=0; i<pack_list_num; i++){
      if(strcmp(name, pack_list[i].name)==0){
        return pack_list[i].pack_idx;
      }
    }
  }
  return -1;
}
void package_free(){
  //printf("package free\n");
  if(pack_list){
    for(int i=0; i<pack_list_num; i++){
      //printf("pack [%i] = [%s|%s]\n", i, pack_list[i].name, pack_list[i].path);
      free(pack_list[i].name);
      free(pack_list[i].path);
    }
    free(pack_list); pack_list_num = 0; pack_list_alloc = 0;
  }
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
        if(mcu[i].per[j].funcs)free(mcu[i].per[j].funcs);
        if(mcu[i].per[j].comment)free(mcu[i].per[j].comment);
      }
      free(mcu[i].per);
    }
    if(mcu[i].pack!=NULL){pack_free(mcu[i].pack); free(mcu[i].pack);}
  }
  free(mcu);
  mcu = NULL; mcun = 0; mcualloc = 0;
}

void mcu_resolv_deps(){
  if(mcu == NULL)return;
  for(int i=0; i<mcun; i++){
    if(mcu[i].packname == NULL)continue;
    int idx = pack_search(mcu[i].packname);
    
    if(idx < 0){
      fprintf(stderr, "MCU [%s]: package [%s] not found\n", mcu[i].name, mcu[i].packname);
      pack_t *p = pack_dummy(mcu[i].packname, packs_maxpins);
      mcu[i].pack = p;
    }else{
      mcu[i].pack = pack_dup(&(pack[idx]));
    }
    
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
      mcu[i].per[j].comment = NULL;
    }
  }
}

void mcu_show(){
  if(mcu == NULL)return;
  for(int i=0; i<mcun; i++){
    printf("%s . %s: %s\n", mcu[i].name, mcu[i].packname, mcu[i].pack->name);
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

void mcu_baseperiph_init(){
  if(mcu_baseperiph)free(mcu_baseperiph);
  mcu_baseperiph = malloc(sizeof(int) * mcun);
  for(int i=0; i<mcun; i++)mcu_baseperiph[i] = i;
  for(int i=1; i<mcun; i++){
    int eqnum;
    for(int j=0; j<i; j++){
      eqnum = -1;
      if(!pack_equal(mcu[i].pack, mcu[j].pack))continue;
      eqnum = j;
      for(int k=0; k<mcu[i].pack->pinn; k++){
        if(mcu[i].per[k].num[0] == 0)continue;
        if(mcu[j].per[k].num[0] == 0)continue;
        
        if(strcmp(mcu[i].per[k].funcs, mcu[j].per[k].funcs) != 0){eqnum = -1; break;}
      }
      if(eqnum>=0){eqnum = j; break;}
      
    }
    if((eqnum >= 0) && (eqnum != i)){
      mcu_baseperiph[i] = eqnum;
      //printf("dup [%s.%s] <- [%s.%s]\n", mcu[eqnum].name, mcu[eqnum].packname, mcu[i].name, mcu[i].packname);
    }
  }
}
void mcu_baseperiph_free(){
  if(mcu_baseperiph){free(mcu_baseperiph); mcu_baseperiph = NULL;}
}

int packdummy_add_pin(pack_t *p, char *s){
  int idx = -1;
  for(int i=0; i<p->pinn; i++){
    if(p->pin[i].name[0] == 0){
      idx = i;
      break;
    }
  }
  if(idx < 0)return -1;
  strncpy(p->pin[idx].name, s, 20);
  return idx;
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
  if(pack == NULL){fprintf(stderr, "'Packages' section not found\n"); fatalflag = 1; return;}
  if(mcu == NULL){fprintf(stderr, "'MCU' section not found\n");fatalflag = 1; return;}
  if(periph == NULL){fprintf(stderr, "'Periph' section not found\n");fatalflag = 1; return;}
  for(int i=0; i<mcun; i++){
    if(mcu[i].pack == NULL){
      fprintf(stderr, "MCU [%s]: package [%s] not found\n", mcu[i].name, mcu[i].packname);
      //fatalflag = 1;
      //return;
    }
  }
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

//Test if str is mcu[mcuidx]
char mcu_match(char *str, int mcuidx){
  char *prevstr = str;
  mcu_t *m = &(mcu[mcuidx]);
  //printf("MCU match [%s] <> [%s][%s|%s]", str, m->name, m->pack->name, m->packname);
  //test mcu.name
  if(str[0] != '['){
    char *en = strchr(str, '[');
    size_t len;
    if(en != NULL)len = en - str; else len = strlen(str)+1;
    if(strncmp(m->name, str, len)!=0){printf(""); return 0;}
    
    str += len;
  }
  //test mcu.pack.name
  if(m->pack == NULL)return 0;
  if(str[0] == '['){
    str++;
    char *en = strchr(str, ']');
    if(en == NULL){fprintf(stderr, "%i: Wrong MCU format [%s]\n", linenum, prevstr); return 0;}
    size_t len = en - str;
    if((strncmp(m->pack->name, str, len)!=0)&&(strncmp(m->packname, str, len)!=0)){printf(""); return 0;}
  }
  //printf("<<<< OK\n");
  return 1;
}

//Чтение заголовка таблицы 'Content'
//  buf - строка таблицы (считанная ранее из файла)
//  npacks(return) - количество корпусов, указанных в таблице
//  tbl(return) - таблица соответствия корпусов и MCU
void content_read_header(char *buf, int *npacks, char **tbl){
  const char delim[] = "| \t\n";
  const char delim2[]= "; \t";
  char *str = strtok(buf, delim);
  str = strtok(NULL, delim); //skip 'Name' field
  int mcols = 0; //MCU columns
  char **names;
  names = malloc(sizeof(char*) * mcun);
  if(names == NULL){fprintf(stderr, "Not enough memory\n"); *npacks=0; fatalflag = 1; return;}
  for(int i=0; i<mcun; i++)names[i] = NULL;
  
  //Read MCU names
  while(str != NULL){
    if(strcmp(str, "Periph")==0)break;
    names[mcols] = str;
    mcols++;
    str = strtok(NULL, delim);
  }
  
  if(mcols == 0){
    fprintf(stderr, "%i: 'MCU' list not found\n", linenum);
    fatalflag = 1;
    *npacks = 0; *tbl = NULL;
  }
  *npacks = mcols;
  char *res = malloc(sizeof(char) * mcols * mcun);
  *tbl = res;
  if(res == NULL){
    fprintf(stderr, "Not enough memory\n"); *npacks=0; fatalflag = 1; return;
  }
  for(int i=0; i<(mcols*mcun); i++)res[i] = 0;
  
  for(int i=0; i<mcols; i++){
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
  char *mcu_idx_tbl; //таблица какие MCU входят в данный Content и под какими номерами
  //read table header
  if(fgets(buf, sizeof(buf), pf)==NULL){
    fprintf(stderr, "%i: Unexpected end of file\n", linenum);
    return;
  }
  linenum++;
  content_read_header(buf, &npacks, &mcu_idx_tbl);
  
  //read table separator
  if(fgets(buf, sizeof(buf), pf)==NULL){
    fprintf(stderr, "%i: Unexpected end of file\n", linenum);
    if(mcu_idx_tbl)free(mcu_idx_tbl);
    return;
  }
  if(mcu_idx_tbl == NULL)return;
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
    char *comm = per + 1;
    per[0] = 0;
    while(per[0] != '|'){
      if(per<=buf){fprintf(stderr, "%i: Wrong table line", linenum); fatalflag = 1; free(mcu_idx_tbl); return;}
      per--;
    }
    per[0] = 0;
    per++;
    
    while(isspace(comm[0]))comm++;
    for(char *ch = comm + strlen(comm)-1; isspace(ch[0]) && ch>=comm; ch--)ch[0]=0;
    if((ch == comm)||(comm[0]==0))comm=NULL;
    
    //find 'Name' field
    const char delim[] = "|";
    const char delim2[] = "; \t";
    char *str = strtok(buf, delim);
    if(str == NULL){fprintf(stderr, "%i: Wrong table line", linenum); fatalflag = 1; free(mcu_idx_tbl); return;}
    char name[20];
    sscanf(str, " %20s", name);

    //'MCU' fields
    for(int i=0; i<npacks; i++){
      str = strtok(NULL, delim);
      if(str == NULL){fprintf(stderr, "%i: Wrong table line", linenum); fatalflag = 1; free(mcu_idx_tbl); return;}
      char *saveptr;
      char *pinname = strtok_r(str, delim2, &saveptr);
      while(pinname != NULL){
        if(pinname[0] == '-')break;
        int pin;
        size_t pos = i*mcun;
        for(int j=0; j<mcun; j++){
          if(mcu_idx_tbl[pos + j]){
            pin = str_to_pinnum(pinname, j);
            if((pin < 0) || (pin>=mcu[j].pack->pinn)){
              if((mcu[j].pack->descr!=NULL) && (strcmp(mcu[j].pack->descr, "Dummy")==0)){
                pin = packdummy_add_pin(mcu[j].pack, pinname);
                if(pin < 0)continue;
              }else{
                fprintf(stderr, "MCU [%s.%s]: pin [%s] not found in package\n", mcu[j].name, mcu[j].packname, pinname);
                continue;
              }
            }
            strncpy(mcu[j].per[pin].name, name, 20);
            strncpy(mcu[j].per[pin].num, pinname, 10);
            if(mcu[j].per[pin].funcs == NULL){
              mcu[j].per[pin].funcs = strdup(per);
            }else{
              mcu[j].per[pin].funcs = realloc(mcu[j].per[pin].funcs, strlen(mcu[j].per[pin].funcs)+strlen(per)+2);
              strcat(mcu[j].per[pin].funcs, per);
            }
            
            if(comm != NULL){
              if(mcu[j].per[pin].comment == NULL){
                mcu[j].per[pin].comment = strdup(comm);
              }else{
                mcu[j].per[pin].comment = realloc(mcu[j].per[pin].comment, strlen(mcu[j].per[pin].comment)+strlen(comm)+2);
                strcat(mcu[j].per[pin].comment, comm);
              }
            }
        
          }
        }
        pinname = strtok_r(NULL, delim2, &saveptr);
      }
    }
  }
  free(mcu_idx_tbl);
  linenum--;
  fseek(pf, pos, SEEK_SET);
}

void comment_parse(char *buf){
  if(comment_n + 1 >= comment_alloc){
    comment_t *prev = comment;
    comment = realloc(comment, sizeof(comment_t)*(comment_alloc + comment_allocdn));
#warning TODO
    comment_alloc += comment_allocdn;
    for(int i=comment_n; i<comment_alloc; i++){
      comment[i].num[0] = 0;
      comment[i].data = NULL;
    }
  }
  sscanf(buf, "%s %m[^\n]", comment[comment_n].num, &(comment[comment_n].data));
  
  comment_n++;
}
void comment_show(){
  if(comment == NULL)return;
  for(int i=0; i<comment_n; i++){
    printf("Comment %i: [%s] %s\n", i, comment[i].num, comment[i].data);
  }
}
void comment_free(){
  if(comment == NULL)return;
  for(int i=0; i<comment_n; i++){
    if(comment[i].data)free(comment[i].data);
  }
  free(comment); comment_n = 0; comment_alloc = 0;
}

void test_mcu_complete(){
  for(int i=0; i<mcun; i++){
    int pins = 0;
    int pinn = 0;
    if(mcu[i].pack == NULL){
      printf("[%s.%s]: Package not found\n", mcu[i].name, mcu[i].packname);
      continue;
    }
    for(int j=0; j<mcu[i].pack->pinn; j++){
      if(mcu[i].per && mcu[i].per[j].funcs)pins++;
      if(mcu[i].pack->pin[j].name[0] != 0)pinn++;
    }
    float p = pins;
    p = p*100 / pinn;
    printf("[%s.%s]:\t%i / %i = %.1f%%\n", mcu[i].name, mcu[i].packname, pins, pinn, p);
    if(pins != pinn){
      printf(" ");
      for(int j=0; j<mcu[i].pack->pinn; j++){
        if(mcu[i].pack->pin[j].name[0] == 0)continue;
        if(mcu[i].per && mcu[i].per[j].funcs)continue;
        printf(" %s", mcu[i].pack->pin[j].name);
        //mcu[i].pack->pin[j].name[0] = 0;
        mcu[i].pack->pin[j].vis = 0;
      }
      printf("\n");
    }
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
      if(per != &per_srv)if(pos == 0)if(test_periph(str, &per_srv)>=0)pos=-1;
    }
    if(pos >= 0){
      if(per == &per_srv)pos+=2;
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

void packs_free(){
  if(pack == NULL)return;
  for(int i=0; i<packnum; i++){pack_free(&(pack[i]));}
  free(pack);
  pack = NULL; packnum = 0; packalloc = 0;
}

void pack_add(pack_t *p){
  if(packnum+1 >= packalloc){
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
}

void pack_load_callback(char *filename, void *data){
  char *name = pack_search_name(filename);
  if(name[0] == 0)return;
  int found = -1;
  for(int i=0; i<pack_list_num; i++){
    if(strcmp(name, pack_list[i].path)==0){found = i; break;}
  }
  if(found<0){return;}
  for(int i=0; i<packnum; i++){
    if(strcmp(pack[i].name, name)==0){
      //printf("pack_import: dup[%i] = [%s]\n", i, filename);
      return;
    }
  }
  //printf("pack import [%s]\n", filename);
  pack_t *p = pack_load(filename);
  if(p == NULL){fprintf(stderr, "pack_inport: [%s] wrong file format\n", filename); return;}
  pack_add(p);
  pack_list[found].pack_idx = packnum-1;
  free(p);
}
void packs_load(){
  //printf("packs_load\n");
  for(int i=0; i<pack_path_num; i++){
    dirfiles_read(pack_path[i], ".kicad_mod", pack_load_callback, NULL);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////// HTML export ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void html_write_style(FILE *pf){
  fprintf(pf, 
    "  /*body {\n"
    "    overflow: hidden;\n"
    "  }*/\n"
    "  table{\n"
    "    border-collapse: collapse; /* Убираем двойные линии между ячейками */ \n"
    "  }\n"
    "  td, th{\n"
    "    border: 1px solid black; /* Параметры рамки */ \n"
    "    width:auto;\n"
    "    text-align: center;\n"
    "    text-shadow: 0 0 1px #ffffff;"
    "  }\n"
    "  th{\n"
    "    background: #b0e0e6; /* Цвет фона */ \n"
    "    position: sticky; top: 0; /*Зафиксировать заголовок*/\n"
    "  }\n"
    "  .noBorder, .noBorder th, .noBorder td {\n"
    "    border: none !important; /* В результате границы становятся абсолютно незаметными */ \n"
    "  }\n"
    "  .hltext{\n"
    "    padding: 1px;\n"
    "    border: 2px dotted #ffffff;\n"
    "    color: #000000;\n"
    "    background-color:#c0c0c080;\n"
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
    "  height: 80%%;\n"
    "}\n"
    "\n"
    "#col-2 {\n"
    "  position: relative;\n"
    "  width: 30%%;\n"
    "  float: right;\n"
    "  height: 80%%;\n"
    "}\n"
    ".table_wrapper{\n"
    "  display: block;\n"
    "  overflow-x: auto;\n"
    "  height: 100%%;\n"
    "  white-space: nowrap;\n"
    "}\n"
  );
}

void html_write_drawfuncs(FILE *pf, float size);
void html_write_scripts(FILE *pf){
  fprintf(pf, "var controls = document.getElementsByClassName(\"controls\");\n");
  fprintf(pf, "\n");
  fprintf(pf, "function SelectPeriph(name){\n"
              "  let mcu = document.getElementById(\"mcuselected\").value;\n"
              "  let tbl = document.getElementsByName(mcu);\n"
              "  let ctl = document.getElementsByName(name);\n"
              "  let hdr = tbl[0].children[0].children[0];\n"
              "  let idx = -1;\n"
              "  let vis = \"\";\n"
              "  for(let i=0; i<hdr.children.length; i++){\n"
              "    if(hdr.children[i].innerText == name){idx = i; break;}\n"
              "  }\n"
              "  if(idx < 0)return;\n"
              "  hid = ! ctl[0].checked;\n"
              "  hdr.children[idx].hidden = hid;\n"
              "  \n"
              "  hdr = tbl[0].children[1];\n"
              "  for(let i=0; i<hdr.children.length; i++){\n"
              "    if(hdr.children[i].children.length < 2)continue;\n"
              "    hdr.children[i].children[idx].hidden = hid;\n"
              "  }\n"
              "}\n\n");

  fprintf(pf, "function SrvVisible(){\n"
              "  let mcu = document.getElementById(\"mcuselected\").value;\n"
              "  let tbl = document.getElementsByName(mcu);\n"
              "  let ctl = document.getElementById(\"srvsel\").value;\n"
              "  let sh_F=true, sh_O=true;\n"
              "  let check = document.getElementsByName(\"Service\")[0];\n"
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
              "  let mcu = document.getElementById(\"mcuselected\").value;\n"
              "  let tbl = document.getElementsByName(mcu);\n"
              "  let lin = tbl[0].children[1];\n"
              "  let col = -1;\n"
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
              "  package_draw();\n"
              "}\n\n");
  
  fprintf(pf, "function PN_input(){\n"
              "  package_draw();\n"
              "}\n\n");
  
  fprintf(pf, "function SelectMCU(){\n"
              "  let mcu = document.getElementById(\"mcuselected\");\n"
              "  let tables = document.getElementsByTagName(\"table\");\n"
              "  for(let i=0; i<tables.length; i++){\n"
              "    tables[i].hidden = (tables[i].attributes.name.value != mcu.value);\n"
              "  }\n"
              "  Periph_Vis_update();\n"
              "  package_draw();\n"
              "  tbl_col_update();\n"
              "  document.title = mcu.children[mcu.selectedIndex].firstChild.data + \" - Pins\";\n"
              "}\n\n");
  
  fprintf(pf, "function Periph_Vis_update(){\n"
              "  SrvVisible();\n"
              "  for(let i=0; i<controls.length; i++){\n"
              "    SelectPeriph(controls[i].name);\n"
              "  }\n"
              "}\n\n");
  
  fprintf(pf, "function HideAll(){\n"
              "  let check = false;\n"
              "  for(let i=0; i<controls.length; i++){\n"
              "    check |= controls[i].checked;\n"
              "  }\n"
              "  for(let i=0; i<controls.length; i++){\n"
              "    controls[i].checked = !check;\n"
              "    SelectPeriph(controls[i].name);\n"
              "  }\n"
              "}\n\n");
  
  fprintf(pf, "function tbl_col_update(){\n"
              "  let mcu = document.getElementById(\"mcuselected\").value;\n"
              "  let tbl = document.getElementsByName(mcu);\n"
              "  let lin = tbl[0].children[1];\n"
              "  for(let j=0; j<tbl[0].children[1].children.length; j++){\n"
              "    let col = -1;\n"
              "    lin = tbl[0].children[1].children[j].getElementsByTagName(\"input\");\n"
              "    for(let i=0; i<lin.length; i++){\n"
              "      if(lin[i].name == \"usersel\"){\n"
              "        col = lin[i].value;\n"
              "        break;\n"
              "      }\n"
              "    }\n"
              "    if(col == -1)return;\n"
              "    lin = tbl[0].children[1].children[j];\n"
              "    if(col == \"#ffffff\")col=\"\";\n"
              "    lin.bgColor = col;\n"
              "  }\n"
              "  package_draw();\n"
              "}\n\n");
              
  fprintf(pf, "function OnLoad(){\n"
              "  SelectMCU();\n"
              "  Periph_Vis_update();\n"
              "  tbl_col_update();\n"
              "}\n\n");
  
  fprintf(pf, "var comment_event = false;\n"
              "function href_onclick(name){\n"
              "let det = document.getElementById(\"details_comments\");\n"
              "  comment_event = true;\n"
              "  det.setAttribute(\"open\",\"\");\n"
              "  for(let i=0; i<det.children.length; i++){\n"
              "    if(det.children[i].id != name){\n"
              "      det.children[i].style = \"\";\n"
              "    }else{\n"
              "      det.children[i].style = \"background-color:powderblue;\";\n"
              "    }\n"
              "  }\n"
              "}\n"
              "\n"
              "function comm_ontoggle(){\n"
              "  if(comment_event){comment_event=false; return;}\n"
              "  let det = document.getElementById(\"details_comments\");\n"
              "  for(let i=0; i<det.children.length; i++){\n"
              "    det.children[i].style = \"\";\n"
              "  }\n"
              "}\n\n");
  
  fprintf(pf, "function tbl_export(){\n"
              "  let mcu = document.getElementById(\"mcuselected\");\n"
              "  let tbl = document.getElementsByName(mcu.value);\n"
              "  \n"
              "  let lin = tbl[0].children[0].children[0].children;\n"
              "  let idx_col = -1, idx_pn = -1, idx_comm=-1;\n"
              "  for(let i=0; i<lin.length; i++){\n"
              "    if(lin[i].className == \"tbl_color\")idx_col = i;\n"
              "    if(lin[i].className == \"tbl_pinname\")idx_pn = i;\n"
              "    if(lin[i].className == \"tbl_comment\")idx_comm = i;\n"
              "  }\n"
              "  \n"
              "  let res = mcu.children[mcu.selectedIndex].firstChild.data + \"\\n\";\n"
              "  \n"
              "  let col = -1;\n"
              "  let cur = tbl[0].children[1].children;\n"
              "  res += cur.length + \"\\n\";\n"
              "  for(let i=0; i<cur.length; i++){\n"
              "    col = cur[i].children[idx_col].children[0].value;\n"
              "    let pn = cur[i].children[idx_pn].children[0].value;\n"
              "    let comm = cur[i].children[idx_comm].children[0].children[0].value;\n"
              "    res += i+\", \"+col+\", \"+pn+\", \"+comm+   \"\\n\";\n"
              "  }\n"
              "  \n"
              "  res += controls.length + \"\\n\"\n"
              "  \n"
              "  for(let i=0; i<controls.length; i++){\n"
              "    res += controls[i].name +\", \"+ controls[i].checked + \"\\n\";\n"
              "  }\n"
              "  \n"
              "  return res;\n"
              "}\n"
              "\n"
              "function tbl_import(str){\n"
              "  let arr = str.split(\"\\n\");\n"
              "  let name = arr[0];\n"
              "  \n"
              "  let mcu = document.getElementById(\"mcuselected\");\n"
              "  let idx = -1;\n"
              "  for(let i=0; i<mcu.children.length; i++){\n"
              "    if(mcu.children[i].firstChild.data == name){\n"
              "      mcu.selectedIndex = i;\n"
              "      idx = i;\n"
              "      break;\n"
              "    }\n"
              "  }\n"
              "  \n"
              "  if(idx < 0){\n"
              "    alert(\"MCU [\" + name + \"] not found\");\n"
              "    return;\n"
              "  }\n"
              "  let tbl = document.getElementsByName(mcu.value);\n"
              "  \n"
              "  let linenum = 1;\n"
              "  \n"
              "  let lin = tbl[0].children[0].children[0].children;\n"
              "  let idx_col = -1, idx_pn = -1, idx_comm=-1;\n"
              "  for(let i=0; i<lin.length; i++){\n"
              "    if(lin[i].className == \"tbl_color\")idx_col = i;\n"
              "    if(lin[i].className == \"tbl_pinname\")idx_pn = i;\n"
              "    if(lin[i].className == \"tbl_comment\")idx_comm = i;\n"
              "  }\n"
              "  \n"
              "  let len = parseInt(arr[linenum]);\n"
              "  linenum++;\n"
              "  \n"
              "  for(let i=linenum; i<(linenum+len); i++){\n"
              "    cur = arr[i].split(\", \");\n"
              "    if(cur == \"\"){len++; continue;}\n"
              "    let idx = cur[0];\n"
              "    lin = tbl[0].children[1].children[idx].children;\n"
              "    lin[idx_col].children[0].value = cur[1];\n"
              "    lin[idx_pn].children[0].value = cur[2];\n"
              "    lin[idx_comm].children[0].children[0].value = cur[3];\n"
              "  }\n"
              "  linenum += len;\n"
              "  \n"
              "  len = parseInt(arr[linenum]);\n"
              "  linenum++;\n"
              "  for(let i=linenum; i<(linenum+len); i++){\n"
              "    cur = arr[i].split(\", \");\n"
              "    if(cur == \"\"){len++; continue;}\n"
              "    let idx = controls.namedItem(cur[0]);\n"
              "    idx.checked = (cur[1] == \"true\");\n"
              "  }\n"
              "  linenum += len;\n"
              "  \n"
              "  SelectMCU();\n"
              "}\n"
              "\n"
              "function SaveToFile(){\n"
              "  let mcu = document.getElementById(\"mcuselected\");\n"
              "  let text = tbl_export();\n"
              "  let blob = new Blob([text], { type: 'text/plain' });\n"
              "  let link = document.createElement('a');\n"
              "  link.href = URL.createObjectURL(blob);\n"
              "  let filename = mcu.children[mcu.selectedIndex].firstChild.data + \".pincfg\";\n"
              "  link.download = filename;\n"
              "  link.click();\n"
              "}\n"
              "\n"
              "function LoadFromFile(){\n"
              "  let ui = document.getElementById(\"LoadFile\");\n"
              "  var file = ui.files[0];\n"
              "  if(!file){return;}\n"
              "  const reader = new FileReader();\n"
              "  function LoadFromFile_done(){\n"
              "    tbl_import(reader.result);\n"
              "    ui.value = \"\"\n"
              "  }\n"
              "  reader.onload = LoadFromFile_done;\n"
              "  reader.readAsText(file);\n"
              "}\n"
              "\n");
  
  html_write_drawfuncs(pf, 500);
}

void html_write_ui(FILE *pf){
  fprintf(pf, "<select id=\"mcuselected\" onchange=\"SelectMCU()\">\n");
  for(int i=0; i<mcun; i++){
    int idx = mcu_baseperiph[i];
    fprintf(pf, "  <option value=\"%s.%s\">%s.%s</option>\n", mcu[idx].name, mcu[idx].packname, mcu[i].name, mcu[i].packname);
  }
  fprintf(pf, "</select>\n\n<br>\n");
  for(int i=0; i<periphn; i++){
    fprintf(pf, "<label for=\"Per.%s\">%s</label><input type=\"checkbox\" class=\"controls\" id=\"Per.%s\" name=\"%s\" checked onclick=\"SelectPeriph('%s');\"/>\n", periph[i].name, periph[i].name, periph[i].name, periph[i].name, periph[i].name);
  }
  fprintf(pf, "<label for=\"Per.%s\">%s</label><input type=\"checkbox\" class=\"controls\" id=\"Per.%s\" name=\"%s\" checked onclick=\"SelectPeriph('%s');\"/>\n", "Other", "Other", "Other", "Other", "Other");
  fprintf(pf, "<input type=\"checkbox\" class=\"controls\" id=\"Per.Service\" name=\"Service\" checked onclick=\"SelectPeriph('Service');\" hidden/>\n");
  fprintf(pf, "\n<br>\n");
  
  fprintf(pf, "<select id=\"srvsel\" onchange=\"SrvVisible()\">\n"
              "  <option value=\"srv_show\">Service lines: Show</option>\n"
              "  <option value=\"srv_opt\">Service lines: Remap only</option>\n"
              "  <option value=\"srv_hide\">Service lines: Hide</option>\n"
              "</select>\n");
  
  fprintf(pf, "<input type=\"button\" value=\"Save\" onclick=\"SaveToFile()\"/>\n"
              "<input type=\"file\" id=\"LoadFile\" accept=\".pincfg\" onchange=\"LoadFromFile();\"/>\n"
              "\n");
  
  fprintf(pf, "<label for=\"pinnames\">Pin names</label><input type=\"checkbox\" id=\"pinnames\" name=\"pinnames\" onclick=\"package_draw();\"/>\n");
  
  fprintf(pf, "<input type=\"button\" value=\"HideAll\" onclick=\"HideAll();\"/>\n");

}

void html_table_comments(FILE *pf, mcu_t *m, int idx){
  fprintf(pf, "      <td><div>");
  size_t filepos = ftell(pf);
  fprintf(pf, "<abbr title=\"");
  char *str = m->per[idx].comment;
  char *prev = str;
  char name[10];
  char links[1000] = "<sup>";
  char *linkpos = links + sizeof("<sup>")-1;
  while(1){
    str = strstr(str, "COMM[");
    if(str == NULL)break;
    
    int found = 0;
    for(int i=0; i<(str-prev); i++){
      if( !isspace(prev[i]) && (prev[i]!=';') ){found = 1; break;}
    }
    if(found){
      fwrite(prev, 1, (str - prev), pf);
      fprintf(pf, " ");
    }
    
    prev = str + sizeof("COMM[")-1;
    str = strchr(prev, ']');
    if(str == NULL)break;
    
    sscanf(prev, "%[^]]", name);
    found = -1;
    for(int i=0; i<comment_n; i++){
      if(strcmp(name, comment[i].num)==0){found = i; break;}
    }
    if(found >= 0){
      sprintf(linkpos, "<a href=\"#comment_%s\" onclick=\"href_onclick('comment_%s');\">%s</a>, ", name, name, name);
      linkpos += strlen(linkpos);
    }
    
    prev = str = str+1;
  }
  if(linkpos > (links + sizeof("<sup"))){
    sprintf(linkpos-2, "</sup>");
  }else{
    links[0] = 0;
  }
  if((prev != str) && (prev[0]!=0)){
    fprintf(pf, "%s", prev);
  }
  
  size_t filepos2 = ftell(pf);
  if(filepos2 <= (filepos + sizeof("<abbr title=\""))){
    fseek(pf, filepos, SEEK_SET);
    fprintf(pf, "%s%s</div></td>\n", m->per[idx].name, links);
  }else{
    fprintf(pf, "\">%s%s</abbr></div></td>\n", m->per[idx].name, links);
  }
}

void html_write_table(FILE *pf, int idx){
  mcu_t *m = &(mcu[idx]);
  if(m->per == NULL)return;
  fprintf(pf, "<table name=\"%s.%s\" onmouseleave=\"table_onmouse(-1);\" hidden>\n", m->name, m->packname);
  fprintf(pf, "  <thead>\n    <tr>\n");
  
  fprintf(pf, "      <th><div>Pin</div></th>\n");
  fprintf(pf, "      <th><div>Name</div></th>\n");
  for(int i=0; i<periphn; i++){
    fprintf(pf, "      <th><div>%s</div></th>\n", periph[i].name);
  }
  fprintf(pf, "      <th><div>Service</div></th>\n");
  fprintf(pf, "      <th><div>Other</div></th>\n");
  fprintf(pf, "      <th class=\"tbl_color\"><div>Col</div></th>\n");
  fprintf(pf, "      <th class=\"tbl_pinname\"><div>PN</div></th>\n");
  fprintf(pf, "      <th class=\"tbl_comment\"><div>Comment</div></th>\n");
  fprintf(pf, "    </tr>\n  </thead>\n");
  fprintf(pf, "  <tbody>\n");
  int ln = 0;
  for(int i=0; i<m->pack->pinn; i++){
    if(m->per[i].name[0] == 0)continue;
    if(strstr(m->per[i].funcs, "SRV.O") != NULL){
      fprintf(pf, "    <tr class=\"tbl_srv_O\" onmousemove=\"table_onmouse(%i);\">\n", i);
    }else if(strstr(m->per[i].funcs, "SRV.F") != NULL){
      fprintf(pf, "    <tr class=\"tbl_srv_F\" onmousemove=\"table_onmouse(%i);\">\n", i);
    }else{
      fprintf(pf, "    <tr onmousemove=\"table_onmouse(%i);\">\n", i);
    }
    //Fixed fields: num, name
    fprintf(pf, "      <td><div>%s</div></td>\n", m->per[i].num);
    if(m->per[i].comment == NULL){
      fprintf(pf, "      <td><div>%s</div></td>\n", m->per[i].name);
    }else{
      html_table_comments(pf, m, i);
      //fprintf(pf, "      <td><div><abbr title=\"%s\">%s</abbr></div></td>\n", m->per[i].comment, m->per[i].name);
    }
    //Variable fields: periph
    for(int j=0; j<periphn; j++){
      fprintf(pf, "      <td><div>%s</div></td>\n", match_periph(m->per[i].funcs, &periph[j]));
    }
    //Fixed field: SRV
    char *srv = match_periph(m->per[i].funcs, &per_srv);
    fprintf(pf, "      <td><div>%s</div></td>\n", srv);
    //Fixed field: 'Other'
    fprintf(pf, "      <td><div>%s</div></td>\n", match_periph(m->per[i].funcs, NULL)); //Other
    //Fixed fields: Col, PN, Comment
    fprintf(pf, "      <td><input name=\"usersel\" type=\"color\" value=\"#ffffff\" onchange=\"SelectColor(%i);\"/></td>\n", ln); //Sel
    ln++;
    fprintf(pf, "      <td><input type=\"text\" value=\"\" size=\"2\" onchange=\"PN_input();\"/></td>\n"); //PN
    fprintf(pf, "      <td><div><input type=\"text\" value=\"\"/></div></td>\n"); //Comment
  }
  fprintf(pf, "  </tbody>\n</table>\n");
}

void html_write_tables(FILE *pf){
  fprintf(pf, "<div id=\"col-1\">\n");
  fprintf(pf, "<div class=\"table_wrapper\">\n");
  for(int i=0; i<mcun; i++){
    if(mcu_baseperiph[i] != i)continue;
    html_write_table(pf, i);
  }
  fprintf(pf, "</div>\n");
  fprintf(pf, "</div>\n");
}

void html_write_canvas(FILE *pf, float size){
  fprintf(pf, "<div id=\"col-2\">\n");
  fprintf(pf, "  <canvas id=\"canvas\" onmousemove=\"cnv_procdrag(event);\" onwheel=\"cnv_wheel(event);\">Package</canvas>\n");
  fprintf(pf, "</div>\n");
}

void html_write_drawfuncs(FILE *pf, float size){
  fprintf(pf, "var cnv_x=1, cnv_y=1, cnv_scale=1;\n"
              "var cnv_dx=0, cnv_dy=0;\n"
              "\n"
              "function cnv_procdrag(event){\n"
              "  if( event.buttons & 4){\n"
              "    cnv_x = 1; cnv_y = 1; cnv_scale = 1; package_draw();\n"
              "  }\n"
              "  if( event.buttons & 1){\n"
              "    cnv_x += event.clientX-cnv_dx;\n"
              "    cnv_y += event.clientY-cnv_dy;\n"
              "    package_draw();\n"
              "  }\n"
              "  cnv_dx = event.clientX;\n"
              "  cnv_dy = event.clientY;\n"
              "}\n"
              "\n"
              "function cnv_wheel(event){\n"
              "  event.preventDefault();\n"
              "  if(event.deltaY == 0)return;\n"
              "  const canvas = document.getElementById(\"canvas\");\n"
              "  let ds = 1;\n"
              "  if(event.deltaY > 0){\n"
              "    ds = 1/1.1;\n"
              "  }else{\n"
              "    ds = 1.1;\n"
              "  }\n"
              "  let X = event.clientX-canvas.parentNode.offsetLeft, Y = event.clientY-canvas.parentNode.offsetTop;\n"
              "  cnv_x = cnv_x*ds + X*(1-ds);\n"
              "  cnv_y = cnv_y*ds + Y*(1-ds);\n"
              "  cnv_scale *= ds;\n"
              "  \n"
              "  package_draw();\n"
              "}\n");
  
  fprintf(pf, "function package_draw(){\n"
              "  const funcs = [\n");
  for(int i=0; i<mcun; i++){
    fprintf(pf, "    [\"%s.%s\", package_draw_%s_%s],\n", mcu[i].name, mcu[i].packname, mcu[i].name, mcu[i].packname);
  }
  fprintf(pf, "  ];\n"
              "  const canvas = document.getElementById(\"canvas\");\n"
              "  if( !canvas.getContext )return;\n"
              "  const ctx = canvas.getContext(\"2d\");\n"
              "  let mcu = document.getElementById(\"mcuselected\").value \n"
              "  let tbl = document.getElementsByName(mcu);\n"
              "  let drawnames = document.getElementById(\"pinnames\").checked;\n"
              "  let x=cnv_x, y=cnv_y, scale=500;\n"
              "\n"
              "  let w = canvas.parentNode.clientWidth;\n"
              "  let h = canvas.parentNode.clientHeight;\n"
              "  if(w < h)scale = w-2; else scale = h-2;\n"
              "  canvas.width = w;\n"
              "  canvas.height = h;\n"
              "  scale *= cnv_scale;\n"
              "\n"
              "  var pkg_draw_prev = -1;\n"
              "  for(let i=0; i<funcs.length; i++){\n"
              "    if(funcs[i][0] == mcu){\n"
              "      if(pkg_draw_prev != i){\n"
              "        ctx.save();\n"
              "        ctx.fillStyle =\"#ffffff\";\n"
              "        ctx.fillRect(0,0,5000,5000);\n"
              "        ctx.restore();\n"
              "        pkg_draw_prev = i;\n"
              "      }\n"
              "      funcs[i][1](ctx, tbl[0].children[1].children, x, y, scale, drawnames); \n"
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
  pack_html_common(pf);
  for(int i=0; i<mcun; i++){
    fprintf(pf, "function package_draw_%s_%s(ctx, tbl, x, y, scale, drawnames){\n", mcu[i].name, mcu[i].packname);
    pack_html_export(mcu[i].pack, pf);
    fprintf(pf, "}\n");
  }
}

void html_write_comments(FILE *pf){
  if(comment == NULL)return;
  fprintf(pf, "<br>\n");
  fprintf(pf, "<details id=\"details_comments\" ontoggle=\"comm_ontoggle();\"><summary>Примечания</summary>\n");
  for(int i=0; i<comment_n; i++){
    fprintf(pf, "<p id=\"comment_%s\"><sup>%s</sup>). %s</p>\n", comment[i].num, comment[i].num, comment[i].data);
  }
  fprintf(pf, "</details>\n");
  fprintf(pf, "\n");
}

void html_write(FILE *pf){
  fprintf(pf, "<head>\n");
  fprintf(pf, "<title>%s</title>\n", "Pintable");
  fprintf(pf, "\n<style type=\"text/css\">\n");
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
  html_write_comments(pf);
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
      pack_path_append(argv[i] + sizeof("--packages=") - 1);
    }else{
      if(inputfile == NULL){
        inputfile = argv[i];
      }else{
        outputfile = argv[i];
      }
    }
  }
  pack_path_append("./");

  if(inputfile == NULL){
    help(argv[0]); return 0;
  }
  if(outputfile == NULL){outputfile = make_out_name(inputfile); alloc_out = 1;}
  
  FILE *pf = fopen(inputfile, "r");
  if(pf == NULL){fprintf(stderr, "Can not open input file [%s]\n", inputfile); goto destroy_all;}
  char buf[4096];
  while( fgets(buf, sizeof(buf), pf) != NULL ){
    linenum++;
    char *ch = buf;
    while( isspace(ch[0]) )ch++;
    if(ch[0] == '#')continue; //comment
    if(StrEq(ch, "Packages:")){packages_parse(buf + sizeof("Packages:")-1); continue;}
    if(StrEq(ch, "MCU:")){mcu_parse(buf + sizeof("MCU:")-1); continue;}
    if(StrEq(ch, "Periph:")){parse_periph(buf + sizeof("Periph:")-1); continue;}
    if(StrEq(ch, "Comment")){comment_parse(ch + sizeof("Comment")-1); continue;}
    if(StrEq(ch, "Content:")){
      int ln = linenum;
      content_skip(buf + sizeof("Content:")-1, pf);
      ln = linenum - ln - 2;
      if(packs_maxpins < ln)packs_maxpins = ln;
      continue;
    }
    //printf("Unknown: [%s]\n", buf);
  }
  
  packs_load();
  mcu_resolv_deps();
  test_deps();
  
  if(fatalflag){fclose(pf); fprintf(stderr, "Fatal error\n"); goto destroy_all;}
  
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
  mcu_baseperiph_init();
  test_mcu_complete();
  
  pf = fopen(outputfile, "w");
  html_write(pf);
  fclose(pf);
  
  
destroy_all:
  periph_free();
  mcu_free();
  package_free();
  packs_free();
  pack_path_free();
  mcu_baseperiph_free();
  comment_free();
  
  if(alloc_out){free(outputfile);}
  return 0;
}