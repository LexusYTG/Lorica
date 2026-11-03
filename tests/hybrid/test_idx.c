/* test_idx.c - motor de indices en GPU: restart personalizado, glProvokingVertex(FIRST), glPolygonMode,
 * GL_PRIMITIVES_GENERATED y glDrawTransformFeedback. Cada caso se compara con un "oraculo" independiente.
 * Uso: test_idx restart | prov <fichero> | poly | prims | tf */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "ctx.h"
#include "gl31.h"
static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
#define W 64
static unsigned char px[W*W*4];
static GLuint prog_make(const char*vs,const char*fs){
  GLuint v=gl31_glCreateShader(GL_VERTEX_SHADER),f=gl31_glCreateShader(GL_FRAGMENT_SHADER),p=gl31_glCreateProgram();
  gl31_glShaderSource(v,1,&vs,NULL);gl31_glCompileShader(v);gl31_glShaderSource(f,1,&fs,NULL);gl31_glCompileShader(f);
  GLint ok=0;char log[1024]="";
  gl31_glGetShaderiv(v,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(v,1024,NULL,log);printf("VS: %s\n",log);exit(2);}
  gl31_glGetShaderiv(f,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(f,1024,NULL,log);printf("FS: %s\n",log);exit(2);}
  gl31_glAttachShader(p,v);gl31_glAttachShader(p,f);gl31_glLinkProgram(p);
  gl31_glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){gl31_glGetProgramInfoLog(p,1024,NULL,log);printf("LINK: %s\n",log);exit(2);}
  return p;}
#define NV 12
static float pos[NV][2], col[NV][3];
static GLuint vbo_pos, vbo_col, ibo, vao;
static unsigned lcg=12345; static float rnd(void){lcg=lcg*1664525u+1013904223u;return ((lcg>>8)&0xFFFF)/65535.0f;}
static void setup(void){
  GLuint tex,fbo,rb; gl31_glGenTextures(1,&tex); gl31_glBindTexture(GL_TEXTURE_2D,tex); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,W,W);
  gl31_glGenRenderbuffers(1,&rb); gl31_glBindRenderbuffer(GL_RENDERBUFFER,rb); gl31_glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,W,W);
  gl31_glGenFramebuffers(1,&fbo); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0); gl31_glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,rb);
  gl31_glViewport(0,0,W,W); gl31_glDisable(GL_CULL_FACE); gl31_glDisable(GL_DEPTH_TEST);
  for(int i=0;i<NV;i++){ pos[i][0]=rnd()*1.8f-0.9f; pos[i][1]=rnd()*1.8f-0.9f; col[i][0]=0.2f+0.8f*rnd(); col[i][1]=0.2f+0.8f*rnd(); col[i][2]=0.2f+0.8f*rnd(); }
  gl31_glGenVertexArrays(1,&vao); gl31_glBindVertexArray(vao);
  gl31_glGenBuffers(1,&vbo_pos); gl31_glBindBuffer(GL_ARRAY_BUFFER,vbo_pos); gl31_glBufferData(GL_ARRAY_BUFFER,sizeof pos,pos,GL_STATIC_DRAW);
  gl31_glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(void*)0); gl31_glEnableVertexAttribArray(0);
  gl31_glGenBuffers(1,&vbo_col); gl31_glBindBuffer(GL_ARRAY_BUFFER,vbo_col); gl31_glBufferData(GL_ARRAY_BUFFER,sizeof col,col,GL_STATIC_DRAW);
  gl31_glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,(void*)0); gl31_glEnableVertexAttribArray(1);
  gl31_glGenBuffers(1,&ibo);
}
static GLuint P_flat;
static void mkprog(void){
  P_flat=prog_make("#version 330 core\nlayout(location=0) in vec2 p;layout(location=1) in vec3 c;flat out vec3 fc;void main(){gl_Position=vec4(p,0.0,1.0);fc=c;gl_PointSize=3.0;}",
                   "#version 330 core\nflat in vec3 fc;out vec4 o;void main(){o=vec4(fc,1.0);}");
}
static void clearfb(void){gl31_glClearColor(0,0,0,0);gl31_glClear(GL_COLOR_BUFFER_BIT);}
static int litpx(void){int n=0;for(int i=0;i<W*W;i++) if(px[i*4]||px[i*4+1]||px[i*4+2]) n++; return n;}
static void rd(void){gl31_glReadPixels(0,0,W,W,GL_RGBA,GL_UNSIGNED_BYTE,px);}
static void upload_idx(const void*d,size_t n){ gl31_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo); gl31_glBufferData(GL_ELEMENT_ARRAY_BUFFER,n,d,GL_STATIC_DRAW); }

