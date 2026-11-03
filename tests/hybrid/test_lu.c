#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ctx.h"
#include "gl31.h"
#define GL_POINT_SPRITE_COORD_ORIGIN 0x8CA0
static int fails=0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
#define W 64
static unsigned char px[W*W*4];
static GLuint prog_make(const char*vs,const char*fs){
  GLuint v=gl31_glCreateShader(GL_VERTEX_SHADER),f=gl31_glCreateShader(GL_FRAGMENT_SHADER),p=gl31_glCreateProgram();
  gl31_glShaderSource(v,1,&vs,NULL);gl31_glCompileShader(v);gl31_glShaderSource(f,1,&fs,NULL);gl31_glCompileShader(f);
  GLint ok=0;char log[1024];
  gl31_glGetShaderiv(v,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(v,1024,NULL,log);printf("VS: %s\n",log);return 0;}
  gl31_glGetShaderiv(f,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(f,1024,NULL,log);printf("FS: %s\n",log);return 0;}
  gl31_glAttachShader(p,v);gl31_glAttachShader(p,f);gl31_glLinkProgram(p);
  gl31_glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){gl31_glGetProgramInfoLog(p,1024,NULL,log);printf("LINK: %s\n",log);return 0;}
  return p;}
static void clear(void){gl31_glClearColor(0,0,0,0);gl31_glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
static void rd(void){gl31_glReadPixels(0,0,W,W,GL_RGBA,GL_UNSIGNED_BYTE,px);}
static int lit(void){int n=0;for(int i=0;i<W*W;i++) if(px[i*4]||px[i*4+1]||px[i*4+2]) n++; return n;}
static unsigned char*P(int x,int y){return &px[(y*W+x)*4];}
static const char*FS_RED="#version 330 core\nout vec4 c;void main(){c=vec4(1,0,0,1);}";
int main(void){
  if(!ctx_make()) return 1;
  if(gl31_init(ctx_loader)!=0){printf("gl31_init fail\n");return 1;}
  printf("backend: clip_distance=%d depth_clamp=%d\n",gl31_caps.clip_distance,gl31_caps.depth_clamp);
  GLuint tex,fbo,rb,vao; gl31_glGenTextures(1,&tex); gl31_glBindTexture(GL_TEXTURE_2D,tex); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,W,W);
  printf("  tras textura err=0x%x\n",gl31_glGetError());
  gl31_glGenRenderbuffers(1,&rb); gl31_glBindRenderbuffer(GL_RENDERBUFFER,rb); gl31_glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,W,W);
  printf("  tras renderbuffer err=0x%x\n",gl31_glGetError());
  gl31_glGenFramebuffers(1,&fbo); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0); gl31_glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,rb);
  { GLenum st=gl31_glCheckFramebufferStatus(GL_FRAMEBUFFER); printf("  fbo status=0x%x err=0x%x\n",st,gl31_glGetError()); CHECK(st==GL_FRAMEBUFFER_COMPLETE,"FBO completo"); }
  gl31_glGenVertexArrays(1,&vao); gl31_glBindVertexArray(vao); gl31_glViewport(0,0,W,W);

  /* ---- A: glPointSize con un VS que NO escribe gl_PointSize ---- */
  const char*vsP="#version 330 core\nvoid main(){gl_Position=vec4(0.0,0.0,0.0,1.0);}";
  GLuint pp=prog_make(vsP,FS_RED); gl31_glUseProgram(pp);
  int sizes[]={1,4,9}; for(int k=0;k<3;k++){ clear(); gl31_glPointSize((float)sizes[k]); gl31_glDrawArrays(GL_POINTS,0,1); rd();
    char m[96]; snprintf(m,96,"glPointSize(%d) sin gl_PointSize en el VS -> %d px (obtenido %d)",sizes[k],sizes[k]*sizes[k],lit()); CHECK(lit()==sizes[k]*sizes[k],m);}
  /* ---- A2: VS que escribe gl_PointSize ---- */
  const char*vsP2="#version 330 core\nvoid main(){gl_Position=vec4(0.0,0.0,0.0,1.0);gl_PointSize=10.0;}";
  GLuint pp2=prog_make(vsP2,FS_RED); gl31_glUseProgram(pp2); gl31_glPointSize(3.0f);
  clear(); gl31_glDrawArrays(GL_POINTS,0,1); rd(); CHECK(lit()==9,"PROGRAM_POINT_SIZE desactivado: manda glPointSize (3x3)");
  gl31_glEnable(GL_PROGRAM_POINT_SIZE); clear(); gl31_glDrawArrays(GL_POINTS,0,1); rd(); CHECK(lit()==100,"PROGRAM_POINT_SIZE activado: manda el VS (10x10)");
  gl31_glDisable(GL_PROGRAM_POINT_SIZE);
  /* ---- B: origen de point sprite ---- */
  const char*fsPC="#version 330 core\nout vec4 c;void main(){c=vec4(gl_PointCoord.x,gl_PointCoord.y,0.0,1.0);}";
  GLuint pc=prog_make(vsP,fsPC); gl31_glUseProgram(pc); gl31_glPointSize(16.0f);
  clear(); gl31_glDrawArrays(GL_POINTS,0,1); rd(); int top=32+7,bot=32-8; int g_up=P(32,top)[1],g_bot=P(32,bot)[1];
  printf("  UPPER_LEFT: g(fila alta)=%d g(fila baja)=%d\n",g_up,g_bot); CHECK(g_up<64 && g_bot>190,"UPPER_LEFT (defecto): y=0 arriba");
  gl31_glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN,GL_LOWER_LEFT);
  clear(); gl31_glDrawArrays(GL_POINTS,0,1); rd(); g_up=P(32,top)[1];g_bot=P(32,bot)[1];
  printf("  LOWER_LEFT: g(fila alta)=%d g(fila baja)=%d\n",g_up,g_bot); CHECK(g_up>190 && g_bot<64,"LOWER_LEFT: y=0 abajo");
  gl31_glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN,GL_UPPER_LEFT);
  /* ---- C: clip distance ---- */
  const char*tri="const vec2 P[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));\n";
  char vsC[600]; snprintf(vsC,600,"#version 330 core\n%svoid main(){gl_Position=vec4(P[gl_VertexID],0.0,1.0);gl_ClipDistance[0]=P[gl_VertexID].x;}",tri);
  GLuint pcd=prog_make(vsC,FS_RED); gl31_glUseProgram(pcd);
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); CHECK(lit()==W*W,"clip distance sin habilitar: pantalla completa");
  gl31_glEnable(GL_CLIP_DISTANCE0); clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd();
  int l=lit(); printf("  clip distance habilitada: %d px (mitad=%d)\n",l,W*W/2);
  CHECK(l>=W*W/2-W && l<=W*W/2+W && P(8,32)[0]==0 && P(56,32)[0]==255,"GL_CLIP_DISTANCE0 recorta la mitad x<0");
  gl31_glDisable(GL_CLIP_DISTANCE0); clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); CHECK(lit()==W*W,"deshabilitar restaura");
  { GLint na=0; gl31_glGetProgramiv(pcd,GL_ACTIVE_UNIFORMS,&na); CHECK(na==0,"el VS+FS con clip distance emuladas: 0 uniforms activos visibles"); }
  /* ---- D: clip control ---- */
  char vsZ[600]; snprintf(vsZ,600,"#version 330 core\n%svoid main(){gl_Position=vec4(P[gl_VertexID],0.25,1.0);}",tri);
  const char*fsZ="#version 330 core\nout vec4 c;void main(){c=vec4(gl_FragCoord.z,0.0,0.0,1.0);}";
  GLuint pz=prog_make(vsZ,fsZ); gl31_glUseProgram(pz);
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); int r0=P(32,32)[0];
  gl31_glClipControl(GL_LOWER_LEFT,GL_ZERO_TO_ONE);
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); int r1=P(32,32)[0];
  printf("  z clip=0.25: depth NEGATIVE_ONE_TO_ONE=%d (esperado ~159)  ZERO_TO_ONE=%d (esperado ~64)\n",r0,r1);
  CHECK(r0>=156&&r0<=162,"depth por defecto = 0.625"); CHECK(r1>=61&&r1<=67,"ZERO_TO_ONE: depth = 0.25 exacto");
  GLint cm=0,co=0; gl31_glGetIntegerv(GL_CLIP_DEPTH_MODE,&cm); gl31_glGetIntegerv(GL_CLIP_ORIGIN,&co);
  CHECK(cm==GL_ZERO_TO_ONE && co==GL_LOWER_LEFT,"glGetIntegerv(CLIP_DEPTH_MODE/CLIP_ORIGIN)");
  gl31_glClipControl(GL_LOWER_LEFT,GL_NEGATIVE_ONE_TO_ONE);
  gl31_glClipControl(0x1234,GL_ZERO_TO_ONE); CHECK(gl31_glGetError()==GL_INVALID_ENUM,"glClipControl con enum invalido");
  /* flip Y: triangulo en la mitad superior del clip space */
  char vsY[600]; snprintf(vsY,600,"#version 330 core\nconst vec2 Q[3]=vec2[3](vec2(-1.0,0.0),vec2(1.0,0.0),vec2(0.0,1.0));void main(){gl_Position=vec4(Q[gl_VertexID],0.0,1.0);}");
  GLuint py=prog_make(vsY,FS_RED); gl31_glUseProgram(py); gl31_glDisable(GL_CULL_FACE);
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); int up_ok=P(32,48)[0]==255&&P(32,16)[0]==0;
  gl31_glClipControl(GL_UPPER_LEFT,GL_NEGATIVE_ONE_TO_ONE);
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); int dn_ok=P(32,16)[0]==255&&P(32,48)[0]==0;
  CHECK(up_ok,"LOWER_LEFT: triangulo arriba"); CHECK(dn_ok,"UPPER_LEFT: triangulo invertido en Y");
  gl31_glFrontFace(GL_CW); GLint ff=0; gl31_glGetIntegerv(GL_FRONT_FACE,&ff); CHECK(ff==GL_CW,"GL_FRONT_FACE devuelve el valor de la app");
  /* culling coherente con el flip: la cara frontal (CCW) sigue siendo visible tras UPPER_LEFT */
  gl31_glFrontFace(GL_CCW); gl31_glEnable(GL_CULL_FACE); gl31_glCullFace(GL_BACK);
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); CHECK(lit()>0,"cara frontal no se descarta tras glClipControl(UPPER_LEFT)");
  gl31_glDisable(GL_CULL_FACE); gl31_glClipControl(GL_LOWER_LEFT,GL_NEGATIVE_ONE_TO_ONE);
  /* ---- E: depth clamp ---- */
  char vsD[600]; snprintf(vsD,600,"#version 330 core\n%svoid main(){gl_Position=vec4(P[gl_VertexID],1.5,1.0);}",tri);
  GLuint pd=prog_make(vsD,FS_RED); gl31_glUseProgram(pd);
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); CHECK(lit()==0,"z > far sin depth clamp: recortado");
  gl31_glEnable(GL_DEPTH_CLAMP); CHECK(gl31_glIsEnabled(GL_DEPTH_CLAMP)==GL_TRUE,"IsEnabled(DEPTH_CLAMP)");
  clear(); gl31_glDrawArrays(GL_TRIANGLES,0,3); rd(); CHECK(lit()==W*W,"GL_DEPTH_CLAMP: se dibuja con z sujeto a [far]");
  gl31_glDisable(GL_DEPTH_CLAMP);
  /* ---- F: introspeccion de uniforms ---- */
  const char*vsU="#version 330 core\nuniform vec4 u_a;uniform float u_b;void main(){gl_Position=u_a*u_b;}";
  const char*fsU="#version 330 core\nuniform vec4 u_c;out vec4 c;void main(){c=u_c;}";
  GLuint pu=prog_make(vsU,fsU); GLint nu=0; gl31_glGetProgramiv(pu,GL_ACTIVE_UNIFORMS,&nu); CHECK(nu==3,"GL_ACTIVE_UNIFORMS = 3 (sin lorica_*)");
  int seen=0,lor=0; for(int i=0;i<nu;i++){char nm[64];GLint sz;GLenum ty;gl31_glGetActiveUniform(pu,i,64,NULL,&sz,&ty,nm); if(!strncmp(nm,"lorica_",7))lor++; else seen++;}
  CHECK(seen==3&&lor==0,"glGetActiveUniform enumera solo los 3 de la app");
  const char*names[3]={"u_a","u_b","u_c"}; GLuint ids[3]; gl31_glGetUniformIndices(pu,3,names,ids);
  int okid=1; for(int i=0;i<3;i++){char nm[64];gl31_glGetActiveUniformName(pu,ids[i],64,NULL,nm); if(strcmp(nm,names[i]))okid=0;}
  CHECK(okid,"GetUniformIndices -> GetActiveUniformName consistentes");
  CHECK(gl31_glGetError()==GL_NO_ERROR,"sin error GL pendiente");
  gl31_shutdown(); printf(fails?"RESULT: %d FALLOS\n":"RESULT: TODO OK\n",fails); return fails!=0;}
