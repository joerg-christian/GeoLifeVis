#version 450

// input/output
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 0) out vec2 fragTexCoord;

// Uniform Buffers
layout(binding = 0) uniform UniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() 
{
    //gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    gl_Position = vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}

/*
// This is the old shader without uniform buffers:
void main() 
{
    gl_Position = vec4(inPosition, 0.0f, 1.0f);
    fragColor = inColor;
}
*/