/* test_gl4misc.c - viewports/scissors indexados, glClearBufferData con formatos, contadores atomicos. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "ctx.h"
#include "gl31.h"
static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){printf("  FAIL: %s\n",m);fails++;} else printf("  ok: %s\n",m);}while(0)
static void rdbuf(GLuint b,size_t n,void*out){ gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,b); gl31_glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,n,out); }
int main(void){
  if(!ctx_make()) return 1;
  if(gl31_init(ctx_loader)!=0){printf("gl31_init fail\n");return 1;}
  printf("== viewports indexados\n");
  { GLfloat vp[4]={10,20,30,40}, g[4]; GLint sc[4]={1,2,3,4}, gi[4];
    gl31_glViewportIndexedfv(3,vp); CHECK(gl31_glGetError()==0,"ViewportIndexedfv(3) sin error");
    gl31_glGetFloati_v(GL_VIEWPORT,3,g); CHECK(g[0]==10&&g[1]==20&&g[2]==30&&g[3]==40,"GetFloati_v(VIEWPORT,3) devuelve lo fijado");
    gl31_glViewportIndexedf(0,0,0,64,32); gl31_glGetFloati_v(GL_VIEWPORT,0,g); CHECK(g[2]==64&&g[3]==32,"indice 0 reenviado al backend");
    gl31_glGetFloati_v(GL_VIEWPORT,3,g); CHECK(g[0]==10,"el indice 3 no se altera por el 0");
    gl31_glViewportIndexedf(16,0,0,1,1); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"indice 16 -> INVALID_VALUE");
    gl31_glScissorIndexedv(5,sc); CHECK(gl31_glGetError()==0,"ScissorIndexedv(5)");
    gl31_glGetFloati_v(GL_SCISSOR_BOX,5,g); CHECK(g[0]==1&&g[3]==4,"GetFloati_v(SCISSOR_BOX,5)");
    gl31_glEnablei(GL_SCISSOR_TEST,5); CHECK(gl31_glIsEnabledi(GL_SCISSOR_TEST,5)==GL_TRUE,"Enablei(SCISSOR_TEST,5)");
    CHECK(gl31_glIsEnabledi(GL_SCISSOR_TEST,4)==GL_FALSE,"el 4 sigue deshabilitado");
    gl31_glDisablei(GL_SCISSOR_TEST,5); CHECK(gl31_glIsEnabledi(GL_SCISSOR_TEST,5)==GL_FALSE,"Disablei");
    GLdouble dr[2]={0.25,0.75}, gd[2]; gl31_glDepthRangeIndexed(2,dr[0],dr[1]); gl31_glGetDoublei_v(GL_DEPTH_RANGE,2,gd);
    CHECK(gd[0]==0.25&&gd[1]==0.75,"DepthRangeIndexed(2)/GetDoublei_v");
    GLfloat arr[8]={1,2,3,4,5,6,7,8}; gl31_glViewportArrayv(6,2,arr); gl31_glGetFloati_v(GL_VIEWPORT,7,g); CHECK(g[0]==5&&g[3]==8,"ViewportArrayv");
    gl31_glViewportArrayv(15,2,arr); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"ViewportArrayv fuera de rango -> INVALID_VALUE");
    (void)gi; }
  printf("== glClearBufferData\n");
  { GLuint b; gl31_glGenBuffers(1,&b); gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,b); gl31_glBufferData(GL_SHADER_STORAGE_BUFFER,64,NULL,GL_DYNAMIC_COPY);
    unsigned char o[64];
    float f=1.5f; gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_R32F,GL_RED,GL_FLOAT,&f); CHECK(gl31_glGetError()==0,"R32F");
    rdbuf(b,64,o); { float x[16]; memcpy(x,o,64); int bad=0; for(int i=0;i<16;i++) if(x[i]!=1.5f) bad++; CHECK(bad==0,"R32F: 16 floats = 1.5"); }
    float rg[2]={0.5f,2.f}; gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_RG32F,GL_RG,GL_FLOAT,rg); CHECK(gl31_glGetError()==0,"RG32F");
    rdbuf(b,64,o); { float x[16]; memcpy(x,o,64); int bad=0; for(int i=0;i<16;i+=2) if(x[i]!=0.5f||x[i+1]!=2.f) bad++; CHECK(bad==0,"RG32F: patron (0.5,2)"); }
    unsigned char rgba[4]={255,128,0,255}; gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,rgba); CHECK(gl31_glGetError()==0,"RGBA8");
    rdbuf(b,64,o); { int bad=0; for(int i=0;i<64;i+=4) if(o[i]!=255||o[i+1]!=128||o[i+2]!=0||o[i+3]!=255) bad++; CHECK(bad==0,"RGBA8 desde UNSIGNED_BYTE"); }
    float fl[4]={1.f,0.f,0.5f,1.f}; gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_RGBA8,GL_RGBA,GL_FLOAT,fl);
    rdbuf(b,64,o); CHECK(o[0]==255&&o[1]==0&&o[2]==128&&o[3]==255,"RGBA8 desde FLOAT normaliza (1,0,.5,1) -> 255,0,128,255");
    float h[2]={1.f,-2.f}; gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_RG16F,GL_RG,GL_FLOAT,h);
    rdbuf(b,64,o); { uint16_t x[2]; memcpy(x,o,4); CHECK(x[0]==0x3C00&&x[1]==0xC000,"RG16F: 1.0=0x3C00, -2.0=0xC000"); }
    uint32_t u[1]={0xDEADBEEF}; gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT,u);
    rdbuf(b,64,o); { uint32_t x; memcpy(&x,o+60,4); CHECK(x==0xDEADBEEFu,"R32UI con RED_INTEGER"); }
    int16_t s16[2]={-5,300}; gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,0x8239/*RG16I*/,GL_RG_INTEGER,GL_SHORT,s16);
    rdbuf(b,64,o); { int16_t x[2]; memcpy(x,o,4); CHECK(x[0]==-5&&x[1]==300,"RG16I con enteros"); }
    gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,GL_R32F,GL_RED,GL_FLOAT,NULL); rdbuf(b,64,o); { int z=1; for(int i=0;i<64;i++) if(o[i]) z=0; CHECK(z,"data==NULL -> ceros"); }
    float ff=3.f; gl31_glClearBufferSubData(GL_SHADER_STORAGE_BUFFER,GL_R32F,16,16,GL_RED,GL_FLOAT,&ff);
    rdbuf(b,64,o); { float x[16]; memcpy(x,o,64); CHECK(x[3]==0.f&&x[4]==3.f&&x[7]==3.f&&x[8]==0.f,"ClearBufferSubData solo toca [16,32)"); }
    gl31_glClearBufferData(GL_SHADER_STORAGE_BUFFER,0x1234,GL_RED,GL_FLOAT,&f); CHECK(gl31_glGetError()!=0,"formato interno invalido -> error");
    gl31_glClearBufferSubData(GL_SHADER_STORAGE_BUFFER,GL_R32F,2,16,GL_RED,GL_FLOAT,&ff); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"offset no multiplo -> INVALID_VALUE"); }
  printf("== contadores atomicos\n");
  { const char* cs="#version 430\nlayout(local_size_x=1) in;\nlayout(binding=2, offset=0) uniform atomic_uint a;\nlayout(binding=2, offset=4) uniform atomic_uint b;\nvoid main(){atomicCounterIncrement(a);atomicCounterIncrement(b);}";
    GLuint sh=gl31_glCreateShader(GL_COMPUTE_SHADER),pr=gl31_glCreateProgram(); gl31_glShaderSource(sh,1,&cs,NULL); gl31_glCompileShader(sh);
    GLint ok=0; gl31_glGetShaderiv(sh,GL_COMPILE_STATUS,&ok); char log[512]; if(!ok){gl31_glGetShaderInfoLog(sh,512,NULL,log);printf("CS: %s\n",log);}
    gl31_glAttachShader(pr,sh); gl31_glLinkProgram(pr); gl31_glGetProgramiv(pr,GL_LINK_STATUS,&ok); CHECK(ok,"compute con atomic_uint enlaza");
    if(ok){ GLint v=-1;
      gl31_glGetActiveAtomicCounterBufferiv(pr,0,0x92C1,&v); CHECK(v==2,"ATOMIC_COUNTER_BUFFER_BINDING = 2");
      gl31_glGetActiveAtomicCounterBufferiv(pr,0,0x92C4,&v); CHECK(v==8,"DATA_SIZE = 8");
      gl31_glGetActiveAtomicCounterBufferiv(pr,0,0x92C5,&v); CHECK(v==2,"ACTIVE_ATOMIC_COUNTERS = 2");
      GLint idx[2]={-1,-1}; gl31_glGetActiveAtomicCounterBufferiv(pr,0,0x92C6,idx); CHECK(idx[0]>=0&&idx[1]>=0,"ACTIVE_ATOMIC_COUNTER_INDICES validos");
      gl31_glGetActiveAtomicCounterBufferiv(pr,0,0x90ED,&v); CHECK(v==1,"REFERENCED_BY_COMPUTE_SHADER = 1");
      gl31_glGetActiveAtomicCounterBufferiv(pr,0,0x92C7,&v); CHECK(v==0,"REFERENCED_BY_VERTEX_SHADER = 0");
      CHECK(gl31_glGetError()==0,"sin error GL");
      gl31_glGetActiveAtomicCounterBufferiv(pr,1,0x92C1,&v); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"indice fuera de rango -> INVALID_VALUE");
      gl31_glGetActiveAtomicCounterBufferiv(pr,0,0x1234,&v); CHECK(gl31_glGetError()==GL_INVALID_ENUM,"pname invalido -> INVALID_ENUM"); } }
  printf("== glShaderStorageBlockBinding\n");
  { const char* cs="#version 430\nlayout(local_size_x=1) in;\nlayout(std430) buffer A { uint a[]; };\nlayout(std430) buffer B { uint b[]; };\nvoid main(){ a[0]=111u; b[0]=222u; }";
    GLuint sh=gl31_glCreateShader(GL_COMPUTE_SHADER),pr=gl31_glCreateProgram(); gl31_glShaderSource(sh,1,&cs,NULL); gl31_glCompileShader(sh);
    GLint ok=0; char log[512]; gl31_glGetShaderiv(sh,GL_COMPILE_STATUS,&ok); if(!ok){gl31_glGetShaderInfoLog(sh,512,NULL,log);printf("CS: %s\n",log);}
    gl31_glAttachShader(pr,sh); gl31_glLinkProgram(pr); gl31_glGetProgramiv(pr,GL_LINK_STATUS,&ok);
    if(!ok){ gl31_glGetProgramInfoLog(pr,512,NULL,log); printf("LINK: %s\n",log); }
    CHECK(ok,"compute con dos SSBO sin layout(binding) enlaza");
    if(ok){ GLuint ia=gl31_glGetProgramResourceIndex(pr,GL_SHADER_STORAGE_BLOCK,"A"), ib=gl31_glGetProgramResourceIndex(pr,GL_SHADER_STORAGE_BLOCK,"B");
      GLenum prop=GL_BUFFER_BINDING; GLint v=-1; GLsizei len;
      gl31_glGetProgramResourceiv(pr,GL_SHADER_STORAGE_BLOCK,ia,1,&prop,1,&len,&v); CHECK(v==0,"binding inicial de A = 0 (como GL)");
      gl31_glShaderStorageBlockBinding(pr,ia,3); gl31_glShaderStorageBlockBinding(pr,ib,5); CHECK(gl31_glGetError()==0,"ShaderStorageBlockBinding(A->3, B->5) sin error");
      gl31_glGetProgramResourceiv(pr,GL_SHADER_STORAGE_BLOCK,ia,1,&prop,1,&len,&v); CHECK(v==3,"la consulta devuelve el binding fijado por la app");
      GLuint bs[2]; gl31_glGenBuffers(2,bs); unsigned z[4]={0,0,0,0};
      for(int i=0;i<2;i++){ gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bs[i]); gl31_glBufferData(GL_SHADER_STORAGE_BUFFER,16,z,GL_DYNAMIC_COPY); }
      gl31_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,3,bs[0]); gl31_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,5,bs[1]);
      gl31_glUseProgram(pr); gl31_glDispatchCompute(1,1,1); gl31_glMemoryBarrier(0xFFFFFFFFu); CHECK(gl31_glGetError()==0,"dispatch sin error");
      unsigned ra[4]={0},rb[4]={0};
      gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bs[0]); gl31_glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,ra);
      gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bs[1]); gl31_glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,rb);
      CHECK(ra[0]==111,"el bloque A escribio en el buffer del binding 3"); CHECK(rb[0]==222,"el bloque B escribio en el buffer del binding 5");
      GLint64 g=-1; gl31_glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_BINDING,3,&g); CHECK((GLuint)g==bs[0],"GetInteger64i_v(SSBO,3) = lo que enlazo la app");
      gl31_glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_BINDING,0,&g); CHECK(g==0,"el binding 0 de la app sigue vacio");
      /* reasignar: A -> 5, B -> 3 intercambia los buffers */
      unsigned zz[4]={0}; for(int i=0;i<2;i++){ gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bs[i]); gl31_glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,zz); }
      gl31_glShaderStorageBlockBinding(pr,ia,5); gl31_glShaderStorageBlockBinding(pr,ib,3);
      gl31_glDispatchCompute(1,1,1); gl31_glMemoryBarrier(0xFFFFFFFFu);
      gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bs[0]); gl31_glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,ra);
      gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bs[1]); gl31_glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,rb);
      CHECK(ra[0]==222&&rb[0]==111,"reasignar los bloques intercambia los buffers");
      gl31_glShaderStorageBlockBinding(pr,9,0); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"indice de bloque invalido -> INVALID_VALUE");
      gl31_glShaderStorageBlockBinding(pr,0,99); CHECK(gl31_glGetError()==GL_INVALID_VALUE,"binding fuera de rango -> INVALID_VALUE");
      /* un programa sin remap, con binding explicito, no se ve afectado */
      const char* cs2="#version 430\nlayout(local_size_x=1) in;\nlayout(std430, binding=2) buffer C { uint c[]; };\nvoid main(){ c[0]=7u; }";
      GLuint s2=gl31_glCreateShader(GL_COMPUTE_SHADER),p2=gl31_glCreateProgram(); gl31_glShaderSource(s2,1,&cs2,NULL); gl31_glCompileShader(s2);
      gl31_glAttachShader(p2,s2); gl31_glLinkProgram(p2); GLuint bc; gl31_glGenBuffers(1,&bc); gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bc);
      gl31_glBufferData(GL_SHADER_STORAGE_BUFFER,16,zz,GL_DYNAMIC_COPY); gl31_glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,bc);
      gl31_glUseProgram(p2); gl31_glDispatchCompute(1,1,1); gl31_glMemoryBarrier(0xFFFFFFFFu);
      unsigned rc[4]={0}; gl31_glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,rc); CHECK(rc[0]==7,"binding explicito (2) sigue funcionando tras el remap del otro programa");
      gl31_glUseProgram(pr); gl31_glBindBuffer(GL_SHADER_STORAGE_BUFFER,bs[0]); gl31_glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,zz);
      gl31_glDispatchCompute(1,1,1); gl31_glMemoryBarrier(0xFFFFFFFFu);
      gl31_glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,16,ra); CHECK(ra[0]==222,"volver al programa remapeado vuelve a resolver los bloques"); } }
  printf("RESULT: %s (%d fallos)\n",fails?"HAY FALLOS":"TODO OK",fails);
  return fails?1:0;
}
