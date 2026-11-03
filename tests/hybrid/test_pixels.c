/* test_pixels.c - motor de transferencia de pixeles: glReadPixels depth/stencil en GPU, GL_PACK_*,
 * SWAP_BYTES, glClampColor, consulta de GL_FRONT. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "ctx.h"
#include "gl31.h"
static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
#define W 16
static GLuint prog_make(const char*vs,const char*fs){
  GLuint v=gl31_glCreateShader(GL_VERTEX_SHADER),f=gl31_glCreateShader(GL_FRAGMENT_SHADER),p=gl31_glCreateProgram();
  gl31_glShaderSource(v,1,&vs,NULL);gl31_glCompileShader(v);gl31_glShaderSource(f,1,&fs,NULL);gl31_glCompileShader(f);
  GLint ok=0;char log[1024];
  gl31_glGetShaderiv(v,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(v,1024,NULL,log);printf("VS: %s\n",log);return 0;}
  gl31_glGetShaderiv(f,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(f,1024,NULL,log);printf("FS: %s\n",log);return 0;}
  gl31_glAttachShader(p,v);gl31_glAttachShader(p,f);gl31_glLinkProgram(p);
  gl31_glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){gl31_glGetProgramInfoLog(p,1024,NULL,log);printf("LINK: %s\n",log);return 0;}
  return p;}
static int noerr(const char*m){ GLenum e=gl31_glGetError(); char b[128]; snprintf(b,128,"%s: sin error GL (0x%x)",m,e); CHECK(e==0,b); return e==0; }
/* profundidad esperada en el centro del pixel (i,j): 0.5 + (ndcx + ndcy)/4 */
static float expd(int i,int j){ float nx=((i+0.5f)/W)*2-1, ny=((j+0.5f)/W)*2-1; return 0.5f+(nx+ny)/4.f; }
int main(void){
  if(!ctx_make()) return 1;
  if(gl31_init(ctx_loader)!=0){printf("gl31_init fail\n");return 1;}
  GLuint rb,fbo,tex,vao;
  gl31_glGenTextures(1,&tex); gl31_glBindTexture(GL_TEXTURE_2D,tex); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,W,W);
  gl31_glGenRenderbuffers(1,&rb); gl31_glBindRenderbuffer(GL_RENDERBUFFER,rb); gl31_glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,W,W);
  gl31_glGenFramebuffers(1,&fbo); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
  gl31_glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,rb);
  CHECK(gl31_glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"FBO D24S8 completo");
  gl31_glGenVertexArrays(1,&vao); gl31_glBindVertexArray(vao); gl31_glViewport(0,0,W,W);
  const char*vs="#version 330 core\nconst vec2 P[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));\nvoid main(){vec2 p=P[gl_VertexID];gl_Position=vec4(p,(p.x+p.y)*0.5,1.0);}";
  const char*fs="#version 330 core\nout vec4 c;void main(){c=vec4(1.0);}";
  GLuint pr=prog_make(vs,fs); gl31_glUseProgram(pr);
  gl31_glEnable(GL_DEPTH_TEST); gl31_glDepthFunc(GL_ALWAYS);
  gl31_glEnable(GL_STENCIL_TEST); gl31_glStencilFunc(GL_ALWAYS,0x5A,0xFF); gl31_glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
  gl31_glClearDepthf(1.0f); gl31_glClearStencil(0); gl31_glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
  gl31_glDrawArrays(GL_TRIANGLES,0,3);
  noerr("dibujo");

  printf("== DEPTH_COMPONENT\n");
  { float d[W*W]; memset(d,0,sizeof d);
    gl31_glReadPixels(0,0,W,W,GL_DEPTH_COMPONENT,GL_FLOAT,d); noerr("FLOAT");
    int bad=0; for(int j=0;j<W;j++)for(int i=0;i<W;i++) if(fabsf(d[j*W+i]-expd(i,j))>2e-3f) bad++;
    CHECK(bad==0,"DEPTH_COMPONENT/FLOAT coincide con el gradiente (orientacion y valores)");
    if(bad) printf("   d[0]=%f esp=%f d[last]=%f esp=%f\n",d[0],expd(0,0),d[W*W-1],expd(W-1,W-1)); }
  { uint32_t d[W*W]; gl31_glReadPixels(0,0,W,W,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,d); noerr("UINT");
    int bad=0; for(int j=0;j<W;j++)for(int i=0;i<W;i++) if(fabs(d[j*W+i]/4294967295.0-expd(i,j))>2e-3) bad++;
    CHECK(bad==0,"DEPTH_COMPONENT/UNSIGNED_INT"); }
  { uint16_t d[W*W]; gl31_glReadPixels(0,0,W,W,GL_DEPTH_COMPONENT,GL_UNSIGNED_SHORT,d); noerr("USHORT");
    int bad=0; for(int j=0;j<W;j++)for(int i=0;i<W;i++) if(fabs(d[j*W+i]/65535.0-expd(i,j))>2e-3) bad++;
    CHECK(bad==0,"DEPTH_COMPONENT/UNSIGNED_SHORT"); }
  { uint8_t d[W*W]; gl31_glReadPixels(0,0,W,W,GL_DEPTH_COMPONENT,GL_UNSIGNED_BYTE,d); noerr("UBYTE");
    int bad=0; for(int j=0;j<W;j++)for(int i=0;i<W;i++) if(fabs(d[j*W+i]/255.0-expd(i,j))>6e-3) bad++;
    CHECK(bad==0,"DEPTH_COMPONENT/UNSIGNED_BYTE"); }
  printf("== STENCIL_INDEX\n");
  { uint8_t s[W*W]; memset(s,0,sizeof s); gl31_glReadPixels(0,0,W,W,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,s); noerr("UBYTE");
    int bad=0; for(int i=0;i<W*W;i++) if(s[i]!=0x5A) bad++; CHECK(bad==0,"STENCIL_INDEX/UNSIGNED_BYTE = 0x5A en todos"); }
  { uint32_t s[W*W]; gl31_glReadPixels(0,0,W,W,GL_STENCIL_INDEX,GL_UNSIGNED_INT,s); noerr("UINT");
    int bad=0; for(int i=0;i<W*W;i++) if(s[i]!=0x5A) bad++; CHECK(bad==0,"STENCIL_INDEX/UNSIGNED_INT"); }
  printf("== DEPTH_STENCIL\n");
  { uint32_t v[W*W]; gl31_glReadPixels(0,0,W,W,GL_DEPTH_STENCIL,GL_UNSIGNED_INT_24_8,v); noerr("24_8");
    int bad=0; for(int j=0;j<W;j++)for(int i=0;i<W;i++){ uint32_t x=v[j*W+i]; if((x&0xFF)!=0x5A || fabs((x>>8)/16777215.0-expd(i,j))>2e-3) bad++; }
    CHECK(bad==0,"DEPTH_STENCIL/UNSIGNED_INT_24_8"); }
  { struct {float d; uint32_t s;} v[W*W]; gl31_glReadPixels(0,0,W,W,GL_DEPTH_STENCIL,GL_FLOAT_32_UNSIGNED_INT_24_8_REV,v); noerr("32F_24_8_REV");
    int bad=0; for(int j=0;j<W;j++)for(int i=0;i<W;i++){ if((v[j*W+i].s&0xFF)!=0x5A || fabsf(v[j*W+i].d-expd(i,j))>2e-3f) bad++; }
    CHECK(bad==0,"DEPTH_STENCIL/FLOAT_32_UNSIGNED_INT_24_8_REV"); }
  printf("== GL_PACK_* y SWAP_BYTES\n");
  { /* sub-rectangulo 4x3 desde (5,2), row_length 10, skip 2 px / 1 fila, alineacion 1 */
    float buf[10*5]; for(int i=0;i<50;i++) buf[i]=-7.f;
    gl31_glPixelStorei(GL_PACK_ROW_LENGTH,10); gl31_glPixelStorei(GL_PACK_SKIP_PIXELS,2); gl31_glPixelStorei(GL_PACK_SKIP_ROWS,1); gl31_glPixelStorei(GL_PACK_ALIGNMENT,1);
    gl31_glReadPixels(5,2,4,3,GL_DEPTH_COMPONENT,GL_FLOAT,buf); noerr("pack");
    int bad=0,pad=0; for(int j=0;j<3;j++)for(int i=0;i<4;i++) if(fabsf(buf[(1+j)*10+2+i]-expd(5+i,2+j))>2e-3f) bad++;
    for(int i=0;i<50;i++){ int r=i/10,c=i%10; int inside=(r>=1&&r<4&&c>=2&&c<6); if(!inside && buf[i]!=-7.f) pad++; }
    CHECK(bad==0,"ROW_LENGTH/SKIP_ROWS/SKIP_PIXELS: valores en su sitio"); CHECK(pad==0,"no escribe fuera del rectangulo (relleno intacto)");
    gl31_glPixelStorei(GL_PACK_ROW_LENGTH,0); gl31_glPixelStorei(GL_PACK_SKIP_PIXELS,0); gl31_glPixelStorei(GL_PACK_SKIP_ROWS,0); gl31_glPixelStorei(GL_PACK_ALIGNMENT,4); }
  { /* alineacion 4 con bytes: 3 px por fila -> stride 4 */
    uint8_t buf[4*4]; memset(buf,0xEE,sizeof buf);
    gl31_glReadPixels(0,0,3,4,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,buf); noerr("align4");
    int bad=0; for(int j=0;j<4;j++){ for(int i=0;i<3;i++) if(buf[j*4+i]!=0x5A) bad++; if(buf[j*4+3]!=0xEE) bad++; }
    CHECK(bad==0,"PACK_ALIGNMENT=4 con filas de 3 bytes (relleno intacto)"); }
  { uint16_t a[W*W],b[W*W]; gl31_glReadPixels(0,0,W,W,GL_DEPTH_COMPONENT,GL_UNSIGNED_SHORT,a);
    gl31_glPixelStorei(GL_PACK_SWAP_BYTES,1); gl31_glReadPixels(0,0,W,W,GL_DEPTH_COMPONENT,GL_UNSIGNED_SHORT,b); noerr("swap");
    int bad=0; for(int i=0;i<W*W;i++) if(b[i]!=(uint16_t)((a[i]>>8)|(a[i]<<8))) bad++;
    CHECK(bad==0,"GL_PACK_SWAP_BYTES intercambia los bytes (depth ushort)");
    uint32_t c[W*W]; gl31_glReadPixels(0,0,W,W,GL_STENCIL_INDEX,GL_UNSIGNED_INT,c);
    CHECK(c[0]==0x5A000000u,"SWAP_BYTES en uint (0x5A -> 0x5A000000)");
    gl31_glPixelStorei(GL_PACK_SWAP_BYTES,0); }
  printf("== PBO\n");
  { GLuint pbo; gl31_glGenBuffers(1,&pbo); gl31_glBindBuffer(GL_PIXEL_PACK_BUFFER,pbo);
    gl31_glBufferData(GL_PIXEL_PACK_BUFFER,W*W*4+64,NULL,GL_STREAM_READ);
    gl31_glReadPixels(0,0,W,W,GL_DEPTH_COMPONENT,GL_FLOAT,(void*)16); noerr("PBO");
    float d[W*W]; gl31_glGetBufferSubData(GL_PIXEL_PACK_BUFFER,16,W*W*4,d);
    int bad=0; for(int j=0;j<W;j++)for(int i=0;i<W;i++) if(fabsf(d[j*W+i]-expd(i,j))>2e-3f) bad++;
    CHECK(bad==0,"depth a PBO con offset 16 (copia GPU->GPU)");
    gl31_glBindBuffer(GL_PIXEL_PACK_BUFFER,0); }
  printf("== estado conservado\n");
  { GLint p=0; gl31_glGetIntegerv(GL_CURRENT_PROGRAM,&p); CHECK((GLuint)p==pr,"programa actual intacto");
    GLint fb=0; gl31_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fb); CHECK((GLuint)fb==fbo,"FBO de dibujo intacto"); }
  printf("== errores\n");
  { gl31_glBindFramebuffer(GL_FRAMEBUFFER,0); }
  { GLuint rb2,f2,t2; gl31_glGenRenderbuffers(1,&rb2); gl31_glBindRenderbuffer(GL_RENDERBUFFER,rb2); gl31_glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,4,4);
    gl31_glGenFramebuffers(1,&f2); gl31_glBindFramebuffer(GL_FRAMEBUFFER,f2); gl31_glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,rb2);
    float d[16]; gl31_glReadPixels(0,0,4,4,GL_DEPTH_COMPONENT,GL_FLOAT,d);
    CHECK(gl31_glGetError()==GL_INVALID_OPERATION,"sin depth buffer -> INVALID_OPERATION");
    gl31_glReadPixels(0,0,4,4,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,d);
    CHECK(gl31_glGetError()==GL_INVALID_OPERATION,"sin stencil buffer -> INVALID_OPERATION");
    gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl31_glReadPixels(0,0,4,4,GL_DEPTH_STENCIL,GL_FLOAT,d);
    CHECK(gl31_glGetError()==GL_INVALID_OPERATION,"DEPTH_STENCIL con tipo FLOAT -> INVALID_OPERATION"); (void)t2; }
  printf("== glClampColor\n");
  { gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl31_glClampColor(GL_CLAMP_READ_COLOR,GL_TRUE); CHECK(gl31_glGetError()==0,"CLAMP_READ_COLOR=TRUE aceptado");
    gl31_glClampColor(GL_CLAMP_READ_COLOR,GL_FALSE); CHECK(gl31_glGetError()==0,"CLAMP_READ_COLOR=FALSE aceptado");
    gl31_glClampColor(GL_CLAMP_READ_COLOR,GL_FIXED_ONLY);
    gl31_glClampColor(0x1234,GL_TRUE); CHECK(gl31_glGetError()==GL_INVALID_ENUM,"target invalido -> INVALID_ENUM");
    gl31_glClampColor(GL_CLAMP_READ_COLOR,0x1234); CHECK(gl31_glGetError()==GL_INVALID_ENUM,"valor invalido -> INVALID_ENUM"); }
  printf("== glReadPixels de color en GPU\n");
  { GLuint tx,fb; unsigned char src[16*4];
    for(int i=0;i<16;i++){src[i*4]=i*16;src[i*4+1]=255-i*16;src[i*4+2]=i*7;src[i*4+3]=200+i;}
    gl31_glGenTextures(1,&tx); gl31_glBindTexture(GL_TEXTURE_2D,tx); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,4,4);
    gl31_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,4,GL_RGBA,GL_UNSIGNED_BYTE,src);
    gl31_glGenFramebuffers(1,&fb); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fb); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tx,0);
    gl31_glPixelStorei(GL_PACK_ALIGNMENT,1);
    #define NRM(c,m) ((unsigned)((c)/255.0*(m)+0.5))
    { unsigned char o[16*4]; gl31_glReadPixels(0,0,4,4,GL_BGRA,GL_UNSIGNED_BYTE,o); noerr("BGRA");
      int bad=0; for(int i=0;i<16;i++) if(o[i*4]!=src[i*4+2]||o[i*4+1]!=src[i*4+1]||o[i*4+2]!=src[i*4]||o[i*4+3]!=src[i*4+3]) bad++; CHECK(bad==0,"BGRA/UNSIGNED_BYTE (swizzle en GPU)"); }
    { unsigned char o[16*3]; gl31_glReadPixels(0,0,4,4,GL_RGB,GL_UNSIGNED_BYTE,o); int bad=0;
      for(int i=0;i<16;i++) if(o[i*3]!=src[i*4]||o[i*3+1]!=src[i*4+1]||o[i*3+2]!=src[i*4+2]) bad++; CHECK(bad==0,"RGB/UNSIGNED_BYTE (3 bytes por pixel)"); }
    { uint16_t o[16*2]; gl31_glReadPixels(0,0,4,4,GL_RG,GL_UNSIGNED_SHORT,o); int bad=0;
      for(int i=0;i<16;i++) if(o[i*2]!=src[i*4]*257||o[i*2+1]!=src[i*4+1]*257) bad++; CHECK(bad==0,"RG/UNSIGNED_SHORT"); }
    { float o[16]; gl31_glReadPixels(0,0,4,4,GL_RED,GL_FLOAT,o); int bad=0;
      for(int i=0;i<16;i++) if(fabsf(o[i]-src[i*4]/255.f)>1e-6f) bad++; CHECK(bad==0,"RED/FLOAT"); }
    { float o[16*4]; gl31_glReadPixels(0,0,4,4,GL_BGR,GL_FLOAT,o); noerr("BGR/FLOAT");
      CHECK(fabsf(o[3*3]-src[3*4+2]/255.f)<1e-6f&&fabsf(o[3*3+2]-src[3*4]/255.f)<1e-6f,"BGR/FLOAT: orden B,G,R"); }
    { uint16_t o[16*4]; gl31_glReadPixels(0,0,4,4,GL_RGBA,GL_HALF_FLOAT,o); noerr("HALF");
      /* 255/255 = 1.0 -> 0x3C00 */
      uint16_t h1=o[0]; CHECK(h1==0,"HALF_FLOAT: pixel 0 rojo = 0"); gl31_glReadPixels(3,3,1,1,GL_RGBA,GL_HALF_FLOAT,o); CHECK(o[0]==0x3B76||o[0]==0x3B77||(o[0]>0x3A00&&o[0]<0x3C00),"HALF_FLOAT: valor intermedio plausible"); }
    { uint16_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,o); noerr("565"); int bad=0;
      for(int i=0;i<16;i++){ unsigned e=(NRM(src[i*4],31)<<11)|(NRM(src[i*4+1],63)<<5)|NRM(src[i*4+2],31); if(o[i]!=e) bad++; } CHECK(bad==0,"UNSIGNED_SHORT_5_6_5"); }
    { uint16_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4,o); noerr("4444"); int bad=0;
      for(int i=0;i<16;i++){ unsigned e=(NRM(src[i*4],15)<<12)|(NRM(src[i*4+1],15)<<8)|(NRM(src[i*4+2],15)<<4)|NRM(src[i*4+3],15); if(o[i]!=e) bad++; } CHECK(bad==0,"UNSIGNED_SHORT_4_4_4_4"); }
    { uint16_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RGBA,GL_UNSIGNED_SHORT_5_5_5_1,o); noerr("5551"); int bad=0;
      for(int i=0;i<16;i++){ unsigned e=(NRM(src[i*4],31)<<11)|(NRM(src[i*4+1],31)<<6)|(NRM(src[i*4+2],31)<<1)|NRM(src[i*4+3],1); if(o[i]!=e) bad++; } CHECK(bad==0,"UNSIGNED_SHORT_5_5_5_1"); }
    { uint32_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RGBA,0x8367,o); int bad=0;
      for(int i=0;i<16;i++){ uint32_t e=src[i*4]|(src[i*4+1]<<8)|(src[i*4+2]<<16)|((uint32_t)src[i*4+3]<<24); if(o[i]!=e) bad++; } CHECK(bad==0,"UNSIGNED_INT_8_8_8_8_REV"); }
    { uint32_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RGBA,0x8035,o); int bad=0;
      for(int i=0;i<16;i++){ uint32_t e=((uint32_t)src[i*4]<<24)|(src[i*4+1]<<16)|(src[i*4+2]<<8)|src[i*4+3]; if(o[i]!=e) bad++; } CHECK(bad==0,"UNSIGNED_INT_8_8_8_8"); }
    { uint32_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RGBA,0x8368,o); noerr("2_10_10_10_REV"); int bad=0;
      for(int i=0;i<16;i++){ uint32_t e=NRM(src[i*4],1023)|(NRM(src[i*4+1],1023)<<10)|(NRM(src[i*4+2],1023)<<20)|(NRM(src[i*4+3],3)<<30); if(o[i]!=e) bad++; } CHECK(bad==0,"UNSIGNED_INT_2_10_10_10_REV"); }
    { int32_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RED,GL_INT,o); int bad=0; for(int i=0;i<16;i++) if(fabs(o[i]/2147483647.0-src[i*4]/255.0)>1e-6) bad++; CHECK(bad==0,"RED/INT normalizado"); }
    { int8_t o[16]; gl31_glReadPixels(0,0,4,4,GL_RED,GL_BYTE,o); CHECK(o[15]==(int8_t)(int)(src[60]/255.0*127+0.5),"RED/BYTE"); }
    { uint16_t a[16],b[16]; gl31_glReadPixels(0,0,4,4,GL_RED,GL_UNSIGNED_SHORT,a); gl31_glPixelStorei(GL_PACK_SWAP_BYTES,1);
      gl31_glReadPixels(0,0,4,4,GL_RED,GL_UNSIGNED_SHORT,b); gl31_glPixelStorei(GL_PACK_SWAP_BYTES,0); int bad=0;
      for(int i=0;i<16;i++) if(b[i]!=(uint16_t)((a[i]>>8)|(a[i]<<8))) bad++; CHECK(bad==0,"SWAP_BYTES en lectura de color"); }
    { unsigned char o[100]; memset(o,0xAA,sizeof o); gl31_glPixelStorei(GL_PACK_ROW_LENGTH,6); gl31_glPixelStorei(GL_PACK_SKIP_PIXELS,1); gl31_glPixelStorei(GL_PACK_SKIP_ROWS,1);
      gl31_glReadPixels(1,1,2,2,GL_RGB,GL_UNSIGNED_BYTE,o); int bad=0,pad=0; unsigned str=18;
      for(int j=0;j<2;j++)for(int i=0;i<2;i++){ int si=((1+j)*4+1+i)*4; unsigned char*d=o+(1+j)*str+(1+i)*3; if(d[0]!=src[si]||d[1]!=src[si+1]||d[2]!=src[si+2]) bad++; }
      for(unsigned i=0;i<100;i++){ unsigned r=i/str,cc=i%str; int in=(r>=1&&r<3&&cc>=3&&cc<9); if(!in&&i<2*str+9+str&&o[i]!=0xAA) pad++; }
      CHECK(bad==0,"ROW_LENGTH/SKIP con RGB"); CHECK(pad==0,"relleno intacto");
      gl31_glPixelStorei(GL_PACK_ROW_LENGTH,0); gl31_glPixelStorei(GL_PACK_SKIP_PIXELS,0); gl31_glPixelStorei(GL_PACK_SKIP_ROWS,0); }
    { GLuint pbo; gl31_glGenBuffers(1,&pbo); gl31_glBindBuffer(GL_PIXEL_PACK_BUFFER,pbo); gl31_glBufferData(GL_PIXEL_PACK_BUFFER,256,NULL,GL_STREAM_READ);
      gl31_glReadPixels(0,0,4,4,GL_BGRA,GL_UNSIGNED_BYTE,(void*)8); noerr("BGRA a PBO"); unsigned char o[64]; gl31_glGetBufferSubData(GL_PIXEL_PACK_BUFFER,8,64,o);
      CHECK(o[0]==src[2]&&o[2]==src[0],"BGRA a PBO con offset"); gl31_glBindBuffer(GL_PIXEL_PACK_BUFFER,0); }
    gl31_glReadPixels(0,0,4,4,0x8D99,GL_UNSIGNED_INT,(void*)0); CHECK(gl31_glGetError()==GL_INVALID_OPERATION,"RGBA_INTEGER sobre buffer normalizado -> INVALID_OPERATION");
    gl31_glPixelStorei(GL_PACK_ALIGNMENT,4); }
  printf("== enteros y flotantes\n");
  { GLuint tu,fu; uint32_t iv[4*4]; for(int i=0;i<4;i++){iv[i*4]=i*100000;iv[i*4+1]=7;iv[i*4+2]=300;iv[i*4+3]=4000000000u;}
    gl31_glGenTextures(1,&tu); gl31_glBindTexture(GL_TEXTURE_2D,tu); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32UI,4,1);
    gl31_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,1,0x8D99,GL_UNSIGNED_INT,iv);
    gl31_glGenFramebuffers(1,&fu); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fu); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tu,0);
    uint32_t o[16]; gl31_glReadPixels(0,0,4,1,0x8D99,GL_UNSIGNED_INT,o); noerr("RGBA_INTEGER/UINT"); CHECK(!memcmp(o,iv,64),"RGBA32UI exacto");
    unsigned char ob[16]; gl31_glReadPixels(0,0,4,1,0x8D99,GL_UNSIGNED_BYTE,ob); CHECK(ob[2]==255&&ob[1]==7,"UINT -> UNSIGNED_BYTE satura a 255");
    uint32_t r1[4]; gl31_glReadPixels(0,0,4,1,0x8D94,GL_UNSIGNED_INT,r1); CHECK(r1[3]==300000u,"RED_INTEGER");
    gl31_glReadPixels(0,0,4,1,GL_RGBA,GL_UNSIGNED_BYTE,ob); CHECK(gl31_glGetError()==GL_INVALID_OPERATION,"formato normalizado sobre buffer entero -> INVALID_OPERATION");
    gl31_glReadPixels(0,0,4,1,0x8D99,GL_FLOAT,o); CHECK(gl31_glGetError()==GL_INVALID_OPERATION,"entero a FLOAT -> INVALID_OPERATION"); }
  { GLuint tf,ff; float fv[4]={2.5f,-1.0f,0.5f,1.0f};
    gl31_glGenTextures(1,&tf); gl31_glBindTexture(GL_TEXTURE_2D,tf); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,1,1);
    gl31_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,fv);
    gl31_glGenFramebuffers(1,&ff); gl31_glBindFramebuffer(GL_FRAMEBUFFER,ff); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tf,0);
    float o[4]; gl31_glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,o); CHECK(o[0]==2.5f&&o[1]==-1.f,"float sin recorte (FIXED_ONLY)");
    gl31_glClampColor(GL_CLAMP_READ_COLOR,GL_TRUE); gl31_glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,o); CHECK(o[0]==1.f&&o[1]==0.f&&o[2]==0.5f,"CLAMP_READ_COLOR=TRUE recorta a [0,1]");
    gl31_glClampColor(GL_CLAMP_READ_COLOR,GL_FIXED_ONLY);
    float v3[4]={1.0f,0.5f,2.0f,1.0f}; gl31_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_RGBA,GL_FLOAT,v3);
    uint32_t p; gl31_glReadPixels(0,0,1,1,GL_RGB,0x8C3B,&p); noerr("10F_11F_11F_REV");
    CHECK(p==(0x3C0u|(0x380u<<11)|(0x200u<<22)),"UNSIGNED_INT_10F_11F_11F_REV (1.0, 0.5, 2.0)");
    uint32_t e; gl31_glReadPixels(0,0,1,1,GL_RGB,0x8C3E,&e); noerr("5_9_9_9_REV");
    { unsigned ex=e>>27; double sc=pow(2.0,(int)ex-15-9); double r=(e&511)*sc,g=((e>>9)&511)*sc,b=((e>>18)&511)*sc;
      CHECK(fabs(r-1.0)<0.01&&fabs(g-0.5)<0.01&&fabs(b-2.0)<0.02,"UNSIGNED_INT_5_9_9_9_REV decodifica a (1, 0.5, 2)"); } }
  gl31_glBindFramebuffer(GL_FRAMEBUFFER,0);
  printf("== subida de texturas BGRA en GPU\n");
  { unsigned char src[6*5*4+16]; for(int i=0;i<(int)sizeof src;i++) src[i]=(unsigned char)(i*7+3);
    GLuint tx,fb; gl31_glGenTextures(1,&tx); gl31_glBindTexture(GL_TEXTURE_2D,tx); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,6,5);
    gl31_glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    gl31_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,6,5,GL_BGRA,GL_UNSIGNED_BYTE,src); noerr("TexSubImage2D BGRA");
    gl31_glGenFramebuffers(1,&fb); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fb); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tx,0);
    gl31_glPixelStorei(GL_PACK_ALIGNMENT,1);
    unsigned char o[6*5*4]; gl31_glReadPixels(0,0,6,5,GL_RGBA,GL_UNSIGNED_BYTE,o); int bad=0;
    for(int i=0;i<30;i++) if(o[i*4]!=src[i*4+2]||o[i*4+1]!=src[i*4+1]||o[i*4+2]!=src[i*4]||o[i*4+3]!=src[i*4+3]) bad++;
    CHECK(bad==0,"BGRA desde memoria: R y B intercambiados");
    /* con ROW_LENGTH / SKIP: region 3x2 dentro de un bloque de 8 de ancho */
    unsigned char big[8*4*4]; for(int i=0;i<(int)sizeof big;i++) big[i]=(unsigned char)(255-i);
    gl31_glPixelStorei(GL_UNPACK_ROW_LENGTH,8); gl31_glPixelStorei(GL_UNPACK_SKIP_PIXELS,2); gl31_glPixelStorei(GL_UNPACK_SKIP_ROWS,1);
    gl31_glTexSubImage2D(GL_TEXTURE_2D,0,1,1,3,2,GL_BGRA,GL_UNSIGNED_BYTE,big); noerr("BGRA con ROW_LENGTH/SKIP");
    gl31_glPixelStorei(GL_UNPACK_ROW_LENGTH,0); gl31_glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); gl31_glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
    gl31_glReadPixels(0,0,6,5,GL_RGBA,GL_UNSIGNED_BYTE,o); bad=0;
    for(int j=0;j<2;j++)for(int i=0;i<3;i++){ const unsigned char*s=big+((1+j)*8+2+i)*4; const unsigned char*d=o+((1+j)*6+1+i)*4; if(d[0]!=s[2]||d[1]!=s[1]||d[2]!=s[0]||d[3]!=s[3]) bad++; }
    CHECK(bad==0,"BGRA con ROW_LENGTH/SKIP_PIXELS/SKIP_ROWS");
    /* desde un PBO con offset sin alinear a 4 */
    GLuint pbo; gl31_glGenBuffers(1,&pbo); gl31_glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo); gl31_glBufferData(GL_PIXEL_UNPACK_BUFFER,sizeof src+8,NULL,GL_STATIC_DRAW);
    gl31_glBufferSubData(GL_PIXEL_UNPACK_BUFFER,3,sizeof src,src);
    gl31_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,6,5,GL_BGRA,GL_UNSIGNED_BYTE,(void*)3); noerr("BGRA desde PBO");
    gl31_glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    gl31_glReadPixels(0,0,6,5,GL_RGBA,GL_UNSIGNED_BYTE,o); bad=0;
    for(int i=0;i<30;i++) if(o[i*4]!=src[i*4+2]||o[i*4+2]!=src[i*4]) bad++;
    CHECK(bad==0,"BGRA desde PIXEL_UNPACK_BUFFER con offset 3 (sin alinear)");
    GLint ub=-1; gl31_glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&ub); CHECK(ub==0,"el PBO de la app queda desenlazado como estaba");
    gl31_glPixelStorei(GL_UNPACK_ALIGNMENT,4); gl31_glPixelStorei(GL_PACK_ALIGNMENT,4); }
  gl31_glBindFramebuffer(GL_FRAMEBUFFER,0);
  printf("== glDrawBuffer(s) con mapeo arbitrario (FBO proxy)\n");
  { GLuint ta,tb,fb,vao; GLuint tx[2]; gl31_glGenTextures(2,tx); ta=tx[0]; tb=tx[1];
    for(int i=0;i<2;i++){ gl31_glBindTexture(GL_TEXTURE_2D,tx[i]); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,4,4); }
    gl31_glGenFramebuffers(1,&fb); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fb);
    gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ta,0); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,tb,0);
    CHECK(gl31_glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"FBO con 2 attachments completo");
    gl31_glViewport(0,0,4,4); gl31_glGenVertexArrays(1,&vao); gl31_glBindVertexArray(vao); gl31_glDisable(GL_DEPTH_TEST); gl31_glDisable(GL_STENCIL_TEST);
    GLuint pr=prog_make("#version 330 core\nconst vec2 P[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));void main(){gl_Position=vec4(P[gl_VertexID],0.0,1.0);}",
                        "#version 330 core\nlayout(location=0) out vec4 a; layout(location=1) out vec4 b; void main(){a=vec4(1.0,0.0,0.0,1.0);b=vec4(0.0,1.0,0.0,1.0);}");
    gl31_glUseProgram(pr);
    unsigned char px[4];
    GLenum all[2]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1}; gl31_glDrawBuffers(2,all);
    gl31_glClearColor(0,0,0,0); gl31_glClear(GL_COLOR_BUFFER_BIT);
    gl31_glDrawBuffer(GL_COLOR_ATTACHMENT1); noerr("DrawBuffer(ATT1)");
    GLint q=0; gl31_glGetIntegerv(GL_DRAW_BUFFER0,&q); CHECK((GLenum)q==GL_COLOR_ATTACHMENT1,"GL_DRAW_BUFFER0 = COLOR_ATTACHMENT1 segun la app");
    gl31_glDrawArrays(GL_TRIANGLES,0,3);
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT1); gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[0]==255&&px[1]==0&&px[3]==255,"salida 0 (rojo) cae en el attachment 1");
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT0); gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[0]==0&&px[1]==0&&px[3]==0,"el attachment 0 no se toca");
    gl31_glClearColor(0,0,1,1); gl31_glClear(GL_COLOR_BUFFER_BIT);
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT1); gl31_glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[2]==255&&px[0]==0,"glClear respeta el mapeo (borra el attachment 1)");
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT0); gl31_glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px); CHECK(px[2]==0,"attachment 0 sin borrar");
    /* mapeo cruzado: salida 0 -> att1, salida 1 -> att0 */
    gl31_glClearColor(0,0,0,0); gl31_glDrawBuffers(2,all); gl31_glClear(GL_COLOR_BUFFER_BIT);
    GLenum sw[2]={GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT0}; gl31_glDrawBuffers(2,sw); noerr("DrawBuffers cruzado");
    gl31_glDrawArrays(GL_TRIANGLES,0,3);
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT0); gl31_glReadPixels(2,2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[1]==255&&px[0]==0,"salida 1 (verde) -> attachment 0");
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT1); gl31_glReadPixels(2,2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[0]==255&&px[1]==0,"salida 0 (rojo) -> attachment 1");
    GLint atype=-1; gl31_glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&atype);
    CHECK((GLuint)atype==tb&&gl31_glGetError()==0,"consultar attachments del FBO de la app (no del proxy)");
    /* cambiar un attachment con el proxy activo se refleja */
    GLuint tc; gl31_glGenTextures(1,&tc); gl31_glBindTexture(GL_TEXTURE_2D,tc); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,4,4);
    gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tc,0);
    gl31_glClearColor(0,0,0,0); gl31_glClear(GL_COLOR_BUFFER_BIT); gl31_glDrawArrays(GL_TRIANGLES,0,3);
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT0); gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[1]==255,"tras re-adjuntar el attachment 0 el proxy dibuja en la textura nueva");
    /* volver a identidad */
    gl31_glDrawBuffers(2,all); gl31_glClear(GL_COLOR_BUFFER_BIT); gl31_glDrawArrays(GL_TRIANGLES,0,3);
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT0); gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px); CHECK(px[0]==255,"identidad: salida 0 -> attachment 0");
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT1); gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px); CHECK(px[1]==255,"identidad: salida 1 -> attachment 1");
    /* volver a enlazar el FBO conserva el mapeo */
    gl31_glDrawBuffer(GL_COLOR_ATTACHMENT1); gl31_glBindFramebuffer(GL_FRAMEBUFFER,0); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fb);
    gl31_glClearColor(0,0,0,0); gl31_glClear(GL_COLOR_BUFFER_BIT); gl31_glDrawArrays(GL_TRIANGLES,0,3);
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT1); gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px); CHECK(px[0]==255,"re-enlazar el FBO conserva el mapeo");
    gl31_glDrawBuffer(GL_COLOR_ATTACHMENT0); gl31_glDeleteFramebuffers(1,&fb); noerr("borrar FBO con proxy"); }
  gl31_glBindFramebuffer(GL_FRAMEBUFFER,0);
  printf("== glGetTexImage: depth y PACK_IMAGE_HEIGHT\n");
  { GLuint td,fb; gl31_glGenTextures(1,&td); gl31_glBindTexture(GL_TEXTURE_2D,td); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_DEPTH_COMPONENT24,4,4);
    gl31_glGenFramebuffers(1,&fb); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fb); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,td,0);
    gl31_glDrawBuffer(GL_NONE); gl31_glReadBuffer(GL_NONE); gl31_glClearDepthf(0.5f); gl31_glClear(GL_DEPTH_BUFFER_BIT);
    gl31_glBindFramebuffer(GL_FRAMEBUFFER,0);
    float d[16]; memset(d,0,sizeof d); gl31_glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,d); noerr("GetTexImage depth");
    int bad=0; for(int i=0;i<16;i++) if(fabsf(d[i]-0.5f)>1e-3f) bad++; CHECK(bad==0,"glGetTexImage de una textura de profundidad (GPU)");
    GLuint t3; gl31_glGenTextures(1,&t3); gl31_glBindTexture(GL_TEXTURE_2D_ARRAY,t3); gl31_glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_RGBA8,2,2,3);
    unsigned char lay[2*2*3*4]; for(int i=0;i<(int)sizeof lay;i++) lay[i]=(unsigned char)i;
    gl31_glPixelStorei(GL_UNPACK_ALIGNMENT,1); gl31_glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,0,2,2,3,GL_RGBA,GL_UNSIGNED_BYTE,lay);
    unsigned char o[2*2*4*6]; memset(o,0xEE,sizeof o); gl31_glPixelStorei(GL_PACK_ALIGNMENT,1); gl31_glPixelStorei(GL_PACK_IMAGE_HEIGHT,3); gl31_glPixelStorei(GL_PACK_SKIP_IMAGES,1);
    gl31_glGetTexImage(GL_TEXTURE_2D_ARRAY,0,GL_RGBA,GL_UNSIGNED_BYTE,o); noerr("GetTexImage array con PACK_IMAGE_HEIGHT");
    /* capa z en offset (1+z)*slice con slice = 3 filas * 8 bytes */
    int b2=0; for(int z=0;z<3;z++) for(int r=0;r<2;r++) if(memcmp(o+(1+z)*24+r*8,lay+z*16+r*8,8)) b2++; CHECK(b2==0,"capas separadas por IMAGE_HEIGHT=3 y empezando en SKIP_IMAGES=1");
    CHECK(o[0]==0xEE&&o[23]==0xEE&&o[16+24*0+8*0+16]==o[16+16],"el relleno entre filas/capas queda intacto");
    gl31_glPixelStorei(GL_PACK_IMAGE_HEIGHT,0); gl31_glPixelStorei(GL_PACK_SKIP_IMAGES,0); gl31_glPixelStorei(GL_PACK_ALIGNMENT,4); gl31_glPixelStorei(GL_UNPACK_ALIGNMENT,4); }
  printf("RESULT: %s (%d fallos)\n",fails?"HAY FALLOS":"TODO OK",fails);
  return fails?1:0;
}
