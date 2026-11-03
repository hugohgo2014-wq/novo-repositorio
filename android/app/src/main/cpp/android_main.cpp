// GDevelop C++ Native Editor — Android NativeActivity + EGL + OpenGL ES 3.0.
// Toda a interface é desenhada por shaders OpenGL; não existe WebView/HTML/JS.
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/input.h>
#include <android_native_app_glue.h>
#include <time.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "gdcpp/project.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "gdcpp-editor", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "gdcpp-editor", __VA_ARGS__)

namespace {
struct Editor {
 android_app* app=nullptr; EGLDisplay display=EGL_NO_DISPLAY; EGLSurface surface=EGL_NO_SURFACE; EGLContext context=EGL_NO_CONTEXT;
 int w=0,h=0; bool active=false; GLuint program=0,vao=0,vbo=0; GLint colorLoc=-1; gdcpp::Project project; int scene=0, selected=-1; float downX=0,downY=0; bool dirty=false; std::string status="Projeto carregado";
};
GLuint shader(GLenum type,const char* src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[512];glGetShaderInfoLog(s,512,nullptr,log);LOGE("shader: %s",log);glDeleteShader(s);return 0;}return s;}
bool initGL(Editor& e){e.display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(e.display==EGL_NO_DISPLAY||!eglInitialize(e.display,nullptr,nullptr))return false;const EGLint a[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};EGLConfig c;EGLint n=0;if(!eglChooseConfig(e.display,a,&c,1,&n)||!n)return false;e.surface=eglCreateWindowSurface(e.display,c,e.app->window,nullptr);const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};e.context=eglCreateContext(e.display,c,EGL_NO_CONTEXT,ca);if(e.surface==EGL_NO_SURFACE||e.context==EGL_NO_CONTEXT||!eglMakeCurrent(e.display,e.surface,e.surface,e.context))return false;eglQuerySurface(e.display,e.surface,EGL_WIDTH,&e.w);eglQuerySurface(e.display,e.surface,EGL_HEIGHT,&e.h);eglSwapInterval(e.display,1);
 const char* vs="#version 300 es\nlayout(location=0) in vec2 p; uniform vec4 rect; void main(){vec2 q=rect.xy+p*rect.zw;gl_Position=vec4(q,0,1);}";
 const char* fs="#version 300 es\nprecision mediump float;uniform vec4 color;out vec4 o;void main(){o=color;}";
 GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);e.program=glCreateProgram();glAttachShader(e.program,v);glAttachShader(e.program,f);glLinkProgram(e.program);glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(e.program,GL_LINK_STATUS,&ok);if(!ok)return false;e.colorLoc=glGetUniformLocation(e.program,"color");glGenVertexArrays(1,&e.vao);glBindVertexArray(e.vao);glGenBuffers(1,&e.vbo);glBindBuffer(GL_ARRAY_BUFFER,e.vbo);const float quad[]={0,0,1,0,1,1,0,0,1,1,0,1};glBufferData(GL_ARRAY_BUFFER,sizeof quad,quad,GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);return true;}
