#version 300 es

// fragment shader

in mediump vec4 fragColor;// input: interpolated color as rgba-value
in mediump vec2 texCoord;// input: texture coordinate (xy-coordinates)
out mediump vec4 finalColor;// output: final color value as rgba-value

uniform sampler2D textureY;
uniform sampler2D textureU;
uniform sampler2D textureV;
uniform int inputFormat;

void main() {
    mediump vec2 newTexCoord = vec2(1.0f - texCoord.x, texCoord.y);

    if (inputFormat == 0) { // RGB(A)
        mediump vec3 rgb = texture(textureY, newTexCoord).rgb;
        finalColor = vec4(rgb, 1.0);
    } else if (inputFormat == 1) { // YUV 4:2:0 with Y plane interleaved UV plane (e.g. NV12 or P010)
        mediump vec3 yuv;

        // We had put the Y values of each pixel to the R component by using GL_RED
        yuv.x = texture(textureY, newTexCoord.st).r - 0.0625;
        // We had put the U and V values of each pixel to the R and G components of the
        // texture respectively using GL_RG. Since U, V bytes are interspread
        // in the texture, this is probably the fastest way to use them in the shader
        yuv.y = texture(textureU, newTexCoord.st).r - 0.5;
        yuv.z = texture(textureU, newTexCoord.st).g - 0.5;
        // The numbers are just YUV to RGB conversion constants
        mediump vec3 rgb = mat3(1, 1, 1,
        0, -0.39465, 2.03211,
        1.13983, -0.58060, 0) * yuv;
        finalColor = vec4(rgb, 1.0f);
    } else if (inputFormat == 2) { // YUV420P
        mediump vec3 yuv;

        // We had put the Y values of each pixel to the R component by using GL_RED
        yuv.x = texture(textureY, newTexCoord.st).r - 0.0625;
        // We had put the U and V values of each pixel to the R and G components of the
        // texture respectively using GL_RG. Since U, V bytes are interspread
        // in the texture, this is probably the fastest way to use them in the shader
        yuv.y = texture(textureU, newTexCoord.st).r - 0.5;
        yuv.z = texture(textureV, newTexCoord.st).r - 0.5;
        // The numbers are just YUV to RGB conversion constants
        mediump vec3 rgb = mat3(1, 1, 1,
        0, -0.39465, 2.03211,
        1.13983, -0.58060, 0) * yuv;
        finalColor = vec4(rgb, 1.0f);
    } else if (inputFormat == 3) { // YUV420P10
        mediump vec3 yuv;

        // Multiply every value with 2^6 = 64, because the values are stored in the lower 10 bits,
        // but have to be in the upper 10 bits for correct calculation
        // We had put the Y values of each pixel to the R component of every texture by using GL_RED
        yuv.x = texture(textureY, newTexCoord.st).r * 64 - 0.0625;
        yuv.y = texture(textureU, newTexCoord.st).r * 64 - 0.5;
        yuv.z = texture(textureV, newTexCoord.st).r * 64 - 0.5;
        // The numbers are just YUV to RGB conversion constants
        mediump vec3 rgb = mat3(1, 1, 1,
        0, -0.39465, 2.03211,
        1.13983, -0.58060, 0) * yuv;

        finalColor = vec4(rgb, 1.0);
    }
}