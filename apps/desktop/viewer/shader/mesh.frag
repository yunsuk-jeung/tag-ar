#version 330 core

in vec3 vColor;
in vec2 vUV;

uniform bool uUseTexture;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    if (uUseTexture) {
        FragColor = texture(uTexture, vUV);
    } else {
        FragColor = vec4(vColor, 1.0);
    }
}
