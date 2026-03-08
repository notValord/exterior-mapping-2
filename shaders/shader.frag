#version 450

layout(location = 0) in vec4 colorGrad;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 fragPosition;
layout(location = 3) in vec2 fragTexCoords;
layout(location = 4) in float depth;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform RenderFragmentObject {
    vec3 lightPos;
    vec3 camPos;
    uint depth;
} rfo;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

struct RenderMaterialObject{
    vec3 ambient;
    float _padd;
    vec3 specular;
    float shininess;
};

layout(std430, set = 0, binding = 2) readonly buffer MaterialSSBOIn {
   RenderMaterialObject materialIn[];
};

layout(push_constant) uniform PushConstant {
    mat4 MVP;
    uint hasTexture;
    uint materialID;
} pc;

const vec3 lightColor = vec3(1.0f); 

float LinearizeDepth()      // bounded to 0-1
{
    float zNear = 0.1;          // TODO: Replace by the zNear of your perspective projection
    float zFar  = 100.0;        // TODO: Replace by the zFar  of your perspective projection
    float d = (zNear * zFar) / (zFar - depth * (zFar - zNear));
    return (d - zNear) / (zFar - zNear);
}

float differentDepth() {
    float zNear = 0.1;
    float zFar  = 100.0;
    float z = gl_FragCoord.z;
    return (2.0 * zNear) / (zFar + zNear - z * (zFar - zNear));
}

vec3 ambientLight(in vec3 diffuseColor) {
    float ambientStrength = 0.02;

    return diffuseColor * ambientStrength * lightColor;        // WRONG
}

vec3 diffuseLight(in vec3 diffuseColor, in vec3 norm, in vec3 lightDir) {
    float diff = max(dot(norm, lightDir), 0.0);
    return diff * diffuseColor * lightColor;
}

vec3 specularLight(in vec3 norm, in vec3 viewDir, in vec3 lightDir) {
    // Only calculate specular if surface is facing the light
    float diffuseFactor = dot(norm, lightDir);
    if (diffuseFactor <= 0.0) {
        return vec3(0.0);
    }

    vec3 reflectDir = reflect(-lightDir, norm);

    float shininess = clamp(materialIn[pc.materialID].shininess / 10.0, 1.0, 128.0);      // convert the value form blender

    float spec = pow(max(dot(reflectDir, viewDir), 0.0), shininess);

    return materialIn[pc.materialID].specular * spec * lightColor;
}

vec4 basicLighting(vec4 objectColor) {
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(rfo.lightPos - fragPosition);
    vec3 viewDir = normalize(rfo.camPos - fragPosition);

    return vec4(ambientLight(objectColor.xyz) + diffuseLight(objectColor.xyz, norm, lightDir) + specularLight(norm, viewDir, lightDir), objectColor.w);
}

void main() {
    if (rfo.depth == 1) {
        float n = differentDepth();
        outColor = vec4(n, n, n, 1.0);
    }
    else {
        // outColor = vec4(normal, 1.0f);
        if (pc.hasTexture == 1) {
            outColor = texture(texSampler, fragTexCoords);
            outColor = basicLighting(outColor);
        }
        else {
            outColor = colorGrad;
            outColor = basicLighting(outColor);
        }
    }
}