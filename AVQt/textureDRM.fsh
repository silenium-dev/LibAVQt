// fragment shader

#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

in vec4 fragColor;// input: interpolated color as rgba-value
in vec2 texCoord;// input: texture coordinate (xy-coordinates)
out vec4 finalColor;// output: final color value as rgba-value

uniform mediump sampler2D texY;
uniform mediump sampler2D texU;
uniform mediump sampler2D texV;
uniform int inputFormat;

void main() {
    vec2 newTexCoord = vec2(1.0f - texCoord.x, texCoord.y);

    if (inputFormat == 0) { // RGB(A)
        vec3 rgb = texture(texY, newTexCoord.st).rgb;
        finalColor = vec4(rgb, 1.0);
    } else if (inputFormat == 1) { // YUV 4:2:0 with Y plane interleaved UV plane (e.g. NV12 or P010)
        vec3 yuv;

        // We had put the Y values of each pixel to the R component by using GL_RED
        yuv.x = texture(texY, newTexCoord.st).r - 0.0625;
        // We had put the U and V values of each pixel to the R and G components of the
        // texture respectively using GL_RG. Since U, V bytes are interspread
        // in the tex, this is probably the fastest way to use them in the shader
        yuv.y = texture(texU, newTexCoord.st).r - 0.5;
        yuv.z = texture(texU, newTexCoord.st).g - 0.5;
        // The numbers are just YUV to RGB conversion constants
        vec3 rgb = mat3(1.0, 1.0, 1.0,
        0.0, -0.39465, 2.03211,
        1.13983, -0.58060, 0.0) * yuv;
        finalColor = vec4(rgb, 1.0f);
    } else if (inputFormat == 2) { // YUV420P
        vec3 yuv;

        // We had put the Y values of each pixel to the R component by using GL_RED
        yuv.x = texture(texY, newTexCoord.st).r - 0.0625;
        // We had put the U and V values of each pixel to the R and G components of the
        // texture respectively using GL_RG. Since U, V bytes are interspread
        // in the tex, this is probably the fastest way to use them in the shader
        yuv.y = texture(texU, newTexCoord.st).r - 0.5;
        yuv.z = texture(texV, newTexCoord.st).r - 0.5;
        // The numbers are just YUV to RGB conversion constants
        vec3 rgb = mat3(1.0, 1.0, 1.0,
        0.0, -0.39465, 2.03211,
        1.13983, -0.58060, 0.0) * yuv;
        finalColor = vec4(rgb, 1.0f);
    } else if (inputFormat == 3) { // YUV420P10
        vec3 yuv;

        // Multiply every value with 2^6 = 64, because the values are stored in the lower 10 bits,
        // but have to be in the upper 10 bits for correct calculation
        // We had put the Y values of each pixel to the R component of every texture by using GL_RED
        yuv.x = texture(texY, newTexCoord.st).r * 64.0 - 0.0625;
        yuv.y = texture(texU, newTexCoord.st).r * 64.0 - 0.5;
        yuv.z = texture(texU, newTexCoord.st).r * 64.0 - 0.5;
        // The numbers are just YUV to RGB conversion constants
        vec3 rgb = mat3(1.0, 1.0, 1.0,
        0.0, -0.39465, 2.03211,
        1.13983, -0.58060, 0.0) * yuv;

        finalColor = vec4(rgb, 1.0);
    }
}
