// vertex shader

#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

layout(location = 0) in vec3 position;// input:  attribute with index '0' with 3 elements per vertex
//layout(location = 1) in vec3 color;      // input:  attribute with index '1' with 3 elements (=rgb) per vertex
layout(location = 1) in vec2 texcoords;// input:  attribute with index '2' with 2 elements per vertex
//layout(location = 2) in float texnr;     // input:  attribute with index '3' with 1 float per vertex

out vec4 fragColor;// output: computed fragmentation color
out vec2 texCoord;// output: computed texture coordinates
//flat out float texID;                    // output: texture ID - mind the 'flat' attribute!

void main() {
  // Mind multiplication order for matrixes
  gl_Position = vec4(position, 1.0);
  fragColor = vec4(0.5, 1.0, 0.3, 1.0);
  texCoord = texcoords;
//  texID = texnr;
}