#version 330 core

in vec3 vColor;
in vec2 vUV;

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0) * texture(uTexture, vUV);
}
