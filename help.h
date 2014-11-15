#ifndef _HELP_H_
#define _HELP_H_

#define GL_LOG_FILE "GL.log"
#define SEPARATOR "////////////////////////////////////////////////////////////////////////////////"

#include <stdlib.h>
#include <stdio.h>

int g_gl_width = 1;
int g_gl_height = 1;

float kViewWidth = 6.;
float kViewHeight;
float kViewDepth = 6.;

float cam_pos[3]; // don't start at zero, or we will be too close
float cam_speed = 20.0f; // 1 unit per second

double previous_seconds;
double elapsedseconds;

GLfloat vertices[] = {
    0.0f, 1.0f, -2.0f,
    -1.0f, -1.0f, -2.0f,
    1.0f, -1.0f, -2.0f
};

GLuint vertexArray;

GLuint tri_view_mat_location;
GLuint tri_proj_mat_location;
GLuint tri_location;

bool gl_log(const char* message, const char* filename, int line) {
	FILE* file = fopen(GL_LOG_FILE, "a+");
	if (!file) {
		fprintf(stderr, "ERROR: could not open %s for logging\n", GL_LOG_FILE);
		return false;
	}

	// fprintf(file, "%s: %i %s\n", filename, line, message);
	fprintf(file, "%s\n", message);	
	fclose(file);
	return true;
}

void glfw_error_callback(int error, const char* description) {
	fputs(description, stderr);
	gl_log(description, __FILE__, __LINE__);
}

void print_shader_info_log(GLuint shader_index)
{
    int max_length = 2048;

    int actual_length = 0;

    char log[2048];
    glGetShaderInfoLog(shader_index, max_length, &actual_length, log);
    
    printf("shader info log for GL index %u:\n%s\n", shader_index, log);
}

void print_shader_program_info_log(GLuint shader_program_index)
{
    int max_length = 2048;
    
    int actual_length = 0;
    char log[2048];
    glGetProgramInfoLog(shader_program_index, max_length, &actual_length, log);
    
    printf("shader program info log for GL index %u:\n%s\n", shader_program_index, log);
}

char* readShader(char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        fprintf(stderr, "Cannot open file %s\n", filename);
        exit(1);
    }

    char* shaderStr;
    long fileLen;

    fseek(file, 0, SEEK_END);
    fileLen = ftell(file);
    rewind(file);
    shaderStr = (char*)malloc((fileLen + 1) * sizeof(char));
    fread(shaderStr, sizeof(char), fileLen, file);
    fclose(file);
    shaderStr[fileLen] = 0;

    return shaderStr;
}

#endif