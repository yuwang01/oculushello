#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <OpenGL/OpenGL.h>
#include <mach/mach_time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/stat.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "maths_funcs.h"

#ifdef WIN32
#define OVR_OS_WIN32
#elif defined(__APPLE__)
#define OVR_OS_MAC
#else
#define OVR_OS_LINUX
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

#include "help.h"

#include "../OculusSDK/LibOVR/Include/OVR.h"
#include "../OculusSDK/LibOVR/Src/OVR_CAPI.h"
#include "../OculusSDK/LibOVR/Src/OVR_CAPI_GL.h"
#include "../OculusSDK/LibOVR/Src/CAPI/CAPI_HSWDisplay.h"

static bool debug = false;
static GLFWwindow* window;
static ovrHmd hmd;
static ovrSizei eyeres[2];
static ovrEyeRenderDesc eye_rdesc[2];
static ovrGLTexture fb_ovr_tex[2];
static union ovrGLConfig glcfg;

static unsigned int hmd_caps;
static unsigned int distort_caps;

static int fb_width, fb_height;
static unsigned int fbo, fb_tex, fb_depth;
static int fb_tex_width, fb_tex_height;

static unsigned int cube_program;

static int initGLVR(void);
static void cleanup(void);
static void reshape(int, int);
static void update_rtarg(int, int);
static unsigned int next_pow2(unsigned int);
static void quat_to_matrix(const float*, float*);
static void KeyCallback(GLFWwindow*, int, int, int, int);

/* forward declaration to avoid including non-public headers of libovr */
OVR_EXPORT void ovrhmd_EnableHSWDisplaySDKRender(ovrHmd hmd, ovrBool enable);

int main(int argc, char **argv)
{   
    int eyeIndex;
    ovrPosef pose[2];

    cam_pos[0] = 0.0f;
    cam_pos[1] = 0.0f;
    cam_pos[2] = 1.2*kViewDepth;

    if (initGLVR() == -1)
    {
        fprintf(stderr, "Error: init()\n");
        return EXIT_FAILURE;
    }

    while (!glfwWindowShouldClose(window))
    {
        // the drawing starts with a call to ovrHmd_BeginFrame 
        ovrHmd_BeginFrame(hmd, 0);

        // start drawing onto our texture render target 
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //for each eye ...
        for(eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++) 
        {
            ovrEyeType eye = hmd->EyeRenderOrder[eyeIndex];
            pose[eye] = ovrHmd_GetHmdPosePerEye(hmd, eye);

            OVR::Matrix4f l_ProjectionMatrix = ovrMatrix4f_Projection(hmd->DefaultEyeFov[eye], 0.5, 500.0, 1);
            OVR::Quatf l_Orientation = OVR::Quatf(pose[eye].Orientation);
            OVR::Matrix4f l_ModelViewMatrix = OVR::Matrix4f(l_Orientation.Inverted());

            glUseProgram(cube_program);
            glUniformMatrix4fv(cube_proj_mat_location, 1, GL_FALSE, &(l_ProjectionMatrix.Transposed().M[0][0]));
            glUniformMatrix4fv(cube_view_mat_location, 1, GL_FALSE, &(l_ModelViewMatrix.Transposed().M[0][0]));
            glViewport(eye == ovrEye_Left ? 0 : fb_width / 2, 0, fb_width / 2, fb_height);

            glDrawArrays(GL_TRIANGLES, 0, 3);

        }

        glBindVertexArray(0);

        ovrHmd_EndFrame(hmd, pose, &fb_ovr_tex[0].Texture);

        glBindVertexArray(vertexArray);

        glfwPollEvents();

        // glfwSwapBuffers(window);

    }

    glfwTerminate();
    
    return 0;
}

