#version 330 core

// fragment shader

in vec4 fragColor;// input: interpolated color as rgba-value
in vec2 texCoord;// input: texture coordinate (xy-coordinates)
out vec4 finalColor;// output: final color value as rgba-value

uniform sampler2D textureY;
uniform sampler2D textureUV;

// 0 == RGB(A) (No conversion necessary)
// 1 == YUV (Y plane, UV interleaved plane; Standard output format of VAAPI,
//           either 8-bit or 16-bit per pixel and plane,
//           the lower bits are filled with 0, if the pixel format has 10 or 12 bit)
// TODO: Other formats like YUV interleaved or three seperate planes for each channel (both RGB(A) and YUV)
uniform int inputFormat;

void main() {
    vec3 rgb;
    vec2 newTexCoord = vec2(1.0f - texCoord.x, texCoord.y);

    if (inputFormat == 0) {
        finalColor = texture(textureY, newTexCoord);
    } else if (inputFormat == 1) {
        vec3 yuv;

        // We had put the Y values of each pixel to the R component by using GL_RED
        yuv.x = texture(textureY, newTexCoord.st).r - 0.0625;
        // We had put the U and V values of each pixel to the R and G components of the
        // texture respectively using GL_RG. Since U, V bytes are interspread
        // in the texture, this is probably the fastest way to use them in the shader
        yuv.y = texture(textureUV, newTexCoord.st).r - 0.5;
        yuv.z = texture(textureUV, newTexCoord.st).g - 0.5;
        // The numbers are just YUV to RGB conversion constants
        rgb = mat3(1, 1, 1,
        0, -0.39465, 2.03211,
        1.13983, -0.58060, 0) * yuv;
        finalColor = vec4(rgb, 1.0);
    }
}
