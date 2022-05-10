#pragma once


namespace ad {
namespace gltfviewer {


// See: https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/source/Renderer/shaders/brdf.glsl
inline const std::string gPbrFunctions = R"#(
const float M_PI = 3.141592653589793;

float clampedDot(vec3 x, vec3 y)
{
    return clamp(dot(x, y), 0.0, 1.0);
}


// F90 is taken to be 1 in the model
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}


// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}


// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}


vec3 BRDF_specularGGX(vec3 f0, vec3 f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    vec3 F = F_Schlick(f0, f90, VdotH);
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);

    return specularWeight * F * Vis * D;
}


//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_lambertian(vec3 f0, vec3 f90, vec3 diffuseColor, float specularWeight, float VdotH)
{
    // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}
)#";


inline const std::string gPbrFragmentShader = R"#(
#version 400

in vec4 ex_position_view;
in vec4 ex_normal_view;
in vec2 ex_baseColorUv;
in vec4 ex_color;

uniform vec4 u_baseColorFactor;
uniform sampler2D u_baseColorTex;
uniform float u_metallicFactor;
uniform float u_roughnessFactor;
uniform sampler2D u_metallicRoughnessTex;

out vec4 out_color;

)#" + gPbrFunctions + R"#(

void main()
{
    vec4 materialColor = 
        u_baseColorFactor * texture(u_baseColorTex, ex_baseColorUv)
        * ex_color
        ;

    float specularWeight = 1.0;
    vec3 f90 = vec3(1.0); // see: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#metals
    float metallic = clamp(u_metallicFactor, 0.0, 1.0);
    float roughness = clamp(u_roughnessFactor, 0.0, 1.0);
    metallic = metallic * texture(u_metallicRoughnessTex, ex_baseColorUv).b;
    roughness = roughness * texture(u_metallicRoughnessTex, ex_baseColorUv).g;
    
    vec3 c_diff = mix(materialColor.rgb, vec3(0), metallic);
    vec3 f0 = mix(vec3(0.04), materialColor.rgb, metallic);
    float alphaRoughness = roughness * roughness;

    vec3 v = normalize(-vec3(ex_position_view));
    vec3 n = normalize(vec3(ex_normal_view)); // linear interpolation might shorten the normal
    // Hardcode directional light from camera atm
    vec3 l = vec3(0., 0., 1.);      // Direction from surface point to light
    vec3 h = normalize(l + v);      // Direction of the vector between l and v, called halfway vector
    float NdotL = clampedDot(n, l);
    float NdotV = clampedDot(n, v);
    float NdotH = clampedDot(n, h);
    float LdotH = clampedDot(l, h);
    float VdotH = clampedDot(v, h);
    // Hardcode light intensity
    float intensity = 3.0;


    // TODO Why the condition in reference glTF viewer?
    //if (NdotL > 0.0 || NdotV > 0.0)
    //{
        vec3 f_diffuse = intensity * NdotL *  BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
        vec3 f_specular = intensity * NdotL * BRDF_specularGGX(f0, f90, alphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);

        out_color = vec4(f_diffuse + f_specular, materialColor.a);
    //}
    //vec3 lamb = BRDF_lambertian(f0, f90, c_diff, specularWeight, VdotH);
    //vec3 spec = BRDF_specularGGX(f0, f90, alphaRoughness, specularWeight, VdotH, NdotL, NdotV, NdotH);
    //out_color = vec4(spec, materialColor.a);
    //out_color = vec4(f_specular, materialColor.a);
    //out_color = vec4(f_diffuse, materialColor.a);
    //out_color = vec4(n, materialColor.a);
    //out_color = vec4(vec3(NdotL), materialColor.a);
    //out_color = vec4(vec3(NdotV), materialColor.a);
    //out_color = vec4(vec3(NdotH), materialColor.a);
    //out_color = vec4(vec3(texture(u_metallicRoughnessTex, ex_baseColorUv)), materialColor.a);
    //out_color = vec4(vec3(roughness), materialColor.a);
    //out_color = vec4(vec3(metallic), materialColor.a);
}

)#";



} // namespace gltfviewer
} // namespace ad