int initGLVR(void)
{
    fprintf(stdout, "%s\n", SEPARATOR);

    int i, x, y;
    unsigned int flags;

    /* libovr must be initialized before we create the OpenGL context */
    ovr_Initialize();

    if (!glfwInit()) {
        fprintf(stderr, "ERROR: could not start GLFW3\n");
        return 1;
    }
    
    char message[256];
    sprintf(message, "starting GLFW %s", glfwGetVersionString());
    assert(gl_log(message, __FILE__, __LINE__));
    glfwSetErrorCallback(glfw_error_callback);

    // uncomment these lines if on Apple OS X
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    glfwWindowHint(GLFW_SAMPLES, 4);

    // GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    // const GLFWvidmode* vmode = glfwGetVideoMode(monitor);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vmode = glfwGetVideoMode(monitor);

    window = glfwCreateWindow(g_gl_width, g_gl_height, "SPH - Oculus", NULL, NULL);
    glfwSetWindowPos(window, vmode->width, vmode->height);

    if (!window) {
        fprintf(stderr, "ERROR: could not open window with GLFW3\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, KeyCallback);

    // start GLEW extension handler
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        printf("Error: OpenGL init failed\n");
        return EXIT_FAILURE;
    }

    if ((hmd = ovrHmd_Create(0)) == NULL)
    {
        fprintf(stderr, "Error: failed to open Oculus HMD, falling back to virtual debug HMD\n");
        if(!(hmd = ovrHmd_CreateDebug(ovrHmd_DK2))) {
            fprintf(stderr, "Error: failed to create virtual debug HMD\n");
            return -1;
        }
        else
        {
            fprintf(stdout, "Log: using virtual debug HMD\n");
            fprintf(stdout, "initialized HMD: %s - %s\n", hmd->Manufacturer, hmd->ProductName);
            debug = true;
        }
    }

    glfwSetWindowSize(window, hmd->Resolution.w, hmd->Resolution.h);
    
    g_gl_width = hmd->Resolution.w;
    g_gl_height = hmd->Resolution.h;

    /* enable position and rotation tracking */
    ovrHmd_ConfigureTracking(hmd, ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position, 0);

    /* retrieve the optimal render target resolution for each eye */
    eyeres[0] = ovrHmd_GetFovTextureSize(hmd, ovrEye_Left, hmd->DefaultEyeFov[0], 1.0);
    eyeres[1] = ovrHmd_GetFovTextureSize(hmd, ovrEye_Right, hmd->DefaultEyeFov[1], 1.0);

    /* and create a single render target texture to encompass both eyes */
    fb_width = eyeres[0].w + eyeres[1].w;
    fb_height = eyeres[0].h > eyeres[1].h ? eyeres[0].h : eyeres[1].h;
    update_rtarg(fb_width, fb_height);

    /* fill in the ovrGLTexture structures that describe our render target texture */
    for(i=0; i<2; i++) {
        fb_ovr_tex[i].OGL.Header.API = ovrRenderAPI_OpenGL;
        fb_ovr_tex[i].OGL.Header.TextureSize.w = fb_tex_width;
        fb_ovr_tex[i].OGL.Header.TextureSize.h = fb_tex_height;
        /* this next field is the only one that differs between the two eyes */
        fb_ovr_tex[i].OGL.Header.RenderViewport.Pos.x = i == 0 ? 0 : fb_width / 2.0;
        fb_ovr_tex[i].OGL.Header.RenderViewport.Pos.y = 0;
        fb_ovr_tex[i].OGL.Header.RenderViewport.Size.w = fb_width / 2.0;
        fb_ovr_tex[i].OGL.Header.RenderViewport.Size.h = fb_height;
        fb_ovr_tex[i].OGL.TexId = fb_tex;   /* both eyes will use the same texture id */
    }

    /* fill in the ovrGLConfig structure needed by the SDK to draw our stereo pair
     * to the actual HMD display (SDK-distortion mode)
     */
    memset(&glcfg, 0, sizeof(glcfg));
    glcfg.OGL.Header.API = ovrRenderAPI_OpenGL;
    glcfg.OGL.Header.RTSize = hmd->Resolution;
    glcfg.OGL.Header.Multisample = 1;

#ifdef WIN32
    glcfg.OGL.Window = GetActiveWindow();
    glcfg.OGL.DC = wglGetCurrentDC();
#elif defined(__linux__)
    glcfg.OGL.Win = glfwGetX11Window(window);
    glcfg.OGL.Disp = glfwGetX11Display();
#endif

    if(hmd->HmdCaps & ovrHmdCap_ExtendDesktop)
    {
        printf("running in \"extended desktop\" mode\n");
    }
    else
    {
        /* to sucessfully draw to the HMD display in "direct-hmd" mode, we have to
         * call ovrHmd_AttachToWindow
         * XXX: this doesn't work properly yet due to bugs in the oculus 0.4.1 sdk/driver
         */
#ifdef WIN32
        ovrHmd_AttachToWindow(hmd, glcfg.OGL.Window, 0, 0);
#elif defined(__linux__)
        ovrHmd_AttachToWindow(hmd, (void*)glcfg.OGL.Win, 0, 0);
#endif
        printf("running in \"direct-hmd\" mode\n");
    }

    /* enable low-persistence display and dynamic prediction for lattency compensation */
    hmd_caps = ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction;
    ovrHmd_SetEnabledCaps(hmd, hmd_caps);

    /* configure SDK-rendering and enable chromatic abberation correction, vignetting, and
     * timewrap, which shifts the image before drawing to counter any lattency between the call
     * to ovrHmd_GetEyePose and ovrHmd_EndFrame.
     */
    distort_caps = ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette | ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive;

    if(!ovrHmd_ConfigureRendering(hmd, &glcfg.Config, distort_caps, hmd->DefaultEyeFov, eye_rdesc)) {
        fprintf(stderr, "failed to configure distortion renderer\n");
    }

    /* disable the retarded "health and safety warning" */
    // ovrhmd_EnableHSWDisplaySDKRender(hmd, 0); // not working as of Nov 7, 2014

    fprintf(stdout, "%s\n", SEPARATOR);
    ////////////////////////////////////////////////////////////////////////////////
    const char* vertex_shader_cube = readShader("shaders/vs_tri.glsl");
    // printf("\ncube vertex shader: \n%s\n", vertex_shader_cube);

    unsigned int vs_cube = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs_cube, 1, &vertex_shader_cube, NULL);
    glCompileShader(vs_cube);
    print_shader_info_log(vs_cube);

    const char* fragment_shader_cube = readShader("shaders/fs_tri.glsl");
    // printf("\ncube fragment shader: \n%s\n", fragment_shader_cube);

    unsigned int fs_cube = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs_cube, 1, &fragment_shader_cube, NULL);
    glCompileShader(fs_cube);
    print_shader_info_log(fs_cube);

    cube_program = glCreateProgram();
    glAttachShader(cube_program, fs_cube);
    glAttachShader(cube_program, vs_cube);
    glLinkProgram(cube_program);

    print_shader_program_info_log(cube_program);
    fprintf(stdout, "%s\n", SEPARATOR);
    ////////////////////////////////////////////////////////////////////////////////

    cube_view_mat_location = glGetUniformLocation(cube_program, "view");
    cube_proj_mat_location = glGetUniformLocation(cube_program, "proj");
    cube_location = glGetAttribLocation(cube_program, "cube");

    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    GLuint positionBuffer;
    glGenBuffers(1, &positionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(cube_location, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(cube_location);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    // glClearColor(.8f, .8f, .8f, 1.0f);

    glClearDepth(1.0f);

    return 0;
}

void cleanup(void)
{
    if(hmd) {
        ovrHmd_Destroy(hmd);
    }
    ovr_Shutdown();
}

void reshape(int width, int height) {
    g_gl_width = width;
    g_gl_height = height;
}

void update_rtarg(int width, int height)
{
    if (!fbo)
    {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &fb_tex);
        glGenRenderbuffers(1, &fb_depth);

        glBindTexture(GL_TEXTURE_2D, fb_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    fb_tex_width = next_pow2(width);
    fb_tex_height = next_pow2(height);

    /* create and attach the texture that will be used as a color buffer */
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fb_tex_width, fb_tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_tex, 0);

    /* create and attach the renderbuffer that will serve as our z-buffer */
    glBindRenderbuffer(GL_RENDERBUFFER, fb_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fb_tex_width, fb_tex_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb_depth);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Error: incomplete framebuffer!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fprintf(stdout, "Log: created render target: %dx%d (texture size: %dx%d)\n", width, height, fb_tex_width, fb_tex_height);

}

unsigned int next_pow2(unsigned int x)
{
    x -= 1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

static void KeyCallback(GLFWwindow* p_Window, int p_Key, int p_Scancode, int p_Action, int p_Mods)
{
    if (p_Key == GLFW_KEY_ESCAPE && p_Action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(p_Window, GL_TRUE);
    } else
    {
        ovrhmd_EnableHSWDisplaySDKRender(hmd, 0);
        if (p_Key == GLFW_KEY_A && p_Action == GLFW_PRESS)
        {
            fprintf(stdout, "A pressed\n");
        }
    }
}
