#include <jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
extern int wumpa_memory_probe(void);
JNIEXPORT jstring JNICALL Java_org_wumpaforge_shield_LauncherActivity_nativeDiagnostics(JNIEnv *env,jclass cls)
{
    (void)cls;
    char result[4096]; size_t used=0;
    int memory=wumpa_memory_probe();
    used+=(size_t)snprintf(result,sizeof(result),"Sparse memory aliases: %s\nHost page size: %ld\n",memory==0?"PASS":"FAIL",sysconf(_SC_PAGESIZE));
    EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLContext context=EGL_NO_CONTEXT; EGLSurface surface=EGL_NO_SURFACE;
    EGLint major=0,minor=0; const char *failure="eglInitialize";
    if(display==EGL_NO_DISPLAY || !eglInitialize(display,&major,&minor)) goto done;
    used+=(size_t)snprintf(result+used,sizeof(result)-used,"EGL: %d.%d, client APIs: %s\n",major,minor,eglQueryString(display,EGL_CLIENT_APIS));
    failure="desktop OpenGL API unavailable";
    if(!eglBindAPI(EGL_OPENGL_API)) goto done;
    const EGLint configAttrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_STENCIL_SIZE,8,EGL_NONE};
    EGLConfig config; EGLint count=0;
    failure="desktop pbuffer configuration";
    if(!eglChooseConfig(display,configAttrs,&config,1,&count)||count!=1)goto done;
    const EGLint contextAttrs[]={EGL_CONTEXT_MAJOR_VERSION_KHR,4,EGL_CONTEXT_MINOR_VERSION_KHR,1,EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,EGL_NONE};
    failure="desktop GL 4.1 core context";
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttrs); if(context==EGL_NO_CONTEXT)goto done;
    const EGLint surfaceAttrs[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};
    failure="pbuffer creation/bind";
    surface=eglCreatePbufferSurface(display,config,surfaceAttrs);
    if(surface==EGL_NO_SURFACE || !eglMakeCurrent(display,surface,surface,context))goto done;
    PFNGLGETSTRINGPROC getString=(PFNGLGETSTRINGPROC)eglGetProcAddress("glGetString");
    PFNGLCLEARCOLORPROC clearColor=(PFNGLCLEARCOLORPROC)eglGetProcAddress("glClearColor");
    PFNGLCLEARPROC clear=(PFNGLCLEARPROC)eglGetProcAddress("glClear");
    PFNGLREADPIXELSPROC readPixels=(PFNGLREADPIXELSPROC)eglGetProcAddress("glReadPixels");
    PFNGLGETERRORPROC getError=(PFNGLGETERRORPROC)eglGetProcAddress("glGetError");
    failure="public eglGetProcAddress desktop entry points";
    if(!getString||!clearColor||!clear||!readPixels||!getError)goto done;
    used+=(size_t)snprintf(result+used,sizeof(result)-used,"GL: %.200s\nGLSL: %.200s\nRenderer: %.200s\n",getString(GL_VERSION),getString(GL_SHADING_LANGUAGE_VERSION),getString(GL_RENDERER));
    clearColor(0,1,0,1);clear(GL_COLOR_BUFFER_BIT);
    unsigned char pixel[4]={0};readPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    failure="desktop GL green pixel round trip";
    if(getError()!=GL_NO_ERROR||pixel[0]!=0||pixel[1]!=255||pixel[2]!=0||pixel[3]!=255)goto done;
    failure=NULL;
done:
    snprintf(result+used,sizeof(result)-used,"Desktop GL probe: %s%s\nThis checks prerequisites only, not game rendering or playability.",failure?"FAIL — ":"PASS",failure?failure:"");
    if(display!=EGL_NO_DISPLAY){
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);
        if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);
        eglTerminate(display);
    }
    return (*env)->NewStringUTF(env,result);
}
