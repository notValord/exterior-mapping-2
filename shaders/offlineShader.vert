#version 450

vec2 positions[4] = vec2[](
    vec2(-1.0,  1.0),   // top left
    vec2( 1.0,  1.0),   // top right
    vec2(-1.0, -1.0),   // bottom left
    vec2( 1.0, -1.0)    // bottom right
);

vec2 uvs[4] = vec2[](
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 0.0)
);

const int quadIndices[6] = int[](
    0, 2, 3,
    3, 1, 0
);

layout(location = 0) out vec2 texCoords;

void main() {
    int idx = quadIndices[gl_VertexIndex];

    mat4 y_flip = mat4(1.0f);
    y_flip[1][1] *= -1;
    
    gl_Position = y_flip * vec4(positions[idx], 0.0, 1.0);
    texCoords = vec2(uvs[idx].x, 1.0f-uvs[idx].y);      // and flip the Y coord to corresctly sample the images
}