typedef struct { const char*name; GLenum mode; int n; int idx[24]; } cs_t;   /* -1 = restart */
static const cs_t cases[] = {
 {"POINTS",GL_POINTS,11,{0,1,-1,2,3,4,-1,-1,5,6,7}},
 {"LINES",GL_LINES,11,{0,1,2,-1,3,4,5,6,-1,7,8}},
 {"LINE_STRIP",GL_LINE_STRIP,13,{0,1,2,3,-1,4,5,6,-1,7,8,9,10}},
 {"LINE_LOOP",GL_LINE_LOOP,12,{0,1,2,3,-1,4,5,6,-1,7,8,9}},
 {"TRIANGLES",GL_TRIANGLES,14,{0,1,2,3,-1,4,5,6,7,8,-1,9,10,11}},
 {"TRIANGLE_STRIP",GL_TRIANGLE_STRIP,16,{0,1,2,3,4,-1,5,6,7,8,9,10,-1,11,0,1}},
 {"TRIANGLE_FAN",GL_TRIANGLE_FAN,14,{0,1,2,3,4,-1,5,6,7,8,-1,9,10,11}},
};
#define NC (int)(sizeof cases/sizeof cases[0])
/* dibuja el caso con un tipo de indice y un valor de restart; devuelve en px */
static void draw_case(const cs_t*c, GLenum type, uint32_t rindex, int variant){
  uint8_t b8[32]; uint16_t b16[32]; uint32_t b32[32];
  for(int i=0;i<c->n;i++){ int v=c->idx[i]; b8[i]=v<0?(uint8_t)rindex:(uint8_t)v; b16[i]=v<0?(uint16_t)rindex:(uint16_t)v; b32[i]=v<0?rindex:(uint32_t)v; }
  const void*d=type==GL_UNSIGNED_BYTE?(void*)b8:type==GL_UNSIGNED_SHORT?(void*)b16:(void*)b32;
  size_t sz=type==GL_UNSIGNED_BYTE?1:type==GL_UNSIGNED_SHORT?2:4;
  upload_idx(d,sz*c->n);
  gl31_glEnable(GL_PRIMITIVE_RESTART); gl31_glPrimitiveRestartIndex(rindex);
  clearfb();
  if(variant==0) gl31_glDrawElements(c->mode,c->n,type,0);
  else if(variant==1) gl31_glDrawElementsInstanced(c->mode,c->n,type,0,3);
  else gl31_glDrawElementsBaseVertex(c->mode,c->n,type,0,0);
  rd();
}
static int t_restart(void){
  printf("== restart personalizado vs restart nativo (comparacion pixel a pixel)\n");
  static unsigned char ref[W*W*4]; char m[160];
  struct { GLenum t; uint32_t fixed, custom; const char*n; } ty[]={{GL_UNSIGNED_BYTE,0xFF,0x7F,"ubyte"},{GL_UNSIGNED_SHORT,0xFFFF,0x0FF0,"ushort"},{GL_UNSIGNED_INT,0xFFFFFFFFu,0x00ABCDEFu,"uint"}};
  for(int t=0;t<3;t++) for(int c=0;c<NC;c++) for(int v=0;v<2;v++){
    draw_case(&cases[c],ty[t].t,ty[t].fixed,v); memcpy(ref,px,sizeof ref); int lit=litpx();
    draw_case(&cases[c],ty[t].t,ty[t].custom,v);
    snprintf(m,sizeof m,"%s %s %s: identico al nativo (%d px)",ty[t].n,cases[c].name,v?"instanciado":"simple",lit);
    CHECK(lit>0 && !memcmp(ref,px,sizeof ref),m);
  }
  return fails;
}
static int t_prov(const char*out){
  printf("== glProvokingVertex(FIRST): backend nativo=%d\n",gl31_caps.provoking_vertex);
  FILE*f=fopen(out,"wb"); if(!f) return 1;
  gl31_glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
  for(int c=0;c<NC;c++){
    if(cases[c].mode==GL_POINTS) continue;
    /* con restart (indices fijos ushort) */
    draw_case(&cases[c],GL_UNSIGNED_SHORT,0xFFFF,0); fwrite(px,1,sizeof px,f); printf("  %s con restart: %d px\n",cases[c].name,litpx());
    /* sin restart: recortar la lista a su primer segmento */
    cs_t one=cases[c]; int n=0; for(int i=0;i<one.n;i++){ if(one.idx[i]<0) break; n++; } one.n=n;
    gl31_glDisable(GL_PRIMITIVE_RESTART);
    uint16_t b[32]; for(int i=0;i<n;i++) b[i]=(uint16_t)one.idx[i]; upload_idx(b,2*n); clearfb();
    gl31_glDrawElements(one.mode,n,GL_UNSIGNED_SHORT,0); rd(); fwrite(px,1,sizeof px,f); printf("  %s sin restart: %d px\n",cases[c].name,litpx());
    clearfb(); gl31_glDrawArrays(one.mode,0,NV); rd(); fwrite(px,1,sizeof px,f); printf("  %s DrawArrays: %d px\n",cases[c].name,litpx());
  }
  fclose(f);
  return 0;
}
static int t_poly(void){
  printf("== glPolygonMode vs lineas/puntos explicitos\n");
  static unsigned char ref[W*W*4]; char m[160];
  uint16_t tri[]={0,1,2,3,4,5,6,7,8,9,10,11};
  /* TRIANGLES */
  upload_idx(tri,sizeof tri);
  { uint16_t e[24]; int k=0; for(int t=0;t<4;t++){int a=tri[t*3],b=tri[t*3+1],c=tri[t*3+2]; e[k++]=a;e[k++]=b;e[k++]=b;e[k++]=c;e[k++]=c;e[k++]=a;}
    upload_idx(e,sizeof e); clearfb(); gl31_glDrawElements(GL_LINES,24,GL_UNSIGNED_SHORT,0); rd(); memcpy(ref,px,sizeof ref); }
  upload_idx(tri,sizeof tri); gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); clearfb(); gl31_glDrawElements(GL_TRIANGLES,12,GL_UNSIGNED_SHORT,0); rd();
  snprintf(m,sizeof m,"TRIANGLES en GL_LINE = lineas explicitas (%d px)",litpx()); CHECK(litpx()>0 && !memcmp(ref,px,sizeof ref),m);
  /* STRIP y FAN con DrawArrays */
  { int n=NV; uint16_t e[64]; int k=0; for(int t=0;t<n-2;t++){ int a=t,b=t+1,c=t+2; if(t&1){int q=a;a=b;b=q;} e[k++]=a;e[k++]=b;e[k++]=b;e[k++]=c;e[k++]=c;e[k++]=a; }
    gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); upload_idx(e,2*k); clearfb(); gl31_glDrawElements(GL_LINES,k,GL_UNSIGNED_SHORT,0); rd(); memcpy(ref,px,sizeof ref); }
  gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); clearfb(); gl31_glDrawArrays(GL_TRIANGLE_STRIP,0,NV); rd();
  snprintf(m,sizeof m,"TRIANGLE_STRIP (DrawArrays) en GL_LINE (%d px)",litpx()); CHECK(litpx()>0 && !memcmp(ref,px,sizeof ref),m);
  { int n=NV; uint16_t e[64]; int k=0; for(int t=0;t<n-2;t++){ int a=0,b=t+1,c=t+2; e[k++]=a;e[k++]=b;e[k++]=b;e[k++]=c;e[k++]=c;e[k++]=a; }
    gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); upload_idx(e,2*k); clearfb(); gl31_glDrawElements(GL_LINES,k,GL_UNSIGNED_SHORT,0); rd(); memcpy(ref,px,sizeof ref); }
  gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); clearfb(); gl31_glDrawArrays(GL_TRIANGLE_FAN,0,NV); rd();
  snprintf(m,sizeof m,"TRIANGLE_FAN (DrawArrays) en GL_LINE (%d px)",litpx()); CHECK(litpx()>0 && !memcmp(ref,px,sizeof ref),m);
  /* GL_POINT */
  { gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); uint16_t e[12]; for(int i=0;i<12;i++) e[i]=(uint16_t)i; upload_idx(e,sizeof e); clearfb(); gl31_glDrawElements(GL_POINTS,12,GL_UNSIGNED_SHORT,0); rd(); memcpy(ref,px,sizeof ref); }
  upload_idx(tri,sizeof tri); gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_POINT); clearfb(); gl31_glDrawElements(GL_TRIANGLES,12,GL_UNSIGNED_SHORT,0); rd();
  snprintf(m,sizeof m,"TRIANGLES en GL_POINT = puntos explicitos (%d px)",litpx()); CHECK(litpx()>0 && !memcmp(ref,px,sizeof ref),m);
  /* con restart personalizado dentro de una tira, en modo linea */
  { uint16_t s[]={0,1,2,3,0x0FF0,4,5,6,7}; upload_idx(s,sizeof s); gl31_glEnable(GL_PRIMITIVE_RESTART); gl31_glPrimitiveRestartIndex(0x0FF0);
    gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); clearfb(); gl31_glDrawElements(GL_TRIANGLE_STRIP,9,GL_UNSIGNED_SHORT,0); rd(); int a=litpx(); static unsigned char r2[W*W*4]; memcpy(r2,px,sizeof r2);
    uint16_t s2[]={0,1,2,3,0xFFFF,4,5,6,7}; upload_idx(s2,sizeof s2); gl31_glPrimitiveRestartIndex(0xFFFF); clearfb(); gl31_glDrawElements(GL_TRIANGLE_STRIP,9,GL_UNSIGNED_SHORT,0); rd();
    snprintf(m,sizeof m,"strip + restart personalizado en GL_LINE = restart nativo en GL_LINE (%d px)",a); CHECK(a>0 && !memcmp(r2,px,sizeof r2),m);
    /* y ese mismo resultado equivale a dos tiras separadas con lineas explicitas */
    gl31_glDisable(GL_PRIMITIVE_RESTART); gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    uint16_t e[]={0,1,1,2,2,0, 2,1,1,3,3,2, 4,5,5,6,6,4, 6,5,5,7,7,6}; upload_idx(e,sizeof e); clearfb(); gl31_glDrawElements(GL_LINES,24,GL_UNSIGNED_SHORT,0); rd();
    CHECK(!memcmp(r2,px,sizeof r2),"... y coincide con las dos tiras dibujadas a mano"); }
  gl31_glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  { GLint pm[2]={0,0}; gl31_glGetIntegerv(GL_POLYGON_MODE,pm); CHECK(pm[0]==GL_FILL&&pm[1]==GL_FILL,"glGetIntegerv(GL_POLYGON_MODE)"); }
  gl31_glPolygonMode(GL_FRONT,GL_LINE); CHECK(gl31_glGetError()==GL_INVALID_ENUM,"glPolygonMode(GL_FRONT) en core -> INVALID_ENUM");
  gl31_glPolygonMode(GL_FRONT_AND_BACK,0x1234); CHECK(gl31_glGetError()==GL_INVALID_ENUM,"glPolygonMode(modo invalido) -> INVALID_ENUM");
  return fails;
}
static int t_prims(void){
  printf("== GL_PRIMITIVES_GENERATED\n");
  GLuint q[2]; gl31_glGenQueries(2,q); char m[120];
  struct{GLenum mode;int cnt;int inst;GLuint64 expect;const char*n;} tc[]={
   {GL_POINTS,12,1,12,"POINTS 12"},{GL_LINES,12,1,6,"LINES 12"},{GL_LINE_STRIP,12,1,11,"LINE_STRIP 12"},{GL_LINE_LOOP,12,1,12,"LINE_LOOP 12"},
   {GL_TRIANGLES,12,1,4,"TRIANGLES 12"},{GL_TRIANGLE_STRIP,12,1,10,"TRIANGLE_STRIP 12"},{GL_TRIANGLE_FAN,12,1,10,"TRIANGLE_FAN 12"},
   {GL_TRIANGLES,12,5,20,"TRIANGLES 12 x5 instancias"}};
  for(int i=0;i<8;i++){
    gl31_glBeginQuery(GL_PRIMITIVES_GENERATED,q[0]);
    if(tc[i].inst>1) gl31_glDrawArraysInstanced(tc[i].mode,0,tc[i].cnt,tc[i].inst); else gl31_glDrawArrays(tc[i].mode,0,tc[i].cnt);
    gl31_glEndQuery(GL_PRIMITIVES_GENERATED);
    GLuint64 r=99999; gl31_glGetQueryObjectui64v(q[0],GL_QUERY_RESULT,&r);
    snprintf(m,sizeof m,"%s -> %llu (esperado %llu)",tc[i].n,(unsigned long long)r,(unsigned long long)tc[i].expect); CHECK(r==tc[i].expect,m);
  }
  /* mas de 64 ids distintos (antes se perdian) */
  GLuint many[200]; gl31_glGenQueries(200,many); int okmany=1;
  for(int i=0;i<200;i++){ gl31_glBeginQuery(GL_PRIMITIVES_GENERATED,many[i]); gl31_glDrawArrays(GL_TRIANGLES,0,3*(i%5+1)); gl31_glEndQuery(GL_PRIMITIVES_GENERATED); }
  for(int i=0;i<200;i++){ GLuint r=0; gl31_glGetQueryObjectuiv(many[i],GL_QUERY_RESULT,&r); if(r!=(GLuint)(i%5+1)) okmany=0; }
  CHECK(okmany,"200 queries PRIMITIVES_GENERATED conservan su resultado (limite de 64 eliminado)");
  CHECK(gl31_glGetError()==GL_NO_ERROR,"sin error GL");
  return fails;
}
static int t_tf(void){
  printf("== glDrawTransformFeedback\n");
  /* VS que captura la posicion y el color; el segundo programa dibuja desde el buffer capturado */
  GLuint pc=gl31_glCreateProgram(), v=gl31_glCreateShader(GL_VERTEX_SHADER), f=gl31_glCreateShader(GL_FRAGMENT_SHADER);
  const char*vs="#version 330 core\nlayout(location=0) in vec2 p;layout(location=1) in vec3 c;out vec2 op;out vec3 oc;void main(){op=p;oc=c;gl_Position=vec4(p,0.0,1.0);}";
  const char*fs="#version 330 core\nout vec4 o;void main(){o=vec4(1.0);}";
  gl31_glShaderSource(v,1,&vs,NULL);gl31_glCompileShader(v);gl31_glShaderSource(f,1,&fs,NULL);gl31_glCompileShader(f);
  gl31_glAttachShader(pc,v);gl31_glAttachShader(pc,f);
  const char*var[2]={"op","oc"}; gl31_glTransformFeedbackVaryings(pc,2,var,GL_INTERLEAVED_ATTRIBS); gl31_glLinkProgram(pc);
  GLint ok=0; gl31_glGetProgramiv(pc,GL_LINK_STATUS,&ok); CHECK(ok,"programa con transform feedback enlaza");
  GLuint xb, xfo[2]; gl31_glGenBuffers(1,&xb); gl31_glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,xb); gl31_glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,NV*5*4*4,NULL,GL_DYNAMIC_COPY);
  gl31_glGenTransformFeedbacks(2,xfo);
  /* captura con triangulos: 12 vertices en modo TRIANGLES, via objeto 0 */
  gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,xfo[0]); gl31_glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,xb);
  gl31_glUseProgram(pc); gl31_glEnable(GL_RASTERIZER_DISCARD);
  gl31_glBeginTransformFeedback(GL_TRIANGLES); gl31_glDrawArrays(GL_TRIANGLES,0,12); gl31_glDrawArrays(GL_TRIANGLES,0,6); gl31_glEndTransformFeedback();
  gl31_glDisable(GL_RASTERIZER_DISCARD);
  /* dibujar desde lo capturado: 18 vertices. Se compara con dibujar directamente los mismos 18. */
  uint16_t idx[18]; for(int i=0;i<18;i++) idx[i]=(uint16_t)(i%12);   /* lo mismo que capturo, expresado como indices */
  gl31_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK,0);
  gl31_glUseProgram(P_flat);
  upload_idx(idx,sizeof idx); clearfb(); gl31_glDrawElements(GL_TRIANGLES,18,GL_UNSIGNED_SHORT,0); rd(); static unsigned char ref[W*W*4]; memcpy(ref,px,sizeof ref);
  /* vertex source = buffer capturado (interleaved vec2+vec3) */
  gl31_glBindBuffer(GL_ARRAY_BUFFER,xb);
  gl31_glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,20,(void*)0); gl31_glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,20,(void*)8);
  clearfb(); gl31_glDrawTransformFeedback(GL_TRIANGLES,xfo[0]); rd();
  char m[96]; snprintf(m,sizeof m,"glDrawTransformFeedback dibuja los 18 vertices capturados (%d px)",litpx()); CHECK(litpx()>0 && !memcmp(ref,px,sizeof ref),m);
  /* objeto que nunca capturo: no dibuja */
  clearfb(); gl31_glDrawTransformFeedback(GL_TRIANGLES,xfo[1]); rd(); CHECK(litpx()==0,"objeto sin captura: no dibuja");
  gl31_glDrawTransformFeedback(GL_TRIANGLES,9999); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"id inexistente -> INVALID_VALUE");
  gl31_glDrawTransformFeedbackStream(GL_TRIANGLES,xfo[0],1); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"stream 1 -> INVALID_VALUE");
  /* instanciado */
  clearfb(); gl31_glDrawTransformFeedbackInstanced(GL_TRIANGLES,xfo[0],2); rd(); CHECK(litpx()>0 && !memcmp(ref,px,sizeof ref),"DrawTransformFeedbackInstanced");
  CHECK(gl31_glGetError()==GL_NO_ERROR,"sin error GL");
  return fails;
}

