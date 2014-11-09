#version 400

in vec3 cube;
uniform mat4 proj, view;

void main () {
	gl_Position = proj * view * vec4 (cube, 1.0);
}