#version 400

in vec3 tri;
uniform mat4 proj, view;

void main () {
	gl_Position = proj * view * vec4 (tri, 1.0);
}