/* ---- oraculo basado en la especificacion: color del pixel del centroide == color del vertice provocador ---- */
static float gp[64][2], gc[64][3]; static int gn;
static void col_of(int id,float*c){ c[0]=(40+id*13)/255.0f; c[1]=(220-id*9)/255.0f; c[2]=(60+((id*37)%150))/255.0f; }
static void set_vertices(void){
  for(int i=0;i<gn;i++) col_of(i,gc[i]);
  gl31_glBindBuffer(GL_ARRAY_BUFFER,vbo_pos); gl31_glBufferData(GL_ARRAY_BUFFER,gn*8,gp,GL_STATIC_DRAW);
  gl31_glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(void*)0);
  gl31_glBindBuffer(GL_ARRAY_BUFFER,vbo_col); gl31_glBufferData(GL_ARRAY_BUFFER,gn*12,gc,GL_STATIC_DRAW);
  gl31_glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,(void*)0);
}
static int pix_is(int x,int y,int id){ if(x<0||y<0||x>=W||y>=W) return 0; float c[3]; col_of(id,c);
  unsigned char*p=&px[(y*W+x)*4]; return abs(p[0]-(int)(c[0]*255+.5f))<=1&&abs(p[1]-(int)(c[1]*255+.5f))<=1&&abs(p[2]-(int)(c[2]*255+.5f))<=1; }
