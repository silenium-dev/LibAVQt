#version 300 es

// GLSL version 3.3
// vertex shader

layout(location = 0) in mediump vec3 position;// input:  attribute with index '0' with 3 elements per vertex
layout(location = 1) in mediump vec2 texcoords;// input:  attribute with index '2' with 2 elements per vertex

out mediump vec4 fragColor;// output: computed fragmentation color
out mediump vec2 texCoord;// output: computed texture coordinates

void main() {
    gl_Position = vec4(position, 1.0);
    fragColor = vec4(0.5, 1.0, 0.3, 1.0);
    texCoord = texcoords;
}