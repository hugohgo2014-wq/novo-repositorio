#include <jni.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "NEBULA", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "NEBULA", __VA_ARGS__)

namespace nebula {
constexpr double G = 6.67430e-11;
constexpr double C = 299792458.0;
constexpr double AU = 149597870700.0;
constexpr double DAY = 86400.0;
constexpr double PI = 3.14159265358979323846;
constexpr size_t MAX_HISTORY = 12000;
constexpr int MAX_CRATERS = 12;
constexpr int MAX_BODIES = 64;

struct V3 {
    double x=0, y=0, z=0;
    V3() = default;
    V3(double X,double Y,double Z):x(X),y(Y),z(Z){}
    V3 operator+(const V3& b) const { return {x+b.x,y+b.y,z+b.z}; }
    V3 operator-(const V3& b) const { return {x-b.x,y-b.y,z-b.z}; }
    V3 operator*(double s) const { return {x*s,y*s,z*s}; }
    V3 operator/(double s) const { return {x/s,y/s,z/s}; }
    V3& operator+=(const V3& b){x+=b.x;y+=b.y;z+=b.z;return *this;}
    V3& operator-=(const V3& b){x-=b.x;y-=b.y;z-=b.z;return *this;}
};
static double dot(const V3&a,const V3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V3 cross(const V3&a,const V3&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double length(const V3&a){return std::sqrt(dot(a,a));}
static V3 norm(const V3&a){double l=length(a);return l>1e-20?a/l:V3(0,1,0);}
static double clampd(double x,double a,double b){return std::max(a,std::min(b,x));}
static float f(double x){return static_cast<float>(x);}

// Body types: star, rocky, gas giant, icy, iron-rich remnant.
enum Material { STAR=0, ROCK=1, GAS=2, ICE=3, IRON=4 };
struct Crater { V3 direction; float angularRadius=0.08f; float depth=0.012f; };
struct Body {
    std::string name;
    V3 p, v;
    double mass=0, radius=1, spin=0, temperature=250;
    double ironFraction=0.30, iceFraction=0.0;
    double luminosity=0;
    Material material=ROCK;
    bool active=true, visible=true, luminous=false, hasAtmosphere=false, hasRings=false;
    float atmosphereDamage=0;
    V3 atmosphereHole{0,0,1};
    double atmosphereEventTime=-1e30;
    std::vector<Crater> craters;
    uint32_t seed=1;
};
struct History {
    double time=0;
    std::vector<V3> p;
    std::vector<V3> v;
    std::vector<double> mass;
    std::vector<uint8_t> luminous;
};
struct HistState { V3 p; V3 v; double mass=0; bool luminous=false; };
struct Ejecta {
    V3 p, v;
    float temperature=1200;
    float density=3000;
    float pressure=0;
    float life=0;
    float mass=1;
    int material=ROCK;
    float size=2.0f;
};

struct CellKey { int64_t x=0,y=0,z=0; bool operator==(const CellKey&b)const{return x==b.x&&y==b.y&&z==b.z;} };
struct CellHash { size_t operator()(const CellKey&k)const noexcept {size_t h=std::hash<int64_t>{}(k.x);h^=std::hash<int64_t>{}(k.y)+0x9e3779b97f4a7c15ULL+(h<<6)+(h>>2);h^=std::hash<int64_t>{}(k.z)+0x9e3779b97f4a7c15ULL+(h<<6)+(h>>2);return h;} };
static CellKey cellOf(const V3&p,double size){return {int64_t(std::floor(p.x/size)),int64_t(std::floor(p.y/size)),int64_t(std::floor(p.z/size))};}

struct Mat4 { float m[16]{}; };
static Mat4 identity(){Mat4 r{};r.m[0]=r.m[5]=r.m[10]=r.m[15]=1;return r;}
static Mat4 multiply(const Mat4&a,const Mat4&b){Mat4 r{};for(int c=0;c<4;c++)for(int row=0;row<4;row++)for(int k=0;k<4;k++)r.m[c*4+row]+=a.m[k*4+row]*b.m[c*4+k];return r;}
static Mat4 perspective(float fovy,float aspect,float n,float far){Mat4 r{};float t=1.0f/std::tan(fovy*0.5f);r.m[0]=t/aspect;r.m[5]=t;r.m[10]=(far+n)/(n-far);r.m[11]=-1;r.m[14]=(2*far*n)/(n-far);return r;}
static Mat4 lookAt(V3 eye,V3 center,V3 up){
    V3 z=norm(eye-center),x=norm(cross(up,z)),y=cross(z,x);Mat4 r=identity();
    r.m[0]=f(x.x);r.m[4]=f(x.y);r.m[8]=f(x.z);
    r.m[1]=f(y.x);r.m[5]=f(y.y);r.m[9]=f(y.z);
    r.m[2]=f(z.x);r.m[6]=f(z.y);r.m[10]=f(z.z);
    r.m[12]=f(-dot(x,eye));r.m[13]=f(-dot(y,eye));r.m[14]=f(-dot(z,eye));return r;
}
static Mat4 modelMatrix(V3 p,float s,float spin,float flatten){
    float c=std::cos(spin), sn=std::sin(spin); Mat4 r=identity();
    r.m[0]=c*s;r.m[2]=-sn*s;r.m[8]=sn*s;r.m[10]=c*s;r.m[5]=s*(1.0f-flatten);
    r.m[12]=f(p.x);r.m[13]=f(p.y);r.m[14]=f(p.z);return r;
}

static GLuint compileShader(GLenum type,const char* src){
    GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048];GLsizei n=0;glGetShaderInfoLog(s,sizeof(log),&n,log);LOGE("shader compile: %s",log);glDeleteShader(s);return 0;}return s;
}
static GLuint makeProgram(const char* vs,const char* fs){GLuint a=compileShader(GL_VERTEX_SHADER,vs),b=compileShader(GL_FRAGMENT_SHADER,fs);if(!a||!b)return 0;GLuint p=glCreateProgram();glAttachShader(p,a);glAttachShader(p,b);glLinkProgram(p);glDeleteShader(a);glDeleteShader(b);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[2048];GLsizei n=0;glGetProgramInfoLog(p,sizeof(log),&n,log);LOGE("program link: %s",log);glDeleteProgram(p);return 0;}return p;}

