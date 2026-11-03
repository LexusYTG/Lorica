/* test_fallback.c - rutas de respaldo cuando el backend NO tiene ES 3.1/3.2 ni extensiones:
 * se apagan capacidades con LORICA_DISABLE_CAPS y se comprueba que las funciones siguen funcionando. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ctx.h"
#include "gl31.h"
static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
static GLuint prog_make(const char*vs,const char*fs){
  GLuint v=gl31_glCreateShader(GL_VERTEX_SHADER),f=gl31_glCreateShader(GL_FRAGMENT_SHADER),p=gl31_glCreateProgram();
  gl31_glShaderSource(v,1,&vs,NULL);gl31_glCompileShader(v);gl31_glShaderSource(f,1,&fs,NULL);gl31_glCompileShader(f);
  GLint ok=0;char log[1024];
  gl31_glGetShaderiv(v,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(v,1024,NULL,log);printf("VS: %s\n",log);return 0;}
  gl31_glGetShaderiv(f,GL_COMPILE_STATUS,&ok);if(!ok){gl31_glGetShaderInfoLog(f,1024,NULL,log);printf("FS: %s\n",log);return 0;}
  gl31_glAttachShader(p,v);gl31_glAttachShader(p,f);gl31_glLinkProgram(p);
  gl31_glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){gl31_glGetProgramInfoLog(p,1024,NULL,log);printf("LINK: %s\n",log);return 0;}
  return p;}
int main(void){
  setenv("LORICA_DISABLE_CAPS","es31,copy_image,sample_shading,draw_buf_indexed,multisample_tex",1);
  if(!ctx_make()) return 1;
  if(gl31_init(ctx_loader)!=0){printf("gl31_init fail\n");return 1;}
  printf("== atributos con binding separado, sin ES 3.1\n");
  { GLuint tex,fbo,vao,vb,vc; gl31_glGenTextures(1,&tex); gl31_glBindTexture(GL_TEXTURE_2D,tex); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,8,8);
    gl31_glGenFramebuffers(1,&fbo); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    gl31_glViewport(0,0,8,8);
    GLuint pr=prog_make("#version 330 core\nlayout(location=0) in vec2 p; layout(location=1) in vec4 c; out vec4 vc;\nvoid main(){vc=c;gl_Position=vec4(p,0.0,1.0);}",
                        "#version 330 core\nin vec4 vc; out vec4 o; void main(){o=vc;}");
    gl31_glUseProgram(pr);
    gl31_glGenVertexArrays(1,&vao); gl31_glBindVertexArray(vao);
    float pos[6]={-1,-1, 3,-1, -1,3}; float col[12]={0,1,0,1, 1,0,0,1, 0,0,1,1};
    gl31_glGenBuffers(1,&vb); gl31_glBindBuffer(GL_ARRAY_BUFFER,vb); gl31_glBufferData(GL_ARRAY_BUFFER,sizeof pos,pos,GL_STATIC_DRAW);
    gl31_glGenBuffers(1,&vc); gl31_glBindBuffer(GL_ARRAY_BUFFER,vc); gl31_glBufferData(GL_ARRAY_BUFFER,sizeof col,col,GL_STATIC_DRAW);
    gl31_glBindBuffer(GL_ARRAY_BUFFER,0);
    gl31_glVertexAttribFormat(0,2,GL_FLOAT,GL_FALSE,0); gl31_glVertexAttribBinding(0,0); gl31_glBindVertexBuffer(0,vb,0,8);
    gl31_glVertexAttribFormat(1,4,GL_FLOAT,GL_FALSE,0);                              /* binding por defecto = 1 */
    gl31_glBindVertexBuffer(1,vc,0,0);                                                /* stride 0: siempre el elemento 0 */
    gl31_glEnableVertexAttribArray(0); gl31_glEnableVertexAttribArray(1);
    GLenum e=gl31_glGetError(); CHECK(e==0,"Format/Binding/BindVertexBuffer sin error");
    gl31_glClearColor(0,0,0,0); gl31_glClear(GL_COLOR_BUFFER_BIT); gl31_glDrawArrays(GL_TRIANGLES,0,3);
    unsigned char px[4]; gl31_glReadPixels(4,4,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[0]==0&&px[1]==255&&px[2]==0&&px[3]==255,"stride 0 -> todos los vertices leen el color 0 (verde)");
    gl31_glBindVertexBuffer(1,vc,0,16);                                               /* ahora interleaved por vertice */
    gl31_glClear(GL_COLOR_BUFFER_BIT); gl31_glDrawArrays(GL_TRIANGLES,0,3);
    gl31_glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[1]>150&&px[0]<100,"stride 16 -> color interpolado desde el vertice 0 (verde dominante en la esquina)");
    gl31_glGetError();
    gl31_glVertexBindingDivisor(1,1); CHECK(gl31_glGetError()==0,"VertexBindingDivisor sin error");
    gl31_glVertexAttribBinding(1,40); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"binding fuera de rango -> INVALID_VALUE"); }
  printf("== glCopyImageSubData sin ES 3.2\n");
  { GLuint a,b; gl31_glGenTextures(1,&a); gl31_glBindTexture(GL_TEXTURE_2D,a); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,8,8);
    unsigned char src[8*8*4]; for(int i=0;i<64;i++){src[i*4]=i;src[i*4+1]=i*2;src[i*4+2]=255-i;src[i*4+3]=255;}
    gl31_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,8,8,GL_RGBA,GL_UNSIGNED_BYTE,src);
    gl31_glGenTextures(1,&b); gl31_glBindTexture(GL_TEXTURE_2D,b); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,8,8);
    gl31_glCopyImageSubData(a,GL_TEXTURE_2D,0,2,2,0, b,GL_TEXTURE_2D,0,4,4,0, 4,4,1);
    CHECK(gl31_glGetError()==0,"CopyImageSubData 2D sin error");
    GLuint fbo; gl31_glGenFramebuffers(1,&fbo); gl31_glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo); gl31_glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,b,0);
    unsigned char out[8*8*4]; memset(out,0,sizeof out); gl31_glReadPixels(0,0,8,8,GL_RGBA,GL_UNSIGNED_BYTE,out);
    int bad=0; for(int y=0;y<4;y++)for(int x=0;x<4;x++){ int si=((y+2)*8+(x+2))*4, di=((y+4)*8+(x+4))*4; if(memcmp(src+si,out+di,4)) bad++; }
    CHECK(bad==0,"los 16 texels copiados coinciden"); CHECK(out[0]==0&&out[3]==0,"fuera de la region no se toca");
    GLuint l; gl31_glGenTextures(1,&l); gl31_glBindTexture(GL_TEXTURE_2D_ARRAY,l); gl31_glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_RGBA8,4,4,3);
    gl31_glCopyImageSubData(a,GL_TEXTURE_2D,0,0,0,0, l,GL_TEXTURE_2D_ARRAY,0,0,0,2, 4,4,1);
    CHECK(gl31_glGetError()==0,"copia a la capa 2 de un array");
    gl31_glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo); gl31_glFramebufferTextureLayer(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,l,0,2);
    unsigned char o2[4*4*4]; gl31_glReadPixels(0,0,4,4,GL_RGBA,GL_UNSIGNED_BYTE,o2);
    bad=0; for(int y=0;y<4;y++)for(int x=0;x<4;x++) if(memcmp(src+(y*8+x)*4,o2+(y*4+x)*4,4)) bad++;
    CHECK(bad==0,"contenido de la capa 2 correcto");
    gl31_glCopyImageSubData(a,GL_TEXTURE_2D,0,0,0,0, b,GL_TEXTURE_2D,0,0,0,0, -1,4,1);
    CHECK(gl31_glGetError()==GL_INVALID_VALUE,"ancho negativo -> INVALID_VALUE"); }
  printf("== tamano de imagen comprimida\n");
  { GLuint t; gl31_glGenTextures(1,&t); gl31_glBindTexture(GL_TEXTURE_2D,t); unsigned char blk[128]={0};
    gl31_glCompressedTexImage2D(GL_TEXTURE_2D,0,0x83F0,8,8,0,32,blk);
    if(gl31_glGetError()==0){ GLint sz=-1; gl31_glGetTexLevelParameteriv(GL_TEXTURE_2D,0,0x86A0,&sz); CHECK(sz==32,"DXT1 8x8 -> 32 bytes");
      gl31_glCompressedTexImage2D(GL_TEXTURE_2D,0,0x83F3,10,6,0,96,blk); gl31_glGetError();
      gl31_glGetTexLevelParameteriv(GL_TEXTURE_2D,0,0x86A0,&sz); CHECK(sz==96,"DXT5 10x6 -> 3x2 bloques x 16 = 96"); }
    else printf("  (saltado: sin S3TC en este backend)\n"); }
  printf("== sample shading / muestras\n");
  { gl31_glEnable(GL_SAMPLE_SHADING); CHECK(gl31_glIsEnabled(GL_SAMPLE_SHADING)==GL_TRUE,"Enable(SAMPLE_SHADING) se registra");
    gl31_glMinSampleShading(0.5f); GLfloat f=0; gl31_glGetFloatv(0x8C37,&f); CHECK(f==0.5f,"MIN_SAMPLE_SHADING_VALUE = 0.5");
    gl31_glDisable(GL_SAMPLE_SHADING); CHECK(gl31_glIsEnabled(GL_SAMPLE_SHADING)==GL_FALSE,"Disable(SAMPLE_SHADING)");
    GLfloat p[2]={-1,-1}; gl31_glGetMultisamplefv(GL_SAMPLE_POSITION,0,p); CHECK(gl31_glGetError()==0&&p[0]==0.5f&&p[1]==0.5f,"GetMultisamplefv(0) = (0.5, 0.5) con 1 muestra");
    gl31_glGetMultisamplefv(GL_SAMPLE_POSITION,3,p); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"indice >= muestras -> INVALID_VALUE");
    gl31_glSampleMaski(0,0xFFFFFFFFu); CHECK(gl31_glGetError()==0,"SampleMaski(todo) aceptado"); }
  printf("== samplers: color de borde\n");
  { GLuint s; gl31_glGenSamplers(1,&s); GLint iv[4]={1,2,3,4},g[4]={0}; gl31_glSamplerParameterIiv(s,0x1004,iv);
    CHECK(gl31_glGetError()==0,"SamplerParameterIiv(BORDER_COLOR) sin error"); gl31_glGetSamplerParameterIiv(s,0x1004,g);
    CHECK(g[0]==1&&g[1]==2&&g[2]==3&&g[3]==4,"Get...Iiv devuelve lo fijado");
    GLuint uv[4]={10,20,30,40},gu[4]={0}; gl31_glSamplerParameterIuiv(s,0x1004,uv); gl31_glGetSamplerParameterIuiv(s,0x1004,gu);
    CHECK(gu[0]==10&&gu[3]==40,"Iuiv ida y vuelta");
    GLfloat fv[4]={0.25f,0.5f,0.75f,1.f},gf[4]; gl31_glSamplerParameterfv(s,0x1004,fv); gl31_glGetSamplerParameterfv(s,0x1004,gf);
    CHECK(gf[1]==0.5f&&gf[3]==1.f,"float ida y vuelta"); }
  printf("== blend / colormask por buffer sin draw_buffers_indexed\n");
  { gl31_glEnablei(GL_BLEND,0); CHECK(gl31_glGetError()==0&&gl31_glIsEnabledi(GL_BLEND,0)==GL_TRUE,"Enablei(BLEND,0)");
    gl31_glDisablei(GL_BLEND,0); CHECK(gl31_glIsEnabledi(GL_BLEND,0)==GL_FALSE,"Disablei(BLEND,0)");
    gl31_glColorMaski(0,GL_TRUE,GL_FALSE,GL_TRUE,GL_TRUE); CHECK(gl31_glGetError()==0,"ColorMaski(0) sin error");
    gl31_glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); }
  printf("== MRT: blend solo en el buffer 1 (primer buffer activo)\n");
  { GLuint t0,t1,fbo,vao; gl31_glGenTextures(1,&t0); gl31_glBindTexture(GL_TEXTURE_2D,t0); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,4,4);
    gl31_glGenTextures(1,&t1); gl31_glBindTexture(GL_TEXTURE_2D,t1); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,4,4);
    gl31_glGenFramebuffers(1,&fbo); gl31_glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t0,0); gl31_glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,t1,0);
    gl31_glViewport(0,0,4,4);
    GLuint pr=prog_make("#version 330 core\nconst vec2 P[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));void main(){gl_Position=vec4(P[gl_VertexID],0.0,1.0);}",
                        "#version 330 core\nlayout(location=0) out vec4 a; layout(location=1) out vec4 b; void main(){a=vec4(0.25,0.0,0.0,1.0);b=vec4(0.25,0.0,0.0,1.0);}");
    gl31_glUseProgram(pr); gl31_glGenVertexArrays(1,&vao); gl31_glBindVertexArray(vao);
    GLenum db[2]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1}; gl31_glDrawBuffers(2,db);
    GLfloat c[4]={0.25f,0,0,0}; gl31_glClearBufferfv(GL_COLOR,0,c); gl31_glClearBufferfv(GL_COLOR,1,c);
    gl31_glEnablei(GL_BLEND,1); gl31_glBlendFunci(1,GL_ONE,GL_ONE);
    GLenum db2[2]={GL_NONE,GL_COLOR_ATTACHMENT1}; gl31_glDrawBuffers(2,db2);
    gl31_glDrawArrays(GL_TRIANGLES,0,3);
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT1); unsigned char px[4]; gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[0]>=125&&px[0]<=131,"buffer 1: 0.25 + 0.25 con BlendFunci(1,ONE,ONE) = ~0.5");
    gl31_glReadBuffer(GL_COLOR_ATTACHMENT0); gl31_glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    CHECK(px[0]>=62&&px[0]<=66,"buffer 0 (sin dibujar) intacto ~0.25");
    gl31_glDisablei(GL_BLEND,1); gl31_glGetError(); }
  printf("RESULT: %s (%d fallos)\n",fails?"HAY FALLOS":"TODO OK",fails);
  return fails?1:0;
}
