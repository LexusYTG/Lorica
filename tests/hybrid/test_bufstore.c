#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ctx.h"
#include "gl31.h"
#include "gl31_vk.h"
static int fails=0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
#define ERR(e,m) do{ GLenum g=gl31_glGetError(); if(g!=(e)){printf("  FAIL: %s (error 0x%x, esperado 0x%x)\n",m,g,(unsigned)(e));fails++;} else printf("  ok: %s\n",m);}while(0)
#define N 4096
int main(void){
  if(!ctx_make()) return 1;
  if(gl31_init(ctx_loader)!=0){printf("gl31_init fail\n");return 1;}
  printf("backend: buffer_storage=%d memobj_fd=%d  LORICA_BUFSTORE=%s\n",gl31_caps.buffer_storage,gl31_caps.memobj_fd,getenv("LORICA_BUFSTORE")?getenv("LORICA_BUFSTORE"):"(auto)");
  const GLbitfield PF=GL_MAP_READ_BIT|GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT|GL_DYNAMIC_STORAGE_BIT;
  GLuint b[3]; gl31_glGenBuffers(3,b);
  /* 1: crear almacenamiento persistente */
  gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,b[0]);
  gl31_glBufferStorage(GL_SHADER_STORAGE_BUFFER,N*4,NULL,PF); ERR(GL_NO_ERROR,"glBufferStorage(PERSISTENT|COHERENT) sin error");
  GLint v=0; gl31_glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER,GL_BUFFER_IMMUTABLE_STORAGE,&v); CHECK(v==1,"GL_BUFFER_IMMUTABLE_STORAGE");
  gl31_glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER,GL_BUFFER_STORAGE_FLAGS,&v); CHECK((GLbitfield)v==PF,"GL_BUFFER_STORAGE_FLAGS");
  /* 2: errores de la especificacion */
  gl31_glBufferData(GL_SHADER_STORAGE_BUFFER,16,NULL,GL_STATIC_DRAW); ERR(GL_INVALID_OPERATION,"glBufferData sobre inmutable -> INVALID_OPERATION");
  gl31_glBufferStorage(GL_SHADER_STORAGE_BUFFER,16,NULL,PF); ERR(GL_INVALID_OPERATION,"segundo glBufferStorage -> INVALID_OPERATION");
  gl31_glBufferStorage(GL_SHADER_STORAGE_BUFFER,0,NULL,PF); ERR(GL_INVALID_VALUE,"size 0 -> INVALID_VALUE");
  gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,b[2]); gl31_glBufferStorage(GL_SHADER_STORAGE_BUFFER,64,NULL,GL_MAP_COHERENT_BIT|GL_MAP_WRITE_BIT); ERR(GL_INVALID_VALUE,"COHERENT sin PERSISTENT -> INVALID_VALUE");
  gl31_glBufferStorage(GL_SHADER_STORAGE_BUFFER,64,NULL,GL_MAP_WRITE_BIT); ERR(GL_NO_ERROR,"storage solo WRITE ok");
  gl31_glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,64,GL_MAP_READ_BIT); ERR(GL_INVALID_OPERATION,"mapear READ sin MAP_READ_BIT -> INVALID_OPERATION");
  gl31_glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,64,GL_MAP_WRITE_BIT|GL_MAP_PERSISTENT_BIT); ERR(GL_INVALID_OPERATION,"mapear PERSISTENT sin el flag -> INVALID_OPERATION");
  /* 3: puntero persistente */
  gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,b[0]);
  float *p=(float*)gl31_glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,N*4,GL_MAP_WRITE_BIT|GL_MAP_READ_BIT|GL_MAP_PERSISTENT_BIT|GL_MAP_COHERENT_BIT);
  CHECK(p!=NULL,"MapBufferRange persistente devuelve puntero"); ERR(GL_NO_ERROR,"sin error tras mapear");
  if(!p) {printf("RESULT: abortado\n");return 1;}
  GLint mapped=0; gl31_glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER,GL_BUFFER_MAPPED,&mapped); CHECK(mapped==1,"GL_BUFFER_MAPPED");
  for(int i=0;i<N;i++) p[i]=(float)i;
  /* 4: compute de GLES lee lo escrito por la CPU y escribe en b[1]; luego compute escribe en b[0] */
  const char*cs="#version 310 es\nlayout(local_size_x=64) in;layout(std430,binding=0) buffer A{float a[];};layout(std430,binding=1) buffer B{float b[];};uniform int mode;\nvoid main(){uint i=gl_GlobalInvocationID.x; if(mode==0) b[i]=a[i]*2.0; else a[i]=b[i]+1.0;}";
  GLuint sh=BE(glCreateShader)(GL_COMPUTE_SHADER);BE(glShaderSource)(sh,1,&cs,NULL);BE(glCompileShader)(sh);
  GLuint pr=BE(glCreateProgram)();BE(glAttachShader)(pr,sh);BE(glLinkProgram)(pr);GLint ok=0;BE(glGetProgramiv)(pr,GL_LINK_STATUS,&ok);
  if(!ok){char l[512];BE(glGetProgramInfoLog)(pr,512,NULL,l);printf("link: %s\n",l);return 1;}
  gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,b[1]); gl31_glBufferData(GL_SHADER_STORAGE_BUFFER,N*4,NULL,GL_DYNAMIC_COPY);
  gl31_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,b[0]); gl31_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,b[1]);
  BE(glUseProgram)(pr); BE(glUniform1i)(BE(glGetUniformLocation)(pr,"mode"),0);
  BE(glDispatchCompute)(N/64,1,1); BE(glMemoryBarrier)(GL_SHADER_STORAGE_BARRIER_BIT); BE(glFinish)();
  float *r=(float*)malloc(N*4); gl31_glBindBuffer(GL_COPY_READ_BUFFER,b[1]); gl31_glGetBufferSubData(GL_COPY_READ_BUFFER,0,N*4,r);
  int bad=0; for(int i=0;i<N;i++) if(r[i]!=2.0f*i){bad++;} CHECK(bad==0,"GLES lee (compute) lo que la CPU escribio por el puntero persistente");
  BE(glUniform1i)(BE(glGetUniformLocation)(pr,"mode"),1);
  BE(glDispatchCompute)(N/64,1,1); BE(glMemoryBarrier)(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT); BE(glFinish)();
  bad=0; for(int i=0;i<N;i++) if(p[i]!=2.0f*i+1.0f){bad++;} CHECK(bad==0,"CPU lee por el puntero persistente lo que escribio el compute de GLES");
  /* 5: glGetBufferSubData sobre el persistente (sin pasar por el mapeo) */
  gl31_glBindBuffer(GL_COPY_READ_BUFFER,b[0]); gl31_glGetBufferSubData(GL_COPY_READ_BUFFER,0,N*4,r);
  bad=0; for(int i=0;i<N;i++) if(r[i]!=2.0f*i+1.0f){bad++;} CHECK(bad==0,"glGetBufferSubData devuelve lo escrito por GPU");
  /* 6: glBufferSubData con DYNAMIC_STORAGE */
  float patch[16]; for(int i=0;i<16;i++) patch[i]=-100.f-i;
  gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,b[0]); gl31_glBufferSubData(GL_SHADER_STORAGE_BUFFER,64,sizeof patch,patch); ERR(GL_NO_ERROR,"glBufferSubData en storage DYNAMIC");
  BE(glFinish)(); bad=0; for(int i=0;i<16;i++) if(p[16+i]!=patch[i]) bad++; CHECK(bad==0,"glBufferSubData visible por el puntero persistente");
  /* 7: unmap / remap / borrado */
  CHECK(gl31_glUnmapBuffer(GL_SHADER_STORAGE_BUFFER)==GL_TRUE,"glUnmapBuffer"); ERR(GL_NO_ERROR,"sin error tras unmap");
  gl31_glDeleteBuffers(3,b); ERR(GL_NO_ERROR,"glDeleteBuffers sin error");
  free(r); gl31_shutdown(); printf(fails?"RESULT: %d FALLOS\n":"RESULT: TODO OK\n",fails); return fails!=0;}