const char* SURFACE_VS=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform float uTime;
uniform float uType;
uniform float uSeed;
uniform vec3 uTidal;
uniform int uCraterCount;
uniform vec3 uCraterDir[12];
uniform vec2 uCraterData[12];
out vec3 vLocal;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out float vHeight;
float hash31(vec3 p){p=fract(p*0.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
float noise3(vec3 x){vec3 i=floor(x),f=fract(x);f=f*f*(3.0-2.0*f);return mix(mix(mix(hash31(i),hash31(i+vec3(1,0,0)),f.x),mix(hash31(i+vec3(0,1,0)),hash31(i+vec3(1,1,0)),f.x),f.y),mix(mix(hash31(i+vec3(0,0,1)),hash31(i+vec3(1,0,1)),f.x),mix(hash31(i+vec3(0,1,1)),hash31(i+vec3(1,1,1)),f.x),f.y),f.z);}
float fbm(vec3 p){float v=0.0,a=0.5;for(int i=0;i<5;i++){v+=a*noise3(p);p=p*2.03+17.1;a*=0.5;}return v;}
float terrain(vec3 n){float continental=fbm(n*3.7+uSeed);float ridges=1.0-abs(2.0*fbm(n*15.0+uSeed*2.0)-1.0);float h=(continental-0.48)*0.024+pow(ridges,5.0)*0.008;
 for(int i=0;i<12;i++){if(i>=uCraterCount)break;float d=acos(clamp(dot(n,normalize(uCraterDir[i])),-1.0,1.0));float x=d/max(uCraterData[i].x,0.0001);if(x<1.0){float bowl=-(1.0-x*x)*uCraterData[i].y;float rim=0.23*uCraterData[i].y*exp(-pow((x-0.88)*11.0,2.0));h+=bowl+rim;}}
 return h;}
void main(){vec3 n=normalize(aPos);float h=0.0;float type=floor(uType+0.5);if(type==1.0||type==3.0||type==4.0)h=terrain(n);float tideAxis=dot(n,normalize(uTidal+vec3(0.00001)));float tidal=(1.5*tideAxis*tideAxis-0.5)*length(uTidal);vec3 p=n*(1.0+h+tidal);
 vec4 world=uModel*vec4(p,1.0);vLocal=n;vWorld=world.xyz;vNormal=normalize(mat3(uModel)*n);vUV=aUV;vHeight=h;gl_Position=uMVP*vec4(p,1.0);}
)GLSL";

const char* SURFACE_FS=R"GLSL(#version 300 es
precision highp float;
in vec3 vLocal;in vec3 vWorld;in vec3 vNormal;in vec2 vUV;in float vHeight;
uniform float uTime;uniform float uType;uniform float uTemperature;uniform vec3 uLightDir;uniform float uLightPower;uniform float uSeed;
out vec4 fragColor;
float hash31(vec3 p){p=fract(p*0.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
float noise3(vec3 x){vec3 i=floor(x),f=fract(x);f=f*f*(3.0-2.0*f);return mix(mix(mix(hash31(i),hash31(i+vec3(1,0,0)),f.x),mix(hash31(i+vec3(0,1,0)),hash31(i+vec3(1,1,0)),f.x),f.y),mix(mix(hash31(i+vec3(0,0,1)),hash31(i+vec3(1,0,1)),f.x),mix(hash31(i+vec3(0,1,1)),hash31(i+vec3(1,1,1)),f.x),f.y),f.z);}
float fbm(vec3 p){float v=0.0,a=0.5;for(int i=0;i<5;i++){v+=a*noise3(p);p=p*2.0+13.7;a*=0.5;}return v;}
void main(){float type=floor(uType+0.5);vec3 base;vec3 n=normalize(vNormal);float noise=fbm(vLocal*8.0+uSeed);float h=vHeight;
 if(type==0.0){float gran=fbm(vLocal*24.0+uTime*0.08);float flare=pow(max(0.0,sin(vLocal.y*45.0+uTime*0.7+gran*3.0)),8.0);base=mix(vec3(1.0,0.23,0.035),vec3(1.0,0.86,0.48),clamp(gran*0.9+flare*0.3,0.0,1.0));}
 else if(type==2.0){float lat=vLocal.y;float turbulence=fbm(vec3(vLocal.x*5.0+uTime*0.10,lat*38.0+sin(vLocal.x*11.0+uTime*0.2)*1.7,vLocal.z*5.0));float bands=sin(lat*105.0+sin(vLocal.x*8.0+uTime*0.18)*2.4+turbulence*4.0);float stormQ=(length(vLocal.xz-vec2(0.35,-0.12))-0.23)*18.0;float storm=exp(-stormQ*stormQ)*0.5;base=mix(vec3(0.42,0.20,0.09),vec3(0.92,0.67,0.38),smoothstep(-0.25,0.45,bands+turbulence*0.4));base=mix(base,vec3(0.75,0.25,0.12),storm);}
 else if(type==3.0){base=mix(vec3(0.12,0.26,0.43),vec3(0.86,0.94,1.0),noise*0.75+0.15);}
 else if(type==4.0){base=mix(vec3(0.22,0.20,0.18),vec3(0.65,0.43,0.30),noise);}
 else {float sea=smoothstep(-0.004,0.006,h);vec3 ocean=vec3(0.018,0.10,0.26);vec3 land=mix(vec3(0.11,0.25,0.09),vec3(0.43,0.39,0.23),smoothstep(0.0,0.02,h));float snow=smoothstep(0.72,0.94,abs(vLocal.y)+noise*0.08);base=mix(ocean,land,sea);base=mix(base,vec3(0.82,0.87,0.82),snow);float lava=smoothstep(0.82,0.96,noise)*smoothstep(0.05,0.12,uTemperature/3000.0);base=mix(base,vec3(1.0,0.19,0.015),lava);}
 float diffuse=max(dot(n,normalize(uLightDir)),0.0);float ambient=type==0.0?1.0:0.0;float light=ambient+diffuse*(uLightPower*0.90);vec3 color=base*light;
 if(type==0.0)color*=1.45;float rim=pow(1.0-max(dot(n,normalize(uLightDir)),0.0),3.0);color+=vec3(0.06,0.16,0.30)*rim*0.17;
 fragColor=vec4(color,1.0);}
)GLSL";

const char* ATMO_VS=R"GLSL(#version 300 es
precision highp float;layout(location=0) in vec3 aPos;layout(location=1) in vec2 aUV;uniform mat4 uMVP;uniform mat4 uModel;out vec3 vN;out vec3 vW;out vec2 vUV;void main(){vN=normalize(mat3(uModel)*aPos);vec4 w=uModel*vec4(aPos,1.0);vW=w.xyz;vUV=aUV;gl_Position=uMVP*vec4(aPos,1.0);}
)GLSL";
const char* ATMO_FS=R"GLSL(#version 300 es
precision highp float;in vec3 vN;in vec3 vW;in vec2 vUV;uniform vec3 uCamera;uniform float uTime;uniform vec3 uColor;uniform float uDamage;uniform vec3 uHoleDir;uniform float uShockAge;uniform float uLightPower;out vec4 fragColor;float hash(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}void main(){vec3 n=normalize(vN);vec3 view=normalize(uCamera-vW);float fres=pow(1.0-max(dot(n,view),0.0),2.3);float clouds=0.85+0.15*sin(vUV.y*130.0+sin(vUV.x*37.0+uTime*0.12)*2.0);float hole=dot(n,normalize(uHoleDir));float damageMask=smoothstep(0.35,0.92,hole)*uDamage;float alpha=fres*(0.22+0.55*clouds)*(1.0-damageMask*0.88)*clamp(uLightPower,0.0,1.0);float shock=exp(-pow((length(vUV-vec2(0.5))-mod(uShockAge*0.035,0.5))*35.0,2.0))*smoothstep(0.0,1.5,uShockAge)*0.25;vec3 col=mix(uColor,vec3(1.0,0.34,0.08),damageMask*0.8)+vec3(0.30,0.65,1.0)*shock;fragColor=vec4(col,clamp(alpha+shock,0.0,0.8));}
)GLSL";

const char* RING_VS=R"GLSL(#version 300 es
precision highp float;layout(location=0) in vec3 aPos;layout(location=1) in vec2 aUV;uniform mat4 uMVP;uniform mat4 uModel;out vec2 vUV;out float vRadius;void main(){vUV=aUV;vRadius=length(aPos.xz);gl_Position=uMVP*vec4(aPos,1.0);}
)GLSL";
const char* RING_FS=R"GLSL(#version 300 es
precision highp float;in vec2 vUV;in float vRadius;uniform float uTime;out vec4 fragColor;void main(){float bands=0.5+0.5*sin(vRadius*470.0+sin(vUV.x*70.0+uTime*0.03)*3.0);float gaps=smoothstep(0.015,0.025,abs(fract(vRadius*17.0)-0.5));float alpha=(0.16+0.58*bands)*gaps;vec3 c=mix(vec3(0.32,0.23,0.15),vec3(0.88,0.76,0.57),bands);fragColor=vec4(c,alpha);}
)GLSL";
const char* PARTICLE_VS=R"GLSL(#version 300 es
precision highp float;layout(location=0) in vec3 aPos;layout(location=1) in vec4 aData;uniform mat4 uVP;uniform float uPointScale;out vec4 vData;void main(){vData=aData;gl_Position=uVP*vec4(aPos,1.0);gl_PointSize=clamp(uPointScale/(gl_Position.w+0.1),1.0,12.0);}
)GLSL";
const char* PARTICLE_FS=R"GLSL(#version 300 es
precision highp float;in vec4 vData;out vec4 fragColor;void main(){vec2 p=gl_PointCoord*2.0-1.0;float r=dot(p,p);if(r>1.0)discard;vec3 c=vData.y<0.5?vec3(1.0,0.79,0.42):(vData.y<1.5?vec3(0.85,0.34,0.11):(vData.y<2.5?vec3(0.72,0.76,0.84):vec3(0.24,0.66,1.0)));float glow=exp(-r*3.0);fragColor=vec4(c,glow*clamp(vData.x,0.0,1.0));}
)GLSL";
const char* STAR_VS=R"GLSL(#version 300 es
precision highp float;layout(location=0) in vec3 aPos;uniform mat4 uVP;uniform float uSize;void main(){gl_Position=uVP*vec4(aPos,1.0);gl_PointSize=uSize;}
)GLSL";
const char* STAR_FS=R"GLSL(#version 300 es
precision highp float;out vec4 fragColor;void main(){vec2 p=gl_PointCoord-0.5;float a=exp(-dot(p,p)*35.0);fragColor=vec4(vec3(0.65,0.78,1.0),a);}
)GLSL";