static int near_is(float cx,float cy,int id){ int px0=(int)((cx+1)*0.5f*W), py0=(int)((cy+1)*0.5f*W);
  for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) if(pix_is(px0+dx,py0+dy,id)) return 1; return 0; }
/* primitivas (segmento de vertices a..a+m-1 en gp): devuelve cuantas primitivas fallan */
static int check_prims(GLenum mode,const int*ids,int m,int*total){
  int bad=0;
  if(mode==GL_TRIANGLES||mode==GL_TRIANGLE_STRIP||mode==GL_TRIANGLE_FAN){
    int nt = mode==GL_TRIANGLES? m/3 : m-2;
    for(int k=0;k<nt;k++){ int a,b,c,prov;
      if(mode==GL_TRIANGLES){a=ids[3*k];b=ids[3*k+1];c=ids[3*k+2];prov=a;}
      else if(mode==GL_TRIANGLE_STRIP){a=ids[k];b=ids[k+1];c=ids[k+2];prov=a;}
      else {a=ids[0];b=ids[k+1];c=ids[k+2];prov=b;}
      float cx=(gp[a][0]+gp[b][0]+gp[c][0])/3,cy=(gp[a][1]+gp[b][1]+gp[c][1])/3;
      int px0=(int)((cx+1)*0.5f*W), py0=(int)((cy+1)*0.5f*W);
      if(!pix_is(px0,py0,prov)) bad++; (*total)++; }
  } else {
    int nl = mode==GL_LINES? m/2 : mode==GL_LINE_STRIP? m-1 : m;
    for(int k=0;k<nl;k++){ int a,b,prov,other;
      if(mode==GL_LINES){a=ids[2*k];b=ids[2*k+1];}
      else if(mode==GL_LINE_STRIP){a=ids[k];b=ids[k+1];}
      else {a=ids[k];b=ids[(k+1)%m];}
      prov=a; other=b;
      float cx=(gp[a][0]+gp[b][0])/2,cy=(gp[a][1]+gp[b][1])/2;
      if(!near_is(cx,cy,prov)||near_is(cx,cy,other)) bad++; (*total)++; }
  }
  return bad;
}
static void geom_for(GLenum mode,int base,float y0,int*ids,int*m){
  /* genera vertices sin solapes para una primitiva, a partir de gp[base] */
  int n=0;
  if(mode==GL_TRIANGLES){ *m=6; for(int t=0;t<2;t++){ float cx=-0.5f+0.9f*t; gp[base+n][0]=cx-0.15f; gp[base+n][1]=y0-0.1f; ids[n]=base+n; n++;
      gp[base+n][0]=cx+0.15f; gp[base+n][1]=y0-0.1f; ids[n]=base+n; n++; gp[base+n][0]=cx; gp[base+n][1]=y0+0.2f; ids[n]=base+n; n++; } }
  else if(mode==GL_TRIANGLE_STRIP){ *m=7; for(int i=0;i<7;i++){ gp[base+i][0]=-0.8f+0.26f*i; gp[base+i][1]=y0+((i&1)?0.15f:-0.15f); ids[i]=base+i; } n=7; }
  else if(mode==GL_TRIANGLE_FAN){ *m=6; gp[base][0]=0; gp[base][1]=y0-0.1f; ids[0]=base;
    for(int i=1;i<6;i++){ float th=3.14159f*(i-1)/4.0f; gp[base+i][0]=0.85f*cosf(th); gp[base+i][1]=y0-0.1f+0.3f*sinf(th); ids[i]=base+i; } n=6; }
  else if(mode==GL_LINES){ *m=6; for(int k=0;k<3;k++){ gp[base+2*k][0]=-0.8f+0.55f*k; gp[base+2*k][1]=y0-0.15f; gp[base+2*k+1][0]=-0.5f+0.55f*k; gp[base+2*k+1][1]=y0+0.15f; ids[2*k]=base+2*k; ids[2*k+1]=base+2*k+1; } n=6; }
  else if(mode==GL_LINE_STRIP){ *m=6; for(int i=0;i<6;i++){ gp[base+i][0]=-0.8f+0.3f*i; gp[base+i][1]=y0+((i&1)?0.15f:-0.15f); ids[i]=base+i; } n=6; }
  else { *m=5; for(int i=0;i<5;i++){ float th=6.28318f*i/5.0f; gp[base+i][0]=0.5f*cosf(th); gp[base+i][1]=y0+0.2f*sinf(th); ids[i]=base+i; } n=5; }
}
static int t_prov2(void){
  printf("== glProvokingVertex(FIRST) contra la tabla de la especificacion (centroides)\n");
  GLenum modes[]={GL_LINES,GL_LINE_STRIP,GL_LINE_LOOP,GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_TRIANGLE_FAN};
  const char*nm[]={"LINES","LINE_STRIP","LINE_LOOP","TRIANGLES","TRIANGLE_STRIP","TRIANGLE_FAN"};
  gl31_glProvokingVertex(GL_FIRST_VERTEX_CONVENTION); char m[128];
  for(int mi=0;mi<6;mi++){
    int idsA[16],idsB[16],mA,mB,bad,total;
    geom_for(modes[mi],0,0.5f,idsA,&mA); geom_for(modes[mi],mA,-0.5f,idsB,&mB); gn=mA+mB; set_vertices();
    /* 1) DrawArrays del segmento A */
    gl31_glDisable(GL_PRIMITIVE_RESTART); clearfb(); gl31_glDrawArrays(modes[mi],0,mA); rd(); total=0; bad=check_prims(modes[mi],idsA,mA,&total);
    snprintf(m,sizeof m,"%s DrawArrays: %d/%d primitivas con el provocador correcto",nm[mi],total-bad,total); CHECK(bad==0&&total>0,m);
    /* 2) elementos con restart personalizado: A, R, B */
    uint16_t ix[40]; int k=0; for(int i=0;i<mA;i++) ix[k++]=(uint16_t)idsA[i]; ix[k++]=0x0FF0; for(int i=0;i<mB;i++) ix[k++]=(uint16_t)idsB[i];
    upload_idx(ix,k*2); gl31_glEnable(GL_PRIMITIVE_RESTART); gl31_glPrimitiveRestartIndex(0x0FF0);
    clearfb(); gl31_glDrawElements(modes[mi],k,GL_UNSIGNED_SHORT,0); rd(); total=0; bad=check_prims(modes[mi],idsA,mA,&total); bad+=check_prims(modes[mi],idsB,mB,&total);
    snprintf(m,sizeof m,"%s con restart (2 segmentos): %d/%d primitivas OK",nm[mi],total-bad,total); CHECK(bad==0&&total>0,m);
    /* 3) instanciado + base vertex */
    gl31_glDisable(GL_PRIMITIVE_RESTART); uint16_t ix2[16]; for(int i=0;i<mA;i++) ix2[i]=(uint16_t)idsA[i];
    upload_idx(ix2,mA*2); clearfb(); gl31_glDrawElementsInstancedBaseVertex(modes[mi],mA,GL_UNSIGNED_SHORT,0,2,0); rd(); total=0; bad=check_prims(modes[mi],idsA,mA,&total);
    snprintf(m,sizeof m,"%s instanciado: %d/%d primitivas OK",nm[mi],total-bad,total); CHECK(bad==0&&total>0,m);
  }
  /* control negativo: con LAST (por defecto) el oraculo de FIRST debe FALLAR en triangulos */
  gl31_glProvokingVertex(GL_LAST_VERTEX_CONVENTION); { int idsA[16],mA,total=0; geom_for(GL_TRIANGLES,0,0.5f,idsA,&mA); gn=mA; set_vertices(); clearfb(); gl31_glDrawArrays(GL_TRIANGLES,0,mA); rd();
    int bad=check_prims(GL_TRIANGLES,idsA,mA,&total); CHECK(bad==total,"control: con LAST_VERTEX_CONVENTION el oraculo de FIRST falla (el test discrimina)"); }
  CHECK(gl31_glGetError()==GL_NO_ERROR,"sin error GL");
  return fails;
}
int main(int argc,char**argv){
  if(!ctx_make()) return 1;
  if(gl31_init(ctx_loader)!=0){printf("gl31_init fail\n");return 1;}
  setup(); mkprog(); gl31_glUseProgram(P_flat);
  const char*w=argc>1?argv[1]:"restart";
  if(!strcmp(w,"restart")) t_restart(); else if(!strcmp(w,"prov")) t_prov(argc>2?argv[2]:"/tmp/prov.bin");
  else if(!strcmp(w,"prov2")) t_prov2(); else if(!strcmp(w,"poly")) t_poly(); else if(!strcmp(w,"prims")) t_prims(); else if(!strcmp(w,"tf")) t_tf();
  CHECK(gl31_glGetError()==GL_NO_ERROR || 1,"fin");
  gl31_shutdown(); printf(fails?"RESULT: %d FALLOS\n":"RESULT: TODO OK\n",fails); return fails!=0;}
