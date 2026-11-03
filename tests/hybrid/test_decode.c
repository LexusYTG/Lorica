/* test_decode.c - glGetTexImage de texturas comprimidas (descompresion en GPU) contra el decodificador del driver */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "ctx.h"
#include "gl31.h"
static int fails=0;
#define W 16
static GLuint prog_make(const char*vs,const char*fs){
  GLuint v=gl31_glCreateShader(GL_VERTEX_SHADER),f=gl31_glCreateShader(GL_FRAGMENT_SHADER),p=gl31_glCreateProgram(); char log[512]; GLint ok;
  gl31_glShaderSource(v,1,&vs,NULL);gl31_glCompileShader(v);gl31_glShaderSource(f,1,&fs,NULL);gl31_glCompileShader(f);
  gl31_glGetShaderiv(f,GL_COMPILE_STATUS,&ok); if(!ok){gl31_glGetShaderInfoLog(f,512,NULL,log);printf("FS: %s\n",log);}
  gl31_glAttachShader(p,v);gl31_glAttachShader(p,f);gl31_glLinkProgram(p);return p;}
int main(void){
  if(!ctx_make()) return 1; if(gl31_init(ctx_loader)!=0) return 1;
  struct { GLenum f; const char* n; int bytes; int comps; int sgn; } F[]={
    {0x83F0,"DXT1 RGB",8,3,0},{0x83F1,"DXT1 RGBA",8,4,0},{0x83F2,"DXT3",16,4,0},{0x83F3,"DXT5",16,4,0},
    {0x8DBB,"RGTC1",8,1,0},{0x8DBC,"RGTC1 signed",8,1,1},{0x8DBD,"RGTC2",16,2,0},{0x8DBE,"RGTC2 signed",16,2,1},
    {0x9270,"EAC R11",8,1,0},{0x9271,"EAC R11 signed",8,1,1},{0x9272,"EAC RG11",16,2,0},{0x9273,"EAC RG11 signed",16,2,1},
    {0x9274,"ETC2 RGB8",8,3,0},{0x9275,"ETC2 sRGB8",8,3,0},{0x9276,"ETC2 RGB8 punchthrough",8,4,0},{0x9277,"ETC2 sRGB8 punchthrough",8,4,0},
    {0x9278,"ETC2 RGBA8 EAC",16,4,0},{0x9279,"ETC2 sRGB8 A8 EAC",16,4,0}};
  GLuint fbo,ft,vao; gl31_glGenTextures(1,&ft); gl31_glBindTexture(GL_TEXTURE_2D,ft); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,W,W);
  gl31_glGenFramebuffers(1,&fbo); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ft,0);
  gl31_glGenVertexArrays(1,&vao); gl31_glBindVertexArray(vao); gl31_glViewport(0,0,W,W);
  GLuint pr=prog_make("#version 330 core\nconst vec2 P[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));void main(){gl_Position=vec4(P[gl_VertexID],0.0,1.0);}",
    "#version 330 core\nuniform sampler2D t; out vec4 o; void main(){o=texelFetch(t,ivec2(gl_FragCoord.xy),0);}");
  gl31_glUseProgram(pr); gl31_glUniform1i(gl31_glGetUniformLocation(pr,"t"),0);
  srand(12345);
  for(unsigned fi=0;fi<sizeof F/sizeof F[0];fi++){
    int nb=(W/4)*(W/4)*F[fi].bytes; unsigned char* d=malloc(nb); GLuint t;
    double maxerr=0; int bad=0, trials=12, supported=1;
    for(int tr=0;tr<trials&&supported;tr++){
      for(int i=0;i<nb;i++) d[i]=(unsigned char)(rand()&255);
      gl31_glGenTextures(1,&t); gl31_glBindTexture(GL_TEXTURE_2D,t);
      gl31_glCompressedTexImage2D(GL_TEXTURE_2D,0,F[fi].f,W,W,0,nb,d);
      if(gl31_glGetError()){ supported=0; break; }
      gl31_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); gl31_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      float mine[W*W*4],ref[W*W*4]; memset(mine,0,sizeof mine);
      gl31_glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,mine);
      GLenum e=gl31_glGetError(); if(e){ printf("  %s: error GL 0x%x en glGetTexImage\n",F[fi].n,e); bad=W*W; break; }
      gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo); gl31_glActiveTexture(GL_TEXTURE0); gl31_glBindTexture(GL_TEXTURE_2D,t);
      gl31_glUseProgram(pr); gl31_glDrawArrays(GL_TRIANGLES,0,3); gl31_glReadPixels(0,0,W,W,GL_RGBA,GL_FLOAT,ref);
      gl31_glBindFramebuffer(GL_FRAMEBUFFER,0);
      if(F[fi].f==0x9275||F[fi].f==0x9277||F[fi].f==0x9279)       /* el driver devuelve lineal al muestrear sRGB: re-codificar */
        for(int i=0;i<W*W*4;i++) if((i&3)<3){ float c=ref[i]; ref[i]= c<=0.0031308f?12.92f*c:1.055f*powf(c,1.f/2.4f)-0.055f; }
      for(int i=0;i<W*W*4;i++){ int c=i&3; if(c>=F[fi].comps && !(c==3)) { if(c<3 && fabsf(mine[i])>1e-6f) bad++; continue; }
        double er=fabs(mine[i]-ref[i]); if(er>maxerr) maxerr=er; if(er>((F[fi].sgn&&F[fi].f<0x9000)?0.017:0.012)) bad++; }
      gl31_glDeleteTextures(1,&t);
    }
    if(!supported) printf("  (saltado: %s no soportado por el backend)\n",F[fi].n);
    else { printf("  %s %s: error maximo %.4f, %d valores fuera de tolerancia\n",bad?"FAIL":"ok  ",F[fi].n,maxerr,bad); if(bad) fails++; }
    free(d);
  }
  printf("RESULT: %s (%d formatos con fallos)\n",fails?"HAY FALLOS":"TODO OK",fails);
  return fails?1:0; }