struct Vertex { float x,y,z,u,v; };
struct Mesh { GLuint vao=0,vbo=0,ebo=0; GLsizei count=0; };
static Mesh makeSphere(int slices=64,int stacks=40){
    std::vector<Vertex> vs;std::vector<uint32_t> is;
    for(int y=0;y<=stacks;y++){float v=float(y)/stacks,phi=float(PI)*v;for(int x=0;x<=slices;x++){float u=float(x)/slices,theta=float(2.0*PI)*u;vs.push_back({std::sin(phi)*std::cos(theta),std::cos(phi),std::sin(phi)*std::sin(theta),u,v});}}
    for(int y=0;y<stacks;y++)for(int x=0;x<slices;x++){uint32_t a=y*(slices+1)+x,b=a+slices+1;is.insert(is.end(),{a,b,a+1,b,b+1,a+1});}
    Mesh m;glGenVertexArrays(1,&m.vao);glGenBuffers(1,&m.vbo);glGenBuffers(1,&m.ebo);glBindVertexArray(m.vao);glBindBuffer(GL_ARRAY_BUFFER,m.vbo);glBufferData(GL_ARRAY_BUFFER,vs.size()*sizeof(Vertex),vs.data(),GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,is.size()*sizeof(uint32_t),is.data(),GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));glBindVertexArray(0);m.count=(GLsizei)is.size();return m;
}
static Mesh makeRing(){
    std::vector<Vertex> vs;std::vector<uint32_t> is;const int seg=192;const float inner=1.32f,outer=2.22f;
    for(int j=0;j<=1;j++){float r=j?outer:inner;for(int i=0;i<=seg;i++){float a=float(i)/seg*float(2*PI);vs.push_back({r*std::cos(a),0.0f,r*std::sin(a),float(i)/seg,float(j)});}}
    for(int i=0;i<seg;i++){uint32_t a=i,b=seg+1+i;is.insert(is.end(),{a,b,a+1,a+1,b,b+1});}
    Mesh m;glGenVertexArrays(1,&m.vao);glGenBuffers(1,&m.vbo);glGenBuffers(1,&m.ebo);glBindVertexArray(m.vao);glBindBuffer(GL_ARRAY_BUFFER,m.vbo);glBufferData(GL_ARRAY_BUFFER,vs.size()*sizeof(Vertex),vs.data(),GL_STATIC_DRAW);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m.ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,is.size()*sizeof(uint32_t),is.data(),GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));glBindVertexArray(0);m.count=(GLsizei)is.size();return m;
}

class Engine {
public:
    std::vector<Body> bodies;
    std::vector<Ejecta> ejecta;
    std::deque<History> history;
    std::mutex mutex;
    GLuint surfaceProgram=0,atmoProgram=0,ringProgram=0,particleProgram=0,starProgram=0;
    Mesh sphere,sphereHigh,ring;
    GLuint particleVAO=0,particleVBO=0,starVAO=0,starVBO=0;
    std::vector<V3> stars;
    int width=1,height=1,selected=0;
    bool paused=false,retardedGravity=true,lightTimeView=false,birdMode=false,followSelected=true;
    double simTime=0,simSpeed=3600.0;
    float yaw=0.6f,pitch=0.35f,cameraDistance=5.0f;
    V3 target{0,0,0};
    float lastFrame=0;
    std::mt19937 rng{0x4E454255};

