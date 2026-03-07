#version 450

const vec3 cubeVertices[8] = vec3[8](
    vec3(-0.1,  0.1, -0.1),     // top left back
    vec3( 0.1,  0.1, -0.1),     // top right back
    vec3(-0.1, -0.1, -0.1),     // bottom left back
    vec3( 0.1, -0.1, -0.1),     // bottom right back
    vec3(-0.1,  0.1,  0.1),     // top left front
    vec3( 0.1,  0.1,  0.1),     // top right front
    vec3(-0.1, -0.1,  0.1),     // bottom lefr front
    vec3( 0.1, -0.1,  0.1)      // bottom right front
);

const vec2 cubeTexCoords[8] = vec2[8](
    vec2(0.0,  0.0),    // top left back
    vec2(1.0,  0.0),    // top right back
    vec2(0.0,  1.0),    // bottom lefr back
    vec2(1.0,  1.0),    // bottom right back
    vec2(-1.0, -1.0),   // top left front
    vec2(2.0,  -1.0),   // top right front
    vec2(-1.0, 2.0),    // bottom left front
    vec2(2.0,  2.0)     // bottom right front
);

const int cubeIndices[36] = int[36](
    // Back face
    0, 1, 3, 3, 2, 0,
    
    // Front face
    4, 7, 5, 7, 4, 6,
    
    // Left face
    0, 6, 4, 6, 0, 2,
    
    // Right face
    5, 3, 1, 3, 5, 7,
    
    // Top face 
    0, 5, 1, 5, 0, 4,
    
    // Bottom face 
    6, 3, 7, 3, 6, 2
);

layout(location = 0) out vec2 texCoords;

layout(push_constant) uniform PushConstant {
    mat4 viewProj;
} pc;

void main() {
    int idx = cubeIndices[gl_VertexIndex];
    gl_Position = pc.viewProj * vec4(cubeVertices[idx], 1.0);
    texCoords = cubeTexCoords[idx];
}