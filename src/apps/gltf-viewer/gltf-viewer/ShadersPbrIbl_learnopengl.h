#pragma once


namespace ad {
namespace gltfviewer {


inline const std::string gIblPbrLearnFragmentShader = R"#(
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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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

in vec4 ex_position_world;
in vec4 ex_normal_world;
in vec2 ex_baseColorUv;
in vec4 ex_color;

uniform vec3 u_cameraPosition_world;

uniform vec3 u_lightDirection_world;
uniform vec3 u_lightColor;

uniform vec4 u_baseColorFactor;
uniform sampler2D u_baseColorTex;
uniform float u_metallicFactor;
uniform float u_roughnessFactor;
uniform sampler2D u_metallicRoughnessTex;

// IBL parameters
uniform float       u_ambientFactor;
uniform int         u_maxReflectionLod;
uniform samplerCube u_irradianceMap;
uniform samplerCube u_prefilterMap;
uniform sampler2D   u_brdfLut;

uniform bool u_hdrTonemapping;
uniform bool u_gammaCorrect;
uniform int u_debugOutput;

out vec4 out_color;

void main()
{
    // The reflected vector must be in world space in order to index the prefiltered cubemap
    vec3 n = normalize(vec3(ex_normal_world));
    vec3 v = normalize(u_cameraPosition_world - ex_position_world.xyz);
    vec3 r = reflect(-v, n);

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

    vec3 L0 = vec3(0.0);

    //
    // Lights
    //
    vec3 l = normalize(-u_lightDirection_world);  // Directional light
    vec3 h = normalize(v + l);

    float NdotV = clampedDot(n, v);
    float NdotL = clampedDot(n, l);
    float VdotH = clampedDot(v, h);

    // TODO implement light attenuation
    float attenuation = 1.;
    vec3 intensity = u_lightColor * attenuation;

    vec3 F_Light = fresnelSchlick(VdotH, F0);

    vec3 kS = F_Light;
    vec3 kD = vec3(1.0) - kS;
    // No diffuse component for metals
    kD *= 1.0 - metallic; // Corresponding to: vec3 c_diff = mix(materialColor.rgb, vec3(0), metallic);


    float NDF = DistributionGGX(n, h, roughness);
    float G   = GeometrySmith(n, v, l, roughness);
    vec3 numerator    = NDF * G * F_Light;
    float denominator = 4.0 * NdotV * NdotL  + 0.0001;
    vec3 specular     = (numerator / denominator) * intensity * NdotL;  

    vec3 diffuse = (kD * materialColor.rgb / PI) * intensity * NdotL;

    // Note: Initially, we multiplied by intensity * NdotL here,
    // Yet multiplying early allows better debug color outputs.
    L0 += diffuse + specular;


    vec3 diffuse_Ibl  = vec3(0.);
    vec3 specular_Ibl = vec3(0.);
    float VdotN = clampedDot(v, n);
    vec3 F_Ibl = fresnelSchlickRoughness(VdotN, F0, roughness);
    {
        //
        // Diffuse IBL
        //
        vec3 kS_Ibl = F_Ibl;
        vec3 kD_Ibl = 1.0 - kS_Ibl;
        kD_Ibl *= 1.0 - metallic;	  

        vec3 irradiance = texture(u_irradianceMap, ex_normal_world.xyz).rgb;
        vec3 diffuse    = irradiance * materialColor.rgb;
        diffuse_Ibl = kD_Ibl * diffuse;

        //
        // Specular IBL
        //
        vec3 prefilteredColor = textureLod(u_prefilterMap, r, roughness * u_maxReflectionLod).rgb;

        vec2 brdf = texture(u_brdfLut, vec2(VdotN, roughness)).rg;
        // Not multiplied by kS, because there is already F (==kS) in computing specular.
        specular_Ibl = prefilteredColor * (F_Ibl * brdf.r + brdf.g);
    }

    vec3 ambient = diffuse_Ibl + specular_Ibl;
    vec3 color = L0 + ambient * u_ambientFactor;

    // HDR tonemapping
    if(u_hdrTonemapping)
    {
        color = color / (color + vec3(1.0));
    }
    // gamma correct
    if(u_gammaCorrect)
    {
        color = pow(color, vec3(1.0/2.2));
    }

    switch(u_debugOutput)
    {
    case 1:
        out_color = vec4(vec3(metallic), 1.0);
        break;
    case 2:
        out_color = vec4(vec3(roughness), 1.0);
        break;
    case 3:
        out_color = materialColor;
        break;
    case 4:
        out_color = vec4(F_Light, 1.0);
        break;
    case 5:
        out_color = vec4(F_Ibl, 1.0);
        break;
    case 6:
        out_color = vec4(n, 1.0);
        break;
    case 7:
        out_color = vec4(v, 1.0);
        break;
    case 8:
        out_color = vec4(h, 1.0);
        break;
    case 9:
        out_color = vec4(vec3(NdotL), 1.0);
        break;
    case 10:
        out_color = vec4(vec3(VdotH), 1.0);
        break;
    case 11:
        out_color = vec4(vec3(NDF), 1.0);
        break;
    case 12:
        out_color = vec4(vec3(G), 1.0);
        break;
    case 13:
        out_color = vec4(diffuse, 1.0);
        break;
    case 14:
        out_color = vec4(specular, 1.0);
        break;
    case 15:
        out_color = vec4(diffuse_Ibl, 1.0);
        break;
    case 16:
        out_color = vec4(specular_Ibl, 1.0);
        break;
    case 17:
        out_color = vec4(L0, 1.0);
        break;
    case 18:
        out_color = vec4(ambient, 1.0);
        break;
    default:
        out_color = vec4(color, materialColor.a);
        break;
    }
} 

)#";

} // namespace gltfviewer
} // namespace ad