    void init(int w,int h){std::lock_guard<std::mutex> lock(mutex);width=std::max(1,w);height=std::max(1,h);glViewport(0,0,width,height);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glClearColor(0.004f,0.008f,0.022f,1.0f);
        surfaceProgram=makeProgram(SURFACE_VS,SURFACE_FS);atmoProgram=makeProgram(ATMO_VS,ATMO_FS);ringProgram=makeProgram(RING_VS,RING_FS);particleProgram=makeProgram(PARTICLE_VS,PARTICLE_FS);starProgram=makeProgram(STAR_VS,STAR_FS);sphere=makeSphere(48,32);sphereHigh=makeSphere(112,72);ring=makeRing();
        glGenVertexArrays(1,&particleVAO);glGenBuffers(1,&particleVBO);glBindVertexArray(particleVAO);glBindBuffer(GL_ARRAY_BUFFER,particleVBO);glBufferData(GL_ARRAY_BUFFER,1,nullptr,GL_DYNAMIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)0);glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,7*sizeof(float),(void*)(3*sizeof(float)));glBindVertexArray(0);
        makeStars();setupSolarSystem();LOGI("NEBULA initialized");
    }
    void resize(int w,int h){std::lock_guard<std::mutex> lock(mutex);width=std::max(1,w);height=std::max(1,h);glViewport(0,0,width,height);}
    void makeStars(){std::uniform_real_distribution<float>d(-1,1);stars.clear();for(int i=0;i<1200;i++){V3 p=norm(V3(d(rng),d(rng),d(rng)))*d(rng);stars.push_back(p*180.0);}}
    void addBody(Body b){if(bodies.size()<MAX_BODIES)bodies.push_back(std::move(b));}
    Body makePlanet(std::string name,double mass,double radius,double au,double phase,Material mat,double spin,double temp,double iron,double ice,bool atmosphere,bool ringsOn=false){Body b;b.name=std::move(name);b.mass=mass;b.radius=radius;b.material=mat;b.spin=spin;b.temperature=temp;b.ironFraction=iron;b.iceFraction=ice;b.hasAtmosphere=atmosphere;b.hasRings=ringsOn;b.seed=uint32_t(bodies.size()*1777+73);b.p={std::cos(phase)*au*AU,0,std::sin(phase)*au*AU};double speed=std::sqrt(G*1.9885e30/(au*AU));b.v={-std::sin(phase)*speed,0,std::cos(phase)*speed};if(mat==GAS)b.temperature=temp;return b;}
    void setupSolarSystem(){bodies.clear();ejecta.clear();history.clear();simTime=0;simSpeed=3600;paused=false;retardedGravity=true;lightTimeView=false;birdMode=false;selected=0;
        Body sun;sun.name="Sol";sun.mass=1.9885e30;sun.radius=6.9634e8;sun.material=STAR;sun.temperature=5778;sun.luminosity=3.828e26;sun.luminous=true;sun.seed=11;addBody(sun);
        addBody(makePlanet("Mercúrio",3.301e23,2.4397e6,0.387,0.4,ROCK,1.24e-6,440,0.68,0,false));
        addBody(makePlanet("Vênus",4.867e24,6.0518e6,0.723,2.0,ROCK,-2.99e-7,737,0.30,0,true));
        addBody(makePlanet("Terra",5.972e24,6.371e6,1.0,0.0,ROCK,7.2921e-5,288,0.32,0,true));
        addBody(makePlanet("Marte",6.417e23,3.3895e6,1.524,1.2,ROCK,7.088e-5,210,0.25,0.03,true));
        addBody(makePlanet("Júpiter",1.898e27,6.9911e7,5.204,2.7,GAS,1.7585e-4,165,0.12,0,true,true));
        addBody(makePlanet("Saturno",5.683e26,5.8232e7,9.58,3.7,GAS,1.6379e-4,134,0.12,0.15,true,true));
        addBody(makePlanet("Urano",8.681e25,2.5362e7,19.2,4.4,ICE,-1.012e-4,76,0.15,0.30,true));
        addBody(makePlanet("Netuno",1.024e26,2.4622e7,30.05,5.2,ICE,1.083e-4,72,0.15,0.30,true));
        // The Moon is a real additional N-body, initialized in an approximate Earth-relative orbit.
        Body moon;moon.name="Lua";moon.mass=7.342e22;moon.radius=1.7374e6;moon.material=ROCK;moon.temperature=220;moon.ironFraction=0.28;moon.seed=91;moon.p=bodies[3].p+V3(3.844e8,0,0);moon.v=bodies[3].v+V3(0,0,1022);addBody(moon);
        target=V3(0,0,0);followSelected=true;cameraDistance=72.0f;yaw=0.7f;pitch=0.26f;recordHistory();
    }
    void recordHistory(){History h;h.time=simTime;h.p.reserve(bodies.size());h.v.reserve(bodies.size());h.mass.reserve(bodies.size());h.luminous.reserve(bodies.size());for(const auto&b:bodies){h.p.push_back(b.p);h.v.push_back(b.v);h.mass.push_back(b.active?b.mass:0);h.luminous.push_back(b.active&&b.luminous?1:0);}history.push_back(std::move(h));while(history.size()>MAX_HISTORY)history.pop_front();}
    HistState sampleHistory(int idx,double t)const{
        if(history.empty()||idx<0)return {};
        if(t<=history.front().time){const auto&h=history.front();if(idx<(int)h.p.size()){V3 velocity=idx<(int)h.v.size()?h.v[idx]:V3{};return {h.p[idx]+velocity*(t-h.time),velocity,h.mass[idx],h.luminous[idx]!=0};}}
        if(t>=history.back().time){const auto&h=history.back();if(idx<(int)h.p.size()){V3 velocity=idx<(int)h.v.size()?h.v[idx]:V3{};return {h.p[idx]+velocity*(t-h.time),velocity,h.mass[idx],h.luminous[idx]!=0};}if(idx<(int)bodies.size()){const auto&body=bodies[idx];return {body.p,body.v,body.active?body.mass:0,body.active&&body.luminous};}return {};}
        size_t lo=0,hi=history.size()-1;while(hi-lo>1){size_t mid=(lo+hi)/2;if(history[mid].time<t)lo=mid;else hi=mid;}
        const auto&a=history[lo];const auto&b=history[hi];if(idx>=(int)a.p.size()||idx>=(int)b.p.size())return {};
        double span=std::max(1e-9,b.time-a.time),u=clampd((t-a.time)/span,0,1);V3 va=idx<(int)a.v.size()?a.v[idx]:V3{};V3 vb=idx<(int)b.v.size()?b.v[idx]:V3{};
        double u2=u*u,u3=u2*u;double h00=2*u3-3*u2+1,h10=u3-2*u2+u,h01=-2*u3+3*u2,h11=u3-u2;
        V3 p=a.p[idx]*h00+va*(h10*span)+b.p[idx]*h01+vb*(h11*span);V3 v=(a.p[idx]*(6*u2-6*u)/span)+va*(3*u2-4*u+1)+(b.p[idx]*(-6*u2+6*u)/span)+vb*(3*u2-2*u);
        double m=a.mass[idx]*(1-u)+b.mass[idx]*u;bool lum=(u<0.5?a.luminous[idx]:b.luminous[idx])!=0;return {p,v,m,lum};
    }
    V3 acceleration(int i,double t,const std::vector<V3>&positions,bool retard){V3 a{};const Body&bi=bodies[i];if(!bi.active)return a;
        for(int j=0;j<(int)bodies.size();j++){if(i==j)continue;HistState hs;V3 src;double mass=0;
            if(retard){V3 now=(j<(int)positions.size()?positions[j]:bodies[j].p);double dist=std::max(1.0,length(now-positions[i]));hs=sampleHistory(j,t-dist/C);src=hs.p;mass=hs.mass;}
            else {if(!bodies[j].active)continue;src=positions[j];mass=bodies[j].mass;}
            if(mass<=0)continue;V3 d=src-positions[i];double r2=dot(d,d);double soft=std::max(1000.0,(bi.radius+bodies[j].radius)*0.08);double denom=std::pow(r2+soft*soft,1.5);if(denom>0)a+=d*(G*mass/denom);
        }return a;
    }
    void step(double dt){if(dt<=0)return;std::vector<V3> p0(bodies.size()),a0(bodies.size()),a1(bodies.size());for(size_t i=0;i<bodies.size();i++)p0[i]=bodies[i].p;
        for(int i=0;i<(int)bodies.size();i++)a0[i]=acceleration(i,simTime,p0,retardedGravity);
        for(size_t i=0;i<bodies.size();i++)if(bodies[i].active){bodies[i].p+=bodies[i].v*dt+a0[i]*(0.5*dt*dt);bodies[i].v+=a0[i]*(0.5*dt);}
        simTime+=dt;for(size_t i=0;i<bodies.size();i++)p0[i]=bodies[i].p;
        for(int i=0;i<(int)bodies.size();i++)a1[i]=acceleration(i,simTime,p0,retardedGravity);
        for(size_t i=0;i<bodies.size();i++)if(bodies[i].active)bodies[i].v+=(a1[i]+a0[i])*(0.5*dt);
        // Spin is independent of orbital position; deformation is derived at render time.
        solveCollisions();updateEjecta(dt);recordHistory();
    }
    double density(const Body&b)const{if(b.material==GAS)return 1300;if(b.material==ICE)return 1600+1000*b.ironFraction;if(b.material==IRON)return 7800;return 3300+4500*b.ironFraction;}
    void addCrater(Body&targetBody,const V3&worldDirection,double energy){Crater c;c.direction=norm(worldDirection);double ratio=clampd(std::cbrt(std::max(energy,1e8)/1e23)*0.015,0.002,0.55);c.angularRadius=float(ratio);c.depth=float(clampd(ratio*0.10,0.002,0.035));targetBody.craters.push_back(c);if(targetBody.craters.size()>MAX_CRATERS)targetBody.craters.erase(targetBody.craters.begin());}
    void atmosphereImpact(Body&b,const V3&direction,double energy){if(!b.hasAtmosphere)return;b.atmosphereDamage=float(clampd(b.atmosphereDamage+energy/1.5e23,0,1));b.atmosphereHole=norm(direction);b.atmosphereEventTime=simTime;}
    void spawnEjecta(const V3&origin,const V3&normal,const V3&baseVelocity,double speed,double totalMass,double ironFraction,double iceFraction){
        std::uniform_real_distribution<double> d(-1,1),u(0.2,1.0);int count=std::min(120, std::max(18,int(30+speed/1200.0)));V3 tangent=norm(cross(normal,V3(0,1,0)));if(length(tangent)<0.1)tangent=norm(cross(normal,V3(1,0,0)));V3 bitangent=norm(cross(normal,tangent));
        for(int k=0;k<count;k++){V3 dir=norm(normal*(0.2+u(rng))+tangent*d(rng)*0.8+bitangent*d(rng)*0.8);Ejecta e;e.p=origin+dir*2.0e6;e.v=baseVelocity+dir*(speed*(0.25+u(rng)*0.65));e.mass=float(totalMass/count);e.density=e.material==IRON?7800.0f:(e.material==ICE?1600.0f:3300.0f);float r=float(std::generate_canonical<double,16>(rng));e.material=r<ironFraction?IRON:(r<ironFraction+(1-ironFraction-iceFraction)?ROCK:ICE);e.temperature=float(800+speed*speed*0.00025);e.life=0;e.size=float(1.6+u(rng)*3.2);ejecta.push_back(e);}
        if(ejecta.size()>1800)ejecta.erase(ejecta.begin(),ejecta.begin()+int(ejecta.size()-1800));
    }
    void solveCollisions(){std::vector<uint8_t> marked(bodies.size(),0);
        for(int i=0;i<(int)bodies.size();i++){if(marked[i]||!bodies[i].active||bodies[i].material==STAR)continue;
            for(int j=i+1;j<(int)bodies.size();j++){if(marked[j]||!bodies[j].active||bodies[j].material==STAR)continue;Body&a=bodies[i];Body&b=bodies[j];V3 delta=b.p-a.p;double dist=length(delta),sum=a.radius+b.radius;if(dist>=sum||dist<1)continue;V3 n=delta/dist;V3 rel=b.v-a.v;double closingSpeed=dot(rel,n);if(closingSpeed>=0){double overlap=sum-dist;a.p-=n*(overlap*0.5);b.p+=n*(overlap*0.5);marked[i]=marked[j]=1;break;}double impactSpeed=length(rel);double mu=a.mass*b.mass/(a.mass+b.mass);double Q=0.5*mu*impactSpeed*impactSpeed/(a.mass+b.mass);
                double strengthA=a.material==ICE?1.2e5:(a.material==IRON?8.0e6:2.4e6);double strengthB=b.material==ICE?1.2e5:(b.material==IRON?8.0e6:2.4e6);double qStar=std::sqrt(strengthA*strengthB)*std::pow(std::max(1.0,(a.radius+b.radius)*0.5)/1.0e6,0.35);double escape=std::sqrt(2.0*G*(a.mass+b.mass)/sum);double energy=0.5*mu*impactSpeed*impactSpeed;
                if(Q>qStar*0.65){ // energetic impact: largest remnant + material-tagged ejecta
                    Body* targetBody=a.mass>=b.mass?&a:&b;Body* impactor=targetBody==&a?&b:&a;V3 impactDir=targetBody==&a?n:n*-1.0;double massA=a.mass,massB=b.mass,totalMass=massA+massB;V3 impactL=cross(b.p-a.p,b.v-a.v)*mu;double ejectaFraction=clampd(0.10+0.22*(Q/std::max(qStar,1.0)),0.12,0.72);double remnantMass=totalMass*(1.0-ejectaFraction);V3 cm=(a.p*massA+b.p*massB)/totalMass;V3 cv=(a.v*massA+b.v*massB)/totalMass;double rho=(density(a)*massA+density(b)*massB)/totalMass;double mixedIron=(a.ironFraction*massA+b.ironFraction*massB)/totalMass;double mixedIce=(a.iceFraction*massA+b.iceFraction*massB)/totalMass;targetBody->mass=remnantMass;targetBody->radius=std::cbrt(3.0*remnantMass/(4.0*PI*rho));targetBody->p=cm;targetBody->v=cv-impactDir*(totalMass*ejectaFraction/remnantMass*impactSpeed*0.30);targetBody->spin+=impactL.y/std::max(1.0,0.4*remnantMass*targetBody->radius*targetBody->radius);targetBody->ironFraction=mixedIron;targetBody->iceFraction=mixedIce;targetBody->temperature=std::min(9000.0,targetBody->temperature+impactSpeed*impactSpeed/12000.0);addCrater(*targetBody,impactDir,energy);atmosphereImpact(*targetBody,impactDir,energy);spawnEjecta(cm+impactDir*targetBody->radius,impactDir,cv,impactSpeed,totalMass*ejectaFraction,targetBody->ironFraction,targetBody->iceFraction);impactor->active=false;impactor->mass=0;impactor->visible=false;marked[targetBody==&a?j:i]=1;marked[targetBody==&a?i:j]=1;
                }else if(impactSpeed<escape*1.35||std::min(a.mass,b.mass)/std::max(a.mass,b.mass)<0.05){ // accretion / impactor capture: conserve mass and linear momentum
                    Body* primary=a.mass>=b.mass?&a:&b;Body* secondary=primary==&a?&b:&a;V3 impactDir=primary==&a?n:n*-1.0;double massA=a.mass,massB=b.mass,total=massA+massB;V3 impactL=cross(b.p-a.p,b.v-a.v)*mu;V3 cm=(a.p*massA+b.p*massB)/total;V3 cv=(a.v*massA+b.v*massB)/total;double rho=(density(a)*massA+density(b)*massB)/total;double mixedIron=(a.ironFraction*massA+b.ironFraction*massB)/total;double mixedIce=(a.iceFraction*massA+b.iceFraction*massB)/total;double secondaryMass=secondary->mass;primary->mass=total;primary->radius=std::cbrt(3.0*total/(4.0*PI*rho));primary->spin+=impactL.y/std::max(1.0,0.4*total*primary->radius*primary->radius);primary->p=cm;primary->v=cv-impactDir*(secondaryMass*0.15/total*impactSpeed*0.30);primary->ironFraction=mixedIron;primary->iceFraction=mixedIce;primary->temperature=std::min(9000.0,(a.temperature*massA+b.temperature*massB)/total+impactSpeed*impactSpeed/100000.0);addCrater(*primary,impactDir,energy);atmosphereImpact(*primary,impactDir,energy);spawnEjecta(cm+impactDir*primary->radius,impactDir,cv,impactSpeed,std::min(secondaryMass*0.15,total*0.002),primary->ironFraction,primary->iceFraction);secondary->active=false;secondary->mass=0;secondary->visible=false;marked[i]=marked[j]=1;
                }else { // grazing bounce with material-dependent restitution and impulse
                    double e=(a.material==ICE||b.material==ICE)?0.18:0.08;double vn=dot(rel,n);if(vn<0){double impulse=-(1+e)*vn/(1/a.mass+1/b.mass);a.v-=n*(impulse/a.mass);b.v+=n*(impulse/b.mass);}double overlap=sum-dist;a.p-=n*(overlap*0.52);b.p+=n*(overlap*0.48);addCrater(a,n,energy*0.4);addCrater(b,n*-1,energy*0.4);atmosphereImpact(a,n,energy);atmosphereImpact(b,n*-1,energy);spawnEjecta((a.p+b.p)*0.5,n,(a.v+b.v)*0.5,impactSpeed,mu*0.002,a.ironFraction,a.iceFraction);marked[i]=marked[j]=1;
                }
                break;
            }
        }
    }
    void updateEjecta(double dt){
        const size_t count=ejecta.size();
        if(count==0)return;
        // Compact multi-material SPH parcels: spatial hashing keeps this bounded on mobile GPUs/CPUs.
        const double h=8.0e6,h2=h*h;
        const double poly6=315.0/(64.0*PI*std::pow(h,9));
        const double gradCoeff=-945.0/(32.0*PI*std::pow(h,9));
        std::unordered_map<CellKey,std::vector<int>,CellHash> grid;
        grid.reserve(count*2);
        std::vector<CellKey> cells(count);
        for(size_t i=0;i<count;i++){cells[i]=cellOf(ejecta[i].p,h);grid[cells[i]].push_back(int(i));}
        std::vector<double> rho(count,0.0),pressure(count,0.0),sound(count,4000.0);
        auto restDensity=[](int material){return material==IRON?7800.0:(material==ICE?1600.0:3300.0);};
        auto soundSpeed=[](int material){return material==IRON?6000.0:(material==ICE?3000.0:5000.0);};
        for(size_t i=0;i<count;i++){
            const auto&e=ejecta[i];sound[i]=soundSpeed(e.material);rho[i]=e.mass*poly6*h2*h2*h2;
            const CellKey c=cells[i];
            for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++)for(int dz=-1;dz<=1;dz++){
                auto it=grid.find({c.x+dx,c.y+dy,c.z+dz});if(it==grid.end())continue;
                for(int j:it->second){if(size_t(j)==i)continue;V3 d=e.p-ejecta[j].p;double r2=dot(d,d);if(r2<h2){double q=h2-r2;rho[i]+=ejecta[j].mass*poly6*q*q*q;}}
            }
            rho[i]=clampd(rho[i],restDensity(e.material)*0.08,restDensity(e.material)*12.0);
            // A compressive-only Tait-like response avoids unphysical tensile clumping in the sparse ejecta.
            double ratio=std::max(0.0,rho[i]/restDensity(e.material)-1.0);
            pressure[i]=std::min(rho[i]*sound[i]*sound[i]*4.0, rho[i]*sound[i]*sound[i]*ratio);
        }
        std::vector<V3> hydro(count);
        for(size_t i=0;i<count;i++){
            const auto&e=ejecta[i];V3 acc{};const CellKey c=cells[i];
            for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++)for(int dz=-1;dz<=1;dz++){
                auto it=grid.find({c.x+dx,c.y+dy,c.z+dz});if(it==grid.end())continue;
                for(int j:it->second){if(size_t(j)==i)continue;const auto&other=ejecta[j];V3 rij=e.p-other.p;double r2=dot(rij,rij);if(r2<1.0||r2>=h2)continue;double q=h2-r2;V3 grad=rij*(gradCoeff*q*q);double rhoi=std::max(1.0,rho[i]),rhoj=std::max(1.0,rho[j]);double pterm=pressure[i]/(rhoi*rhoi)+pressure[j]/(rhoj*rhoj);
                    V3 vij=e.v-other.v;double visc=0.0;double approach=dot(vij,rij);if(approach<0){double mu=h*approach/(r2+0.01*h2);double cbar=0.5*(sound[i]+sound[j]);double rhobar=0.5*(rhoi+rhoj);visc=(-0.6*cbar*mu+1.2*mu*mu)/rhobar;}
                    acc-=grad*(other.mass*(pterm+visc));
                }
            }
            double a=length(acc);if(a>2.0e5)acc=acc*(2.0e5/a);hydro[i]=acc;
        }
        for(size_t i=0;i<count;i++){
            auto&e=ejecta[i];V3 acc=hydro[i];double nearest=1e100;int ni=-1;
            for(int j=0;j<(int)bodies.size();j++)if(bodies[j].active&&bodies[j].mass>0){double d=length(bodies[j].p-e.p);if(d<nearest){nearest=d;ni=j;}}
            if(ni>=0&&nearest>1){V3 d=bodies[ni].p-e.p;acc+=d*(G*bodies[ni].mass/std::pow(dot(d,d)+1e10,1.5));}
            e.v+=acc*dt;e.p+=e.v*dt;e.life+=float(dt);e.temperature=std::max(80.0f,e.temperature-float(dt*0.0003));e.density=float(rho[i]);e.pressure=float(pressure[i]);
        }
        ejecta.erase(std::remove_if(ejecta.begin(),ejecta.end(),[](const Ejecta&e){return e.life>500000.0f||e.temperature<85.0f;}),ejecta.end());
    }

    void update(float realDt){std::lock_guard<std::mutex> lock(mutex);if(!paused){double remaining=std::min(86400.0,double(realDt)*simSpeed);int steps=0;while(remaining>1e-5&&steps<16){double dt=std::min(300.0,remaining);step(dt);remaining-=dt;steps++;}if(remaining>0){ // keep simulation responsive when the user selects extreme time acceleration
                step(std::min(remaining,300.0));}}
        // A slow atmospheric recovery makes impacts persistent but not permanently frozen.
        for(auto&b:bodies)if(b.atmosphereDamage>0)b.atmosphereDamage=std::max(0.0f,b.atmosphereDamage-float(realDt*0.00003));
    }

    V3 cameraFocus()const{if(followSelected&&selected>=0&&selected<(int)bodies.size()&&bodies[selected].active)return bodies[selected].p/AU;return target/AU;}
    V3 cameraPosition(){V3 focus=cameraFocus();float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);V3 offset{cameraDistance*cp*sy,cameraDistance*sp,cameraDistance*cp*cy};return focus+offset;}
    V3 visiblePosition(int i,V3 cameraAU){if(!lightTimeView)return bodies[i].p;double delay=length(bodies[i].p-cameraAU*AU)/C;return sampleHistory(i,simTime-delay).p;}
    HistState lightState(int i,const V3& targetPosition){double distance=length(bodies[i].p-targetPosition);return sampleHistory(i,simTime-distance/C);}
    float flattening(const Body&b)const{if(b.mass<=0)return 0;double value=b.spin*b.spin*b.radius*b.radius*b.radius/(2.0*G*b.mass);return f(clampd(value,0,0.16));}
    V3 tidalVector(int idx)const{const Body&b=bodies[idx];V3 sum{};for(int j=0;j<(int)bodies.size();j++)if(j!=idx&&bodies[j].active&&bodies[j].mass>b.mass*0.0001){V3 d=bodies[j].p-b.p;double r=length(d);if(r>b.radius*2){double amp=std::min(0.025,(bodies[j].mass/b.mass)*std::pow(b.radius/r,3)*100000.0);sum+=norm(d)*amp;}}return sum;}

    void draw(int w,int h){std::lock_guard<std::mutex> lock(mutex);if(w>0&&h>0){width=w;height=h;}glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
        V3 focus=cameraFocus();V3 cam=cameraPosition();float nearPlane=f(std::max(1.0e-8,cameraDistance*0.0001));float farPlane=f(std::max(0.02,cameraDistance*3.5));Mat4 proj=perspective(0.78f,float(width)/float(height),nearPlane,farPlane);Mat4 view=lookAt(cam,focus,V3(0,1,0));Mat4 vp=multiply(proj,view);
        drawStars(vp);
        for(int i=0;i<(int)bodies.size();i++){const Body&b=bodies[i];if(!b.visible&&!lightTimeView)continue;HistState observed{};bool historical=false;if(!b.active){if(!lightTimeView)continue;double delay=length(b.p-cam*AU)/C;observed=sampleHistory(i,simTime-delay);if(observed.mass<=0)continue;historical=true;}V3 posSI=historical?observed.p:visiblePosition(i,cam);V3 pos=posSI/AU;double d=length(pos-cam);double realR=b.radius/AU;double minR=clampd(cameraDistance*0.0025,0.000002,0.18);double radiusAU=std::max(realR,minR);if(b.material==STAR)radiusAU=std::max(realR,minR*1.8);if(d>250)continue;
            Mat4 model=modelMatrix(pos,f(radiusAU),f(std::fmod(b.spin*simTime,2.0*PI)),flattening(b));Mat4 mvp=multiply(vp,model);drawBody(i,b,model,mvp,cam,pos,radiusAU);
        }
        drawParticles(vp);glDisable(GL_CULL_FACE);
    }
    void drawStars(const Mat4&vp){std::vector<float> data;data.reserve(stars.size()*3);V3 offset=cameraFocus();for(const auto&s:stars){V3 p=offset+s;data.insert(data.end(),{f(p.x),f(p.y),f(p.z)});}if(!starVAO){glGenVertexArrays(1,&starVAO);glGenBuffers(1,&starVBO);}glBindVertexArray(starVAO);glBindBuffer(GL_ARRAY_BUFFER,starVBO);glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(float),data.data(),GL_DYNAMIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glUseProgram(starProgram);glUniformMatrix4fv(glGetUniformLocation(starProgram,"uVP"),1,GL_FALSE,vp.m);glUniform1f(glGetUniformLocation(starProgram,"uSize"),2.0f);glDrawArrays(GL_POINTS,0,(GLsizei)stars.size());glBindVertexArray(0);}
    void drawBody(int idx,const Body&b,const Mat4&model,const Mat4&mvp,V3 cam,V3 pos,double renderRadiusAU){
        double bodyDistance=length(cam-pos);double bodyVisualRadius=std::max(b.radius/AU,1e-9);Mesh& activeSphere=(bodyDistance/bodyVisualRadius<24.0)?sphereHigh:sphere;
        V3 lightDir{0,1,0};float lightPower=0;
        for(int j=0;j<(int)bodies.size();j++)if(j!=idx){HistState hs=lightState(j,b.p);if(hs.luminous&&hs.mass>0){V3 d=hs.p-b.p;double dist=std::max(1.0,length(d));double lum=bodies[j].luminosity;if(lum<=0)lum=3.828e26;lightDir=norm(d);lightPower=f(clampd(lum/(4.0*PI*dist*dist)/1361.0,0,4));break;}}
        glUseProgram(surfaceProgram);glUniformMatrix4fv(glGetUniformLocation(surfaceProgram,"uMVP"),1,GL_FALSE,mvp.m);glUniformMatrix4fv(glGetUniformLocation(surfaceProgram,"uModel"),1,GL_FALSE,model.m);glUniform1f(glGetUniformLocation(surfaceProgram,"uTime"),f(std::fmod(simTime,100000.0)));glUniform1f(glGetUniformLocation(surfaceProgram,"uType"),float(b.material));glUniform1f(glGetUniformLocation(surfaceProgram,"uSeed"),float(b.seed));glUniform1f(glGetUniformLocation(surfaceProgram,"uTemperature"),f(b.temperature));glUniform3f(glGetUniformLocation(surfaceProgram,"uLightDir"),f(lightDir.x),f(lightDir.y),f(lightDir.z));glUniform1f(glGetUniformLocation(surfaceProgram,"uLightPower"),std::max(0.0f,lightPower));V3 tidal=tidalVector(idx);glUniform3f(glGetUniformLocation(surfaceProgram,"uTidal"),f(tidal.x),f(tidal.y),f(tidal.z));
        int n=std::min(int(b.craters.size()),MAX_CRATERS);std::array<float,MAX_CRATERS*3> dirs{};std::array<float,MAX_CRATERS*2> data{};for(int k=0;k<n;k++){dirs[k*3]=f(b.craters[k].direction.x);dirs[k*3+1]=f(b.craters[k].direction.y);dirs[k*3+2]=f(b.craters[k].direction.z);data[k*2]=b.craters[k].angularRadius;data[k*2+1]=b.craters[k].depth;}glUniform1i(glGetUniformLocation(surfaceProgram,"uCraterCount"),n);glUniform3fv(glGetUniformLocation(surfaceProgram,"uCraterDir[0]"),MAX_CRATERS,dirs.data());glUniform2fv(glGetUniformLocation(surfaceProgram,"uCraterData[0]"),MAX_CRATERS,data.data());glBindVertexArray(activeSphere.vao);glDrawElements(GL_TRIANGLES,activeSphere.count,GL_UNSIGNED_INT,0);glBindVertexArray(0);
        if(b.hasRings){Mat4 ringModel=modelMatrix(pos,f(std::max(renderRadiusAU*1.35,0.00001)),f(std::fmod(b.spin*simTime,2.0*PI)),flattening(b));Mat4 ringMvp=multiply(multiply(perspective(0.78f,float(width)/height,f(std::max(1.0e-8,cameraDistance*0.0001)),f(std::max(0.02,cameraDistance*3.5))),lookAt(cam,cameraFocus(),V3(0,1,0))),ringModel);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);glUseProgram(ringProgram);glUniformMatrix4fv(glGetUniformLocation(ringProgram,"uMVP"),1,GL_FALSE,ringMvp.m);glUniform1f(glGetUniformLocation(ringProgram,"uTime"),f(std::fmod(simTime,100000.0)));glBindVertexArray(ring.vao);glDrawElements(GL_TRIANGLES,ring.count,GL_UNSIGNED_INT,0);glBindVertexArray(0);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);}
        if(b.hasAtmosphere){float shell=1.035f;Mat4 amodel=modelMatrix(pos,f(std::max(renderRadiusAU*shell,0.000004)),f(std::fmod(b.spin*simTime,2.0*PI)),flattening(b));Mat4 avp=multiply(multiply(perspective(0.78f,float(width)/height,f(std::max(1.0e-8,cameraDistance*0.0001)),f(std::max(0.02,cameraDistance*3.5))),lookAt(cam,cameraFocus(),V3(0,1,0))),amodel);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glUseProgram(atmoProgram);glUniformMatrix4fv(glGetUniformLocation(atmoProgram,"uMVP"),1,GL_FALSE,avp.m);glUniformMatrix4fv(glGetUniformLocation(atmoProgram,"uModel"),1,GL_FALSE,amodel.m);glUniform3f(glGetUniformLocation(atmoProgram,"uCamera"),f(cam.x),f(cam.y),f(cam.z));glUniform1f(glGetUniformLocation(atmoProgram,"uTime"),f(std::fmod(simTime,100000.0)));glUniform1f(glGetUniformLocation(atmoProgram,"uDamage"),b.atmosphereDamage);glUniform1f(glGetUniformLocation(atmoProgram,"uLightPower"),std::max(0.0f,lightPower));glUniform3f(glGetUniformLocation(atmoProgram,"uHoleDir"),f(b.atmosphereHole.x),f(b.atmosphereHole.y),f(b.atmosphereHole.z));glUniform1f(glGetUniformLocation(atmoProgram,"uShockAge"),b.atmosphereEventTime<0?0:f(simTime-b.atmosphereEventTime));if(b.material==GAS||b.material==ICE)glUniform3f(glGetUniformLocation(atmoProgram,"uColor"),0.28f,0.63f,0.94f);else glUniform3f(glGetUniformLocation(atmoProgram,"uColor"),0.18f,0.48f,1.0f);glBindVertexArray(activeSphere.vao);glDrawElements(GL_TRIANGLES,activeSphere.count,GL_UNSIGNED_INT,0);glBindVertexArray(0);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);}
    }
    void drawParticles(const Mat4&vp){if(ejecta.empty())return;std::vector<float> data;data.reserve(ejecta.size()*7);for(const auto&e:ejecta){V3 p=e.p/AU;data.insert(data.end(),{f(p.x),f(p.y),f(p.z),std::min(1.0f,e.temperature/5000.0f),float(e.material),e.size,1.0f});}glBindVertexArray(particleVAO);glBindBuffer(GL_ARRAY_BUFFER,particleVBO);glBufferData(GL_ARRAY_BUFFER,data.size()*sizeof(float),data.data(),GL_DYNAMIC_DRAW);glUseProgram(particleProgram);glUniformMatrix4fv(glGetUniformLocation(particleProgram,"uVP"),1,GL_FALSE,vp.m);glUniform1f(glGetUniformLocation(particleProgram,"uPointScale"),14.0f);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDrawArrays(GL_POINTS,0,(GLsizei)ejecta.size());glDisable(GL_BLEND);glBindVertexArray(0);}

    void touch(int action,float x,float y,float dx,float dy,float scale){(void)x;(void)y;std::lock_guard<std::mutex> lock(mutex);if(action==2||action==3){if(action==3){V3 forward=norm(cameraFocus()-cameraPosition());V3 right=norm(cross(forward,V3(0,1,0)));V3 upv=norm(cross(right,forward));target+=(right*(-dx*cameraDistance*0.002)+upv*(dy*cameraDistance*0.002))*AU;followSelected=false;}if(action==2&&(std::abs(dx)>0.01f||std::abs(dy)>0.01f)){yaw-=dx*0.006f;pitch=clampd(pitch+dy*0.005f,-1.48,1.48);}if(scale>0.001f&&std::abs(scale-1.0f)>0.0001f)cameraDistance=float(clampd(cameraDistance/scale,0.00002,180.0));if(birdMode&&selected>=0&&selected<(int)bodies.size()&&bodies[selected].active)cameraDistance=std::max(cameraDistance,float(bodies[selected].radius/AU*1.015));}}
    void doubleTap(float x,float y){std::lock_guard<std::mutex> lock(mutex);V3 cam=cameraPosition();Mat4 proj=perspective(0.78f,float(width)/height,f(std::max(1.0e-8,cameraDistance*0.0001)),f(std::max(0.02,cameraDistance*3.5)));Mat4 view=lookAt(cam,cameraFocus(),V3(0,1,0));Mat4 vp=multiply(proj,view);double best=0.10;int pick=-1;
        for(int i=0;i<(int)bodies.size();i++)if(bodies[i].active&&bodies[i].visible){V3 p=visiblePosition(i,cam)/AU;float v[4]={f(p.x),f(p.y),f(p.z),1};float eye[4]{};for(int r=0;r<4;r++)for(int k=0;k<4;k++)eye[r]+=vp.m[k*4+r]*v[k];if(eye[3]<=0)continue;float sx=(eye[0]/eye[3]*0.5f+0.5f)*width;float sy=(1.0f-(eye[1]/eye[3]*0.5f+0.5f))*height;double d=std::hypot(sx-x,sy-y)/std::max(width,height);if(d<best){best=d;pick=i;}}
        if(pick>=0){selected=pick;target=bodies[pick].p;followSelected=true;birdMode=false;cameraDistance=std::max(0.0002f,float(bodies[pick].radius/AU*8.0));}}
    void command(int cmd){std::lock_guard<std::mutex> lock(mutex);switch(cmd){case 0:paused=!paused;break;case 1:{if(simSpeed<59)simSpeed=60;else if(simSpeed<599)simSpeed=600;else if(simSpeed<3599)simSpeed=3600;else if(simSpeed<86399)simSpeed=86400;else simSpeed=1;break;}case 2:birdMode=!birdMode;followSelected=true;if(selected>=0&&selected<(int)bodies.size()){target=bodies[selected].p;if(birdMode)cameraDistance=float(std::max(0.00002,bodies[selected].radius/AU*2.6));else cameraDistance=std::max(cameraDistance,0.1f);}break;case 3:addNewPlanet();break;case 9:addMeteor();break;case 4:retardedGravity=!retardedGravity;break;case 5:lightTimeView=!lightTimeView;break;case 7:if(selected>=0&&selected<(int)bodies.size()){bodies[selected].active=false;bodies[selected].mass=0;bodies[selected].visible=false;selected=(selected==0?3:0);followSelected=true;if(selected<(int)bodies.size()&&bodies[selected].active){target=bodies[selected].p;if(selected==3)cameraDistance=std::max(0.0002f,float(bodies[selected].radius/AU*12.0));}}break;case 8:setupSolarSystem();break;}}
    void addNewPlanet(){if(bodies.size()>=MAX_BODIES)return;double rAU=1.8;Body b=makePlanet("Novo planeta",4.5e24,6.0e6,rAU,0.7,ROCK,5.0e-5,260,0.30,0.02,true);b.p=V3(rAU*AU,0,0.6*AU);b.v=V3(-9000,0,24000);bodies.push_back(b);selected=int(bodies.size()-1);target=b.p;followSelected=true;cameraDistance=std::max(0.001f,float(b.radius/AU*12.0));recordHistory();}
    void addMeteor(){
        if(bodies.size()>=MAX_BODIES)return;
        int targetIndex=(selected>=0&&selected<(int)bodies.size()&&bodies[selected].active&&bodies[selected].material!=STAR)?selected:3;
        if(targetIndex<0||targetIndex>=(int)bodies.size()||!bodies[targetIndex].active)return;
        Body&targetBody=bodies[targetIndex];V3 direction=norm(V3(0.78,0.16,-0.60));V3 tangent=norm(cross(direction,V3(0,1,0)));
        Body meteor;meteor.name="Asteroide (10 km)";meteor.mass=1.3e15;meteor.radius=5000;meteor.material=ROCK;meteor.temperature=220;meteor.ironFraction=0.18;meteor.iceFraction=0.03;meteor.seed=uint32_t(bodies.size()*991+411);meteor.p=targetBody.p+direction*(targetBody.radius+4.0e7);meteor.v=targetBody.v-direction*16500.0+tangent*2500.0;meteor.active=true;meteor.visible=true;selected=targetIndex;target=targetBody.p;followSelected=true;cameraDistance=std::max(0.0002f,float(targetBody.radius/AU*12.0));bodies.push_back(meteor);recordHistory();
    }
    void setParams(float massMul,float radiusMul,float spinMul){std::lock_guard<std::mutex> lock(mutex);if(selected<0||selected>=(int)bodies.size()||!bodies[selected].active)return;Body&b=bodies[selected];double oldMass=b.mass;b.mass*=clampd(massMul,0.05,5);b.radius*=clampd(radiusMul,0.05,5);b.spin*=clampd(spinMul,0,5);if(oldMass>0)b.temperature*=std::pow(b.mass/oldMass,0.08);recordHistory();}
    std::string status(){std::lock_guard<std::mutex> lock(mutex);char out[1024];const Body*b=(selected>=0&&selected<(int)bodies.size())?&bodies[selected]:nullptr;double massEarth=b&&b->mass>0?b->mass/5.972e24:0;double radiusKm=b?b->radius/1000:0;double dayHours=b&&std::abs(b->spin)>1e-12?std::abs(2*PI/b->spin/3600):0;const char*grav=retardedGravity?"c retardada (experimental)":"Newton estável";const char*view=lightTimeView?"cone de luz":"posição atual";double iron=b?b->ironFraction*100.0:0,ice=b?b->iceFraction*100.0:0;const char*atmo=b&&b->hasAtmosphere?"atmosfera":"sem atmosfera";std::snprintf(out,sizeof(out),"%s | t = %.2f dias | ×%.0f\nFoco: %s | massa %.3g Terras | raio %.0f km | T %.0f K\nGrav.: %s | Luz: %s | giro %.2f h | Fe %.0f%% gelo %.0f%% | %s | N %zu / detritos %zu",
            paused?"PAUSADO":"SIMULANDO",simTime/DAY,simSpeed,b?b->name.c_str():"nenhum",massEarth,radiusKm,b?b->temperature:0,grav,view,dayHours,iron,ice,atmo,bodies.size(),ejecta.size());return out;}
};

