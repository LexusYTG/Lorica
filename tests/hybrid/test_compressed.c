/* test_compressed.c - glGetCompressedTexImage con la copia (sombra) en GPU */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ctx.h"
#include "gl31.h"
static int fails=0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
#define DXT1 0x83F0
#define DXT5 0x83F3
int main(void){
  if(!ctx_make()) return 1; if(gl31_init(ctx_loader)!=0){printf("init fail\n");return 1;}
  unsigned char d[64]; for(int i=0;i<64;i++) d[i]=(unsigned char)(i*5+1);
  printf("== 2D\n");
  GLuint t; gl31_glGenTextures(1,&t); gl31_glBindTexture(GL_TEXTURE_2D,t);
  gl31_glCompressedTexImage2D(GL_TEXTURE_2D,0,DXT1,8,8,0,32,d);
  if(gl31_glGetError()){ printf("  (sin S3TC: saltado)\n"); return 0; }
  unsigned char o[64]; memset(o,0,sizeof o); gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,o);
  CHECK(gl31_glGetError()==0&&!memcmp(o,d,32),"DXT1 8x8: lectura = lo subido");
  unsigned char blk[8]={200,201,202,203,204,205,206,207};
  gl31_glCompressedTexSubImage2D(GL_TEXTURE_2D,0,4,4,4,4,DXT1,8,blk);
  gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,o); CHECK(!memcmp(o+24,blk,8)&&!memcmp(o,d,24),"SubImage del bloque (1,1) actualiza solo ese bloque");
  gl31_glCompressedTexSubImage2D(GL_TEXTURE_2D,0,4,0,4,4,DXT1,8,blk);
  gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,o); CHECK(!memcmp(o+8,blk,8)&&!memcmp(o,d,8)&&!memcmp(o+16,d+16,8),"SubImage del bloque (1,0): offset de fila correcto");
  gl31_glCompressedTexImage2D(GL_TEXTURE_2D,1,DXT1,4,4,0,8,d+40);
  unsigned char m1[8]; gl31_glGetCompressedTexImage(GL_TEXTURE_2D,1,m1); CHECK(!memcmp(m1,d+40,8),"nivel 1 independiente del 0");
  printf("== PBO\n");
  { GLuint up,pk; gl31_glGenBuffers(1,&up); gl31_glBindBuffer(GL_PIXEL_UNPACK_BUFFER,up); gl31_glBufferData(GL_PIXEL_UNPACK_BUFFER,64,d,GL_STATIC_DRAW);
    GLuint t2; gl31_glGenTextures(1,&t2); gl31_glBindTexture(GL_TEXTURE_2D,t2);
    gl31_glCompressedTexImage2D(GL_TEXTURE_2D,0,DXT1,8,8,0,32,(void*)16);
    gl31_glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    gl31_glGenBuffers(1,&pk); gl31_glBindBuffer(GL_PIXEL_PACK_BUFFER,pk); gl31_glBufferData(GL_PIXEL_PACK_BUFFER,64,NULL,GL_STREAM_READ);
    gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,(void*)8); unsigned char r[32]; gl31_glGetBufferSubData(GL_PIXEL_PACK_BUFFER,8,32,r);
    CHECK(gl31_glGetError()==0&&!memcmp(r,d+16,32),"subida desde PBO (offset 16) y lectura a PBO (offset 8), todo en GPU");
    gl31_glBindBuffer(GL_PIXEL_PACK_BUFFER,0); gl31_glBindTexture(GL_TEXTURE_2D,t); }
  printf("== array / cubo / 3D\n");
  { GLuint ta; gl31_glGenTextures(1,&ta); gl31_glBindTexture(GL_TEXTURE_2D_ARRAY,ta);
    unsigned char lay[3*16]; for(int i=0;i<48;i++) lay[i]=(unsigned char)(255-i);
    gl31_glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY,0,DXT5,4,4,3,0,48,lay); unsigned char r[48]; gl31_glGetCompressedTexImage(GL_TEXTURE_2D_ARRAY,0,r);
    CHECK(!memcmp(r,lay,48),"2D array DXT5 de 3 capas");
    unsigned char nl[16]; for(int i=0;i<16;i++) nl[i]=(unsigned char)(i+100);
    gl31_glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,1,4,4,1,DXT5,16,nl); gl31_glGetCompressedTexImage(GL_TEXTURE_2D_ARRAY,0,r);
    CHECK(!memcmp(r+16,nl,16)&&!memcmp(r,lay,16)&&!memcmp(r+32,lay+32,16),"SubImage3D en la capa 1");
    GLuint tc; gl31_glGenTextures(1,&tc); gl31_glBindTexture(GL_TEXTURE_CUBE_MAP,tc);
    for(int f=0;f<6;f++){ unsigned char fb[16]; memset(fb,f+1,16); gl31_glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,0,DXT5,4,4,0,16,fb); }
    unsigned char c[16]; gl31_glGetCompressedTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,0,c); CHECK(c[0]==4&&c[15]==4,"cubo: la cara -Y guarda su propio contenido"); }
  printf("== TexStorage + SubImage, CopyImageSubData\n");
  { GLuint ts; gl31_glGenTextures(1,&ts); gl31_glBindTexture(GL_TEXTURE_2D,ts); gl31_glTexStorage2D(GL_TEXTURE_2D,2,0x83F2/*DXT3*/,8,8);
    unsigned char b16[16]; for(int i=0;i<16;i++) b16[i]=(unsigned char)(33+i);
    gl31_glCompressedTexSubImage2D(GL_TEXTURE_2D,0,0,4,4,4,0x83F2,16,b16); unsigned char r[64]; gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,r);
    CHECK(gl31_glGetError()==0&&!memcmp(r+32,b16,16),"TexStorage comprimido + SubImage: bloque (0,1) en su sitio (fila 1, offset 32)");
    GLuint td; gl31_glGenTextures(1,&td); gl31_glBindTexture(GL_TEXTURE_2D,td); gl31_glTexStorage2D(GL_TEXTURE_2D,1,0x83F2,8,8);
    gl31_glCopyImageSubData(ts,GL_TEXTURE_2D,0,0,4,0, td,GL_TEXTURE_2D,0,4,0,0, 4,4,1); int e=gl31_glGetError();
    unsigned char r2[64]; memset(r2,0,64); gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,r2);
    CHECK(e==0&&!memcmp(r2+16,b16,16),"CopyImageSubData entre comprimidas copia el bloque a la sombra destino (bloque (1,0))"); }
  printf("== errores\n");
  { GLuint tn; gl31_glGenTextures(1,&tn); gl31_glBindTexture(GL_TEXTURE_2D,tn); gl31_glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA8,4,4);
    unsigned char r[64]; gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,r); CHECK(gl31_glGetError()==GL_INVALID_OPERATION,"textura sin comprimir -> INVALID_OPERATION");
    gl31_glGetCompressedTexImage(GL_TEXTURE_2D,-1,r); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"nivel negativo -> INVALID_VALUE");
    gl31_glGetCompressedTexImage(0x1234,0,r); CHECK(gl31_glGetError()==GL_INVALID_ENUM,"target invalido -> INVALID_ENUM");
    gl31_glDeleteTextures(1,&t); gl31_glBindTexture(GL_TEXTURE_2D,t); gl31_glGetCompressedTexImage(GL_TEXTURE_2D,0,r); gl31_glGetError(); }
  printf("RESULT: %s (%d fallos)\n",fails?"HAY FALLOS":"TODO OK",fails);
  return fails?1:0; }
