#version 450
//keep in sync with host / other shaders:
struct UserParameters 
{
    uint maxIndexDataSet;
    uint resX;
    uint resY;
    uint runCompute;
    int kernelSize;
    int yearMin;
    int yearMax;
    int monthMin;
    int monthMax;
    int dayMin;
    int dayMax;
    int hourMin;
    int hourMax;
    float sigma;
    int kernelChanged;
    int labelNA ;
	int labelWALK;
	int labelBIKE;
	int labelBUS;
	int labelCAR;
	int labelSUBWAY;
	int labelTRAIN;
	int labelAIRPLANE;
	int labelBOAT;
	int labelMOTORCYCLE;
    int labelMonday;
    int labelTuesday;
    int labelWednesday;
    int labelThursday;
    int labelFriday;
    int labelSaturday;
    int labelSunday;
    int numberOfWorkGroups;
    float zoom; // A value between 0 and 1. The whole map has sidelength 1. No zoom means m_zoom = 1. Maximum Zoom means m_zoom = 0.1
    float originX;
	float originY;
};
const uint gaussDensity = 4; // gaus filter must be divided by this to obtain correct number of cars at current point
layout(location = 0) in vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform sampler2D cityMapSampler;
layout(set = 0, binding = 1) uniform sampler2D colorMapSampler;
layout(std140, set = 1, binding = 0) uniform uboUserParameters 
{
    UserParameters parameters;
} userParameters;
layout(set = 1, binding = 2, r32i ) uniform coherent iimage2D imageKDE;
layout(set = 1, binding = 3) uniform isampler2D kdeSampler;

layout(location = 0) out vec4 outColor;


void main() 
{
    //calculate zoomed coordinates
    // origin are the normalized coordinates of the current window origin wrt the complete map texture
    float zoom = userParameters.parameters.zoom;
    vec2 origin = vec2( userParameters.parameters.originX, userParameters.parameters.originY);
    vec2 cityCoords =  origin + fragTexCoord * zoom;
    
    //cityCoords = vec2(cityCoords.x,cityCoords.y);
    // Get color from textures
    outColor = vec4(0,0,0,0);
    vec2 mirroredFragTexCoord =vec2(fragTexCoord.x,-fragTexCoord.y);
    float kdeValue = float(texture(kdeSampler,fragTexCoord).r);
    vec4 cityMapColor = texture(cityMapSampler, cityCoords);
    float maximumKdeValue = float(imageLoad(imageKDE,ivec2(0,0)).r);

    if(maximumKdeValue==1)  maximumKdeValue = 2.8; // avoid division by zero for log values
    float normalizedDensity = log(kdeValue)/log(maximumKdeValue);
    normalizedDensity = max(0.01,min(0.99,1-normalizedDensity));
    vec4 heatMapColor = texture(colorMapSampler,vec2(0.5,normalizedDensity));
    if(kdeValue > 0) outColor = vec4(mix(heatMapColor.rgb,cityMapColor.rgb,0.2),1);
    else outColor = cityMapColor;
}
