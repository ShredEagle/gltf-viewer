#pragma once


namespace ad {
namespace gltfviewer {


inline const std::string gPbrLearnFragmentShader = R"#(
#version 400

const float PI = 3.141592653589793;

float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    // TODO change k for IBL
    // So roughness is not squared here?
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    // Obstruction (view)
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    // Shadowing (light)
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

in vec4 ex_position_view;
in vec4 ex_normal_view;
in vec2 ex_baseColorUv;
in vec4 ex_color;

uniform vec3 u_lightColor;

uniform vec4 u_baseColorFactor;
uniform sampler2D u_baseColorTex;
uniform float u_metallicFactor;
uniform float u_roughnessFactor;
uniform sampler2D u_metallicRoughnessTex;

uniform int u_debugOutput;

// Cannot use as switch case labels apparently
#define METALLIC 1;

out vec4 out_color;

void main()
{
    vec3 n = normalize(vec3(ex_normal_view));
    vec3 v = normalize(-vec3(ex_position_view));

    vec4 materialColor = 
        u_baseColorFactor * texture(u_baseColorTex, ex_baseColorUv)
        * ex_color
        ;

    float metallic = clamp(u_metallicFactor, 0.0, 1.0);
    float roughness = clamp(u_roughnessFactor, 0.0, 1.0);
    metallic = metallic * texture(u_metallicRoughnessTex, ex_baseColorUv).b;
    roughness = roughness * texture(u_metallicRoughnessTex, ex_baseColorUv).g;
    
    // Implements a tinted reflection for metals, and a hardcoded "monochrome" reflection for dielectrics
    vec3 F0 = mix(vec3(0.04), materialColor.rgb, metallic);
    // TODO should it be squared even more?
    //float alphaRoughness = roughness * roughness;

    vec3 lightColor = vec3(3.);
    vec3 l = normalize(vec3(0., 0., 1.));      // Direction from surface point to light, in camera space

    vec3 h = normalize(v + l);

    float NdotV = clampedDot(n, v);
    float NdotL = clampedDot(n, l);
    float VdotH = clampedDot(v, h);

    // TODO implement light attenuation
    float attenuation = 1.;
    vec3 intensity = u_lightColor * 3 * attenuation;

    vec3 F = fresnelSchlick(VdotH, F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    // No diffuse component for metals
    kD *= 1.0 - metallic; // Corresponding to: vec3 c_diff = mix(materialColor.rgb, vec3(0), metallic);


    float NDF = DistributionGGX(n, h, roughness);
    float G   = GeometrySmith(n, v, l, roughness);
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL  + 0.0001;
    vec3 specular     = (numerator / denominator) * intensity * NdotL;  

    vec3 diffuse = (kD * materialColor.rgb / PI) * intensity * NdotL;

    // Note: Initially, we multiplied by intensity * NdotL here,
    // Yet multiplying early allows better debug color outputs.
    vec3 L0 = diffuse + specular;

    vec3 color = L0;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));

    switch(u_debugOutput)
    {
    //case METALLIC:
    case 1:
        out_color = vec4(vec3(metallic), 1.0);
        break;
    case 2:
        out_color = vec4(vec3(roughness), 1.0);
        break;
    case 3:
        out_color = materialColor;
        break;
    case 5:
        out_color = vec4(F, 1.0);
        break;
    case 7:
        out_color = vec4(n, 1.0);
        break;
    case 8:
        out_color = vec4(v, 1.0);
        break;
    case 9:
        out_color = vec4(h, 1.0);
        break;
    case 10:
        out_color = vec4(vec3(NdotL), 1.0);
        break;
    case 11:
        out_color = vec4(vec3(VdotH), 1.0);
        break;
    case 12:
        out_color = vec4(vec3(NDF), 1.0);
        break;
    case 13:
        out_color = vec4(vec3(G), 1.0);
        break;
    case 14:
        out_color = vec4(diffuse, 1.0);
        break;
    case 15:
        out_color = vec4(specular, 1.0);
        break;
    case 18:
        out_color = vec4(L0, 1.0);
        break;
    case 4:
    case 6:
    case 16:
    case 17:
    case 19:
        out_color = vec4(1.0, 0., 1.0, 1.0);
        break;
    default:
        out_color = vec4(color, materialColor.a);
        break;
    }
} 

)#";


} // namespace gltfviewer
} // namespace ad
