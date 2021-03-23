#version 330 core

// fragment shader

in vec4 fragColor;// input: interpolated color as rgba-value
in vec2 texCoord;// input: texture coordinate (xy-coordinates)
out vec4 finalColor;// output: final color value as rgba-value

uniform sampler2D textureY;
uniform sampler2D textureUV;

void main() {
    float r, g, b, y, u, v;

    vec3 yuv;
    vec2 newTexCoord = vec2(1.0f - texCoord.x, texCoord.y);

    // We had put the Y values of each pixel to the R,G,B components by GL_LUMINANCE,
    // that's why we're pulling it from the R component, we could also use G or B
    yuv.x = texture(textureY, newTexCoord.st).r - 0.0625;
    // We had put the U and V values of each pixel to the G and R components of the
    // texture respectively using GL_RG. Since U,V bytes are interspread
    // in the texture, this is probably the fastest way to use them in the shader
    yuv.y = texture(textureUV, newTexCoord.st).r - 0.5;
    yuv.z = texture(textureUV, newTexCoord.st).g - 0.5;
    // The numbers are just YUV to RGB conversion constants
    vec3 rgb = mat3(1, 1, 1,
    0, -0.39465, 2.03211,
    1.13983, -0.58060, 0) * yuv;

    finalColor = vec4(rgb, 1.0);
}