void rect(Editor&e,float x,float y,float w,float h,float r,float g,float b){float nx=2*x/e.w-1,ny=1-2*y/e.h,nw=2*w/e.w,nh=2*h/e.h;GLint loc=glGetUniformLocation(e.program,"rect");glUniform4f(loc,nx,ny-nh,nw,nh);glUniform4f(e.colorLoc,r,g,b,1);glDrawArrays(GL_TRIANGLES,0,6);}
// Fonte vetorial 5x7 embutida: caracteres são pequenos blocos OpenGL, sem bitmap/HTML.
const unsigned char* glyph(char c){static const unsigned char blank[7]={0};static const unsigned char digits[10][7]={{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,17,14},{2,6,10,18,31,2,2},{31,16,16,30,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,2,12}};static const unsigned char upper[26][7]={{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{28,18,17,17,17,18,28},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};if(c>='0'&&c<='9')return digits[c-'0'];if(c>='a'&&c<='z')c-=32;if(c>='A'&&c<='Z')return upper[c-'A'];return blank;}
void text(Editor&e,float x,float y,const std::string&s,float scale,float r=0.88f,float g=0.91f,float b=0.96f){float ox=x;for(char c:s){if(c=='\n'){y+=9*scale;x=ox;continue;}auto bits=glyph(c);for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(bits[row]&(1<<(4-col)))rect(e,x+col*scale,y+row*scale,scale*.82f,scale*.82f,r,g,b);x+=6*scale;}}
void button(Editor&e,float x,float y,float w,float h,const std::string&label,bool active=false){rect(e,x,y,w,h,active?.12f:.16f,active?.42f:.20f,active?.78f:.27f);text(e,x+9,y+h*.32f,label,1.45f);}
std::string num(double v){char b[40];std::snprintf(b,sizeof b,"%.0f",v);return b;}
void render(Editor&e){glViewport(0,0,e.w,e.h);glDisable(GL_DEPTH_TEST);glClearColor(.055f,.067f,.09f,1);glClear(GL_COLOR_BUFFER_BIT);glUseProgram(e.program);glBindVertexArray(e.vao);
 const float s=std::min(e.w/800.f,e.h/450.f);auto X=[&](float v){return v*s;};
 // Top bar
 rect(e,0,0,e.w,X(48),.075f,.09f,.13f);text(e,X(16),X(16),"GDEVELOP C++ EDITOR",X(1.45f),.35f,.78f,1);button(e,e.w-X(180),X(7),X(78),X(34),"SALVAR",e.dirty);button(e,e.w-X(94),X(7),X(84),X(34),"PLAY");
 // Left scene/object hierarchy
 float left=X(174),top=X(58),bottom=e.h-X(12);rect(e,0,top,left,bottom-top,.09f,.105f,.14f);text(e,X(12),X(68),"CENAS",X(1.25f),.45f,.72f,1);float yy=X(91);for(size_t i=0;i<e.project.layouts.size()&&i<8;i++){button(e,X(8),yy,left-X(16),X(28),e.project.layouts[i].name,(int)i==e.scene);yy+=X(32);}text(e,X(12),yy+X(8),"OBJETOS",X(1.25f),.45f,.72f,1);yy+=X(30);if(!e.project.layouts.empty()){auto&L=e.project.layouts[e.scene];for(size_t i=0;i<L.objects.size()&&i<12;i++){text(e,X(16),yy,L.objects[i].name,X(1.05f),.78f,.82f,.9f);yy+=X(20);}button(e,X(8),bottom-X(78),X(74),X(30),"+ OBJ");button(e,X(88),bottom-X(78),X(78),X(30),"+ INST");}
 // Canvas and grid
 float cx=left+X(10),cy=top+X(8),right=e.w-X(205),cw=right-cx,ch=bottom-cy-X(8);rect(e,cx,cy,cw,ch,.12f,.14f,.18f);if(!e.project.layouts.empty()){auto&L=e.project.layouts[e.scene];float scale=std::min(cw/(float)std::max(1,e.project.windowWidth),ch/(float)std::max(1,e.project.windowHeight));float gw=e.project.windowWidth*scale,gh=e.project.windowHeight*scale,gx=cx+(cw-gw)/2,gy=cy+(ch-gh)/2;rect(e,gx,gy,gw,gh,L.bgR/255.f,L.bgG/255.f,L.bgB/255.f);
 for(int i=0;i<=e.project.windowWidth;i+=50){float px=gx+i*scale;rect(e,px,gy,1,gh,.55f,.58f,.62f);}for(int i=0;i<=e.project.windowHeight;i+=50){float py=gy+i*scale;rect(e,gx,py,gw,1,.55f,.58f,.62f);}
 for(size_t i=0;i<L.instances.size();i++){auto&in=L.instances[i];float iw=std::max(12.0,(in.width?in.width:64)*scale),ih=std::max(12.0,(in.height?in.height:64)*scale);float ix=gx+in.x*scale,iy=gy+in.y*scale;rect(e,ix,iy,iw,ih,(int)i==e.selected?1.f:.22f,(int)i==e.selected?.55f:.72f,(int)i==e.selected?.12f:.92f);if((int)i==e.selected){rect(e,ix-2,iy-2,iw+4,2,1,1,.3f);rect(e,ix-2,iy+ih,iw+4,2,1,1,.3f);text(e,ix,iy-13,in.name,1.0f);}}
 }
 // Inspector
 float ix=e.w-X(192);rect(e,ix,top,X(192),bottom-top,.09f,.105f,.14f);text(e,ix+X(12),X(68),"INSPETOR",X(1.25f),.45f,.72f,1);if(!e.project.layouts.empty()&&e.selected>=0&&e.selected<(int)e.project.layouts[e.scene].instances.size()){auto&in=e.project.layouts[e.scene].instances[e.selected];float y=X(100);text(e,ix+X(12),y,"INSTANCIA",X(1.1f));y+=X(27);text(e,ix+X(12),y,in.name,X(1.2f),.4f,.85f,1);y+=X(34);text(e,ix+X(12),y,"POSICAO X  "+num(in.x),X(1.0f));y+=X(23);text(e,ix+X(12),y,"POSICAO Y  "+num(in.y),X(1.0f));y+=X(23);text(e,ix+X(12),y,"LARGURA    "+num(in.width),X(1.0f));y+=X(23);text(e,ix+X(12),y,"ALTURA     "+num(in.height),X(1.0f));y+=X(35);button(e,ix+X(10),y,X(170),X(30),"MOVER -10 X");y+=X(36);button(e,ix+X(10),y,X(170),X(30),"MOVER +10 X");y+=X(36);button(e,ix+X(10),y,X(170),X(30),"EXCLUIR INST.");}else{text(e,ix+X(12),X(108),"Selecione uma",X(1.05f));text(e,ix+X(12),X(126),"instancia no palco",X(1.05f));}
 rect(e,0,e.h-X(10),e.w,X(10),e.dirty?.9f:.12f,e.dirty?.55f:.5f,e.dirty?.12f:.2f);text(e,X(12),e.h-X(28),e.status,X(1.0f),.7f,.75f,.82f);
}
void saveProject(Editor&e){ // Persistência nativa: mantém uma cópia-fonte e grava alterações estruturais via arquivo de estado do editor.
 std::string path=e.app->activity->internalDataPath;path+="/editor-session.txt";std::ofstream f(path);if(!f){e.status="Falha ao gravar sessao";return;}f<<"GDC++ editor session\nscene="<<e.scene<<"\n";if(!e.project.layouts.empty()){auto&l=e.project.layouts[e.scene];for(auto&i:l.instances)f<<i.name<<","<<i.x<<","<<i.y<<","<<i.width<<","<<i.height<<"\n";}e.dirty=false;e.status="Snapshot da sessao salvo; exportacao JSON pendente";}
void touch(Editor&e,float x,float y){float s=std::min(e.w/800.f,e.h/450.f),left=174*s,top=58*s,bottom=e.h-12*s;float right=e.w-205*s;if(y<48*s&&x>e.w-190*s&&x<e.w-95*s){saveProject(e);return;}if(e.project.layouts.empty())return;auto&L=e.project.layouts[e.scene];if(x<left&&y>top){float row=(y-91*s)/(32*s);if(row>=0&&row<(float)e.project.layouts.size()){e.scene=(int)row;e.selected=-1;e.status="Cena: "+e.project.layouts[e.scene].name;return;}if(y>bottom-82*s&&x<82*s){if(!L.objects.empty()){auto d=L.objects.front();d.name+="_Novo";L.objects.push_back(d);e.dirty=true;e.status="Objeto criado";}return;}if(y>bottom-82*s&&x<170*s){if(!L.objects.empty()){gdcpp::InstanceDef in;in.name=L.objects.front().name;in.layer=L.layers.empty()?"":L.layers.front().name;in.x=100;in.y=100;in.width=64;in.height=64;L.instances.push_back(in);e.selected=(int)L.instances.size()-1;e.dirty=true;e.status="Instancia criada";}return;}}
 float ix=e.w-192*s;if(x>ix&&e.selected>=0&&e.selected<(int)L.instances.size()){auto&in=L.instances[e.selected];float ry=y/s;if(ry>300&&ry<345){in.x-=10;e.dirty=true;}else if(ry>345&&ry<385){in.x+=10;e.dirty=true;}else if(ry>385&&ry<430){L.instances.erase(L.instances.begin()+e.selected);e.selected=-1;e.dirty=true;}return;}
 if(x>=left&&x<right&&y>top&&y<bottom){float cx=left+10*s,cy=top+8*s,cw=right-cx,ch=bottom-cy-8*s;float scale=std::min(cw/(float)std::max(1,e.project.windowWidth),ch/(float)std::max(1,e.project.windowHeight));float gw=e.project.windowWidth*scale,gh=e.project.windowHeight*scale,gx=cx+(cw-gw)/2,gy=cy+(ch-gh)/2;for(int i=(int)L.instances.size()-1;i>=0;i--){auto&in=L.instances[i];float w=std::max(12.0,(in.width?in.width:64)*scale),h=std::max(12.0,(in.height?in.height:64)*scale);float ox=gx+in.x*scale,oy=gy+in.y*scale;if(x>=ox-5&&x<=ox+w+5&&y>=oy-5&&y<=oy+h+5){e.selected=i;e.status="Selecionado: "+in.name;return;}}if(e.selected>=0&&e.selected<(int)L.instances.size()){auto&in=L.instances[e.selected];in.x=std::max(0.0,(x-gx)/scale);in.y=std::max(0.0,(y-gy)/scale);e.dirty=true;e.status="Instancia movida";}}
}
bool loadProject(Editor&e){AAsset*a=AAssetManager_open(e.app->activity->assetManager,"game/project.json",AASSET_MODE_BUFFER);if(!a)return false;off_t n=AAsset_getLength(a);std::string src((size_t)n,'\0');AAsset_read(a,src.data(),n);AAsset_close(a);std::string err;if(!e.project.load(src,err)){LOGE("project: %s",err.c_str());return false;}return true;}
void term(Editor&e){if(e.display!=EGL_NO_DISPLAY){eglMakeCurrent(e.display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(e.context!=EGL_NO_CONTEXT)eglDestroyContext(e.display,e.context);if(e.surface!=EGL_NO_SURFACE)eglDestroySurface(e.display,e.surface);eglTerminate(e.display);}e.display=EGL_NO_DISPLAY;e.context=EGL_NO_CONTEXT;e.surface=EGL_NO_SURFACE;}
void cmd(android_app*a,int32_t c){auto&e=*(Editor*)a->userData;if(c==APP_CMD_INIT_WINDOW&&a->window){if(initGL(e)){e.active=true;e.status="Editor OpenGL ES 3.0 pronto";}}else if(c==APP_CMD_TERM_WINDOW){e.active=false;term(e);}}
int32_t input(android_app*a,AInputEvent*ev){auto&e=*(Editor*)a->userData;if(AInputEvent_getType(ev)!=AINPUT_EVENT_TYPE_MOTION)return 0;int act=AMotionEvent_getAction(ev)&AMOTION_EVENT_ACTION_MASK;float x=AMotionEvent_getX(ev,0),y=AMotionEvent_getY(ev,0);if(act==AMOTION_EVENT_ACTION_UP)touch(e,x,y);return 1;}
}
void android_main(android_app*app){app_dummy();Editor e;e.app=app;app->userData=&e;app->onAppCmd=cmd;app->onInputEvent=input;if(!loadProject(e))e.status="Falha ao abrir game/project.json";while(true){int events;android_poll_source*src;while(ALooper_pollOnce(e.active?0:-1,nullptr,&events,(void**)&src)>=0){if(src)src->process(app,src);if(app->destroyRequested){term(e);return;}}if(!e.active)continue;if(e.w!=0&&e.h!=0){render(e);eglSwapBuffers(e.display,e.surface);}}}