static Engine engine;
}

using namespace nebula;

extern "C" JNIEXPORT void JNICALL Java_com_nebula_universelab_MainActivity_nativeInit(JNIEnv*, jobject, jint w, jint h){engine.init(w,h);}
extern "C" JNIEXPORT void JNICALL Java_com_nebula_universelab_MainActivity_nativeResize(JNIEnv*, jobject, jint w, jint h){engine.resize(w,h);}
extern "C" JNIEXPORT void JNICALL Java_com_nebula_universelab_MainActivity_nativeRender(JNIEnv*, jobject, jfloat dt){engine.update(dt);engine.draw(engine.width,engine.height);}
extern "C" JNIEXPORT void JNICALL Java_com_nebula_universelab_MainActivity_nativeTouch(JNIEnv*, jobject, jint action, jfloat x, jfloat y, jfloat dx, jfloat dy, jfloat scale){engine.touch(action,x,y,dx,dy,scale);}
extern "C" JNIEXPORT void JNICALL Java_com_nebula_universelab_MainActivity_nativeDoubleTap(JNIEnv*, jobject, jfloat x, jfloat y){engine.doubleTap(x,y);}
extern "C" JNIEXPORT void JNICALL Java_com_nebula_universelab_MainActivity_nativeCommand(JNIEnv*, jobject, jint cmd){engine.command(cmd);}
extern "C" JNIEXPORT jstring JNICALL Java_com_nebula_universelab_MainActivity_nativeGetStatus(JNIEnv* env, jobject){std::string s=engine.status();return env->NewStringUTF(s.c_str());}
extern "C" JNIEXPORT void JNICALL Java_com_nebula_universelab_MainActivity_nativeSetParameters(JNIEnv*, jobject, jfloat m, jfloat r, jfloat s){engine.setParams(m,r,s);}
