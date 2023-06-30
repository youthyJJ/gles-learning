#version 300 es
uniform sampler2D texture0;
uniform sampler2D texture1;

in vec2 TexCoord;
in vec3 vertexColor;

out vec4 FragColor;

void main() {
    // FragColor = texture(texture0, TexCoord) * vec4(vertexColor, 1.0F);
    vec4 t0 = texture(texture0, TexCoord);
    vec4 t1 = texture(texture1, TexCoord);
//    // 左右翻转
//    vec2 horiztalFlip = vec2(1.0 - TexCoord.s, TexCoord.t);
//    vec4 t1 = texture(texture1, horiztalFlip);
    FragColor = mix(t0, t1, 0.2F);
}