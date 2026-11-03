#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "ctx.h"
#include "gl31.h"
#include "gl31_vk.h"
#define GL_QUERY_COUNTER_BITS 0x8864
static double now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
static int fails=0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
int main(void){
  if(!ctx_make()) return 1;
  if(gl31_init(ctx_loader)!=0){printf("gl31_init fail\n");return 1;}
  printf("caps.timer(backend)=%d\n",gl31_caps.timer);
  CHECK(gl31_caps.timer==0,"ruta Vulkan forzada (backend sin timer)");
  CHECK(gl31_vkq_ready(),"Vulkan listo con timestamps");
  GLint bits=0; gl31_glGetQueryiv(GL_TIMESTAMP,GL_QUERY_COUNTER_BITS,&bits); CHECK(bits==64,"GL_QUERY_COUNTER_BITS(TIMESTAMP)==64");
  /* carga de GPU medible */
  GLuint fbo,tex; BE(glGenTextures)(1,&tex); BE(glBindTexture)(GL_TEXTURE_2D,tex);
  BE(glTexStorage2D)(GL_TEXTURE_2D,1,GL_RGBA8,2048,2048);
  BE(glGenFramebuffers)(1,&fbo); BE(glBindFramebuffer)(GL_FRAMEBUFFER,fbo); BE(glFramebufferTexture2D)(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
  GLuint q[3]; gl31_glGenQueries(3,q);
  /* programa pesado: bucle de senos en el fragment shader, 512x512 */
  const char*vs="#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0.0,1.0);}";
  const char*fs="#version 300 es\nprecision highp float;out vec4 o;uniform float u;void main(){float a=gl_FragCoord.x*0.001+u;for(int i=0;i<600;i++)a=sin(a)*1.0001+0.37;o=vec4(fract(a),0.0,0.0,1.0);}";
  GLuint sv=BE(glCreateShader)(GL_VERTEX_SHADER),sf=BE(glCreateShader)(GL_FRAGMENT_SHADER);
  BE(glShaderSource)(sv,1,&vs,NULL);BE(glCompileShader)(sv);BE(glShaderSource)(sf,1,&fs,NULL);BE(glCompileShader)(sf);
  GLuint pr=BE(glCreateProgram)();BE(glAttachShader)(pr,sv);BE(glAttachShader)(pr,sf);BE(glLinkProgram)(pr);
  GLint lk=0;BE(glGetProgramiv)(pr,GL_LINK_STATUS,&lk);CHECK(lk,"programa de carga enlaza");
  GLuint vao;BE(glGenVertexArrays)(1,&vao);BE(glBindVertexArray)(vao);BE(glUseProgram)(pr);BE(glViewport)(0,0,512,512);
  BE(glFinish)();
  gl31_glQueryCounter(q[0],GL_TIMESTAMP);
  gl31_glBeginQuery(GL_TIME_ELAPSED,q[2]);
  double w0=now_ms();
  for(int i=0;i<6;i++){ BE(glUniform1f)(BE(glGetUniformLocation)(pr,"u"),i*0.1f); BE(glDrawArrays)(GL_TRIANGLES,0,3); }
  gl31_glEndQuery(GL_TIME_ELAPSED);
  gl31_glQueryCounter(q[1],GL_TIMESTAMP);
  gl31_glFinish(); double w1=now_ms();
  GLuint64 av=0,t0=0,t1=0,el=0;
  gl31_glGetQueryObjectui64v(q[1],GL_QUERY_RESULT_AVAILABLE,&av); CHECK(av==1,"query disponible tras glFinish");
  gl31_glGetQueryObjectui64v(q[0],GL_QUERY_RESULT,&t0); gl31_glGetQueryObjectui64v(q[1],GL_QUERY_RESULT,&t1);
  gl31_glGetQueryObjectui64v(q[2],GL_QUERY_RESULT,&el);
  printf("  TIMESTAMP t0=%llu t1=%llu delta=%.3f ms | TIME_ELAPSED=%.3f ms | pared CPU=%.3f ms\n",(unsigned long long)t0,(unsigned long long)t1,(t1-t0)/1e6,el/1e6,w1-w0);
  CHECK(t0>0 && t1>t0,"GL_TIMESTAMP monotono (t1>t0>0)");
  CHECK(el>0,"GL_TIME_ELAPSED > 0");
  double dm=(t1-t0)/1e6; CHECK(dm>0.5*(w1-w0) && dm<1.5*(w1-w0)+5,"delta de timestamps coherente con el tiempo de pared (+-50%)");
  GLint64 nowv=0; gl31_glGetInteger64v(GL_TIMESTAMP,&nowv); CHECK((GLuint64)nowv>=t1,"glGetInteger64v(GL_TIMESTAMP) >= ultimo timestamp");
  
  gl31_glDeleteQueries(3,q); CHECK(!gl31_vkq_owns(q[0]),"borrado libera slots Vulkan");
  GLenum e=BE(glGetError)(); CHECK(e==0,"sin error GL");
  gl31_shutdown(); printf(fails?"RESULT: %d FALLOS\n":"RESULT: TODO OK\n",fails); return fails!=0;}
