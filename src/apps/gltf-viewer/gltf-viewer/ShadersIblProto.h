#pragma once


namespace ad {
namespace gltfviewer {



inline const GLchar* gQuadVertexShader = R"#(
    #version 400

    layout(location=0) in vec2 ve_position;
    layout(location=1) in vec2 ve_uv;

    out vec2 ex_uv;

    void main(void)
    {
        ex_uv = ve_uv;
        gl_Position = vec4(ve_position, 0., 1.);
    }
)#";


inline const GLchar* gTexture2DFragmentShader = R"#(
    #version 400

    in vec2 ex_uv;

    out vec4 out_color;

    uniform sampler2D u_texture;

    void main(void)
    {
        out_color = vec4(texture(u_texture, ex_uv).rgb, 1.);
    }
)#";

inline const GLchar* gIblVertexShader = R"#(
    #version 400

    layout(location=0) in vec3 ve_position;
    layout(location=1) in vec2 ve_uv;
    layout(location=2) in vec3 ve_normal;

    uniform mat4 u_camera;
    uniform mat4 u_projection;

    out vec4 ex_color;
    out vec2 ex_uv;
    out vec3 ex_position_local;
    out vec4 ex_position_world;
    out vec4 ex_position_view;
    out vec4 ex_normal_world;
    out vec4 ex_normal_view;

    void main(void)
    {
        ex_uv = ve_uv;

        mat4 model = mat4(1.0); // hardcoded atm

        ex_position_local = ve_position.xyz;

        ex_position_world = model * vec4(ex_position_local, 1.);
        ex_normal_world = model * vec4(ve_normal, 0.);

        mat4 modelViewTransform = u_camera * model;
        ex_position_view = modelViewTransform * vec4(ve_position, 1.);
        ex_normal_view = vec4(
            normalize(transpose(inverse(mat3(modelViewTransform))) * ve_normal),
            0.);

        //ex_color = vec4(.5, .5, .5, 1.);
        // Gold, see: https://learnopengl.com/PBR/Theory
        ex_color = vec4(1.0, 0.71, 0.29, 1.0);

        gl_Position = u_projection * ex_position_view;
    }
)#";

inline const GLchar* gIblCubemapFragmentShader = R"#(
    #version 400

    in vec3 ex_position_local;
    in vec4 ex_color;

    out vec4 out_color;

    uniform samplerCube u_cubemap;
    uniform int u_explicitLod;

    void main(void)
    {
        if(u_explicitLod >= 0)
        {
            out_color = textureLod(u_cubemap, ex_position_local, u_explicitLod);
        }
        else
        {
            out_color = texture(u_cubemap, ex_position_local);
        }


        // HDR tonemapping
        out_color.rgb = out_color.rgb / (out_color.rgb + vec3(1.0));
    }
)#";


inline const GLchar* gIblEquirectangularFragmentShader = R"#(
    #version 400

    in vec3 ex_position_local;
    in vec4 ex_color;

    out vec4 out_color;

    uniform sampler2D u_equirectangularMap;

    const vec2 invAtan = vec2(0.1591, 0.3183);
    vec2 sampleSphericalMap(vec3 v)
    {
        vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
        uv *= invAtan;
        uv += 0.5;
        return uv;
    }

    void main(void)
    {
        vec2 uv = sampleSphericalMap(normalize(ex_position_local));
        out_color = texture(u_equirectangularMap, uv);

        // HDR tonemapping
        out_color.rgb = out_color.rgb / (out_color.rgb + vec3(1.0));
    }
)#";


inline const GLchar* gIrradianceFragmentShader = R"#(
    #version 400

    const float PI = 3.14159265359;

    in vec3 ex_position_local;
    in vec4 ex_color;

    out vec4 out_color;

    uniform sampler2D u_equirectangularMap;

    const vec2 invAtan = vec2(0.1591, 0.3183);
    vec2 sampleSphericalMap(vec3 v)
    {
        vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
        uv *= invAtan;
        uv += 0.5;
        return uv;
    }

    void main(void)
    {
        vec3 irradiance = vec3(0.0);  

        vec3 normal = normalize(ex_position_local); // The normal on the equivalent sphere
        vec3 up    = vec3(0.0, 1.0, 0.0);
        vec3 right = normalize(cross(up, normal));
        up         = normalize(cross(normal, right));

        float sampleDelta = 0.005;
        float nrSamples = 0.0;
        for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
        {
            // It becomes much more flattering if we do not integrate the whole hemisphere
            //for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
            for(float theta = 0.0; theta < 0.3 * PI; theta += sampleDelta)
            {
                // spherical to cartesian (in tangent space)
                vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
                // tangent space to world
                vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

                // world to equirectangular
                vec2 uv = sampleSphericalMap(normalize(sampleVec));

                //irradiance += texture(u_equirectangularMap, uv).rgb * cos(theta) * sin(theta);
                //nrSamples ++; // Note: Should it be related to sin(theta)?

                // This is likely equivalent, up to a factor
                float surface = sin(theta) * sampleDelta * sampleDelta;
                irradiance += texture(u_equirectangularMap, uv).rgb * cos(theta) * surface;
                nrSamples += surface;
            }
        }
        irradiance = PI * irradiance * (1.0 / float(nrSamples));

        out_color = vec4(irradiance, 1.0);
    }
)#";


const std::string common = R"#(
    const float PI = 3.14159265359;

    float RadicalInverse_VdC(uint bits) 
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return float(bits) * 2.3283064365386963e-10; // / 0x100000000
    }

    vec2 Hammersley(uint i, uint N)
    {
        return vec2(float(i)/float(N), RadicalInverse_VdC(i));
    }  

    vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
    {
        float a = roughness*roughness;
        
        float phi = 2.0 * PI * Xi.x;
        float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
        float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
        
        // from spherical coordinates to cartesian coordinates
        vec3 H;
        H.x = cos(phi) * sinTheta;
        H.y = sin(phi) * sinTheta;
        H.z = cosTheta;
        
        // from tangent-space vector to world-space sample vector
        vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent   = normalize(cross(up, N));
        vec3 bitangent = cross(N, tangent);
        
        vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
        return normalize(sampleVec);
    }  

    const vec2 invAtan = vec2(0.1591, 0.3183);
    vec2 sampleSphericalMap(vec3 v)
    {
        vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
        uv *= invAtan;
        uv += 0.5;
        return uv;
    }
)#";


const std::string gPrefilterFragmentShader = R"#(
    #version 400

)#" + common + R"#(

    in vec3 ex_position_local;

    out vec4 out_color;

    uniform float u_roughness;
    uniform sampler2D u_equirectangularMap;


    const uint SAMPLE_COUNT = 1024u;

    void main(void)
    {
        vec3 N = normalize(ex_position_local);    
        // TODO: understand those assignments
        vec3 R = N;
        vec3 V = R;

        float totalWeight = 0.0;   
        vec3 prefilteredColor = vec3(0.0);     
        for(uint i = 0u; i < SAMPLE_COUNT; ++i)
        {
            vec2 Xi = Hammersley(i, SAMPLE_COUNT);
            vec3 H  = ImportanceSampleGGX(Xi, N, u_roughness);
            // TODO: understand why this is the light vector?
            vec3 L  = normalize(2.0 * dot(V, H) * H - V);

            float NdotL = max(dot(N, L), 0.0);
            if(NdotL > 0.0)
            {
                vec2 uv = sampleSphericalMap(L);
                prefilteredColor += texture(u_equirectangularMap, uv).rgb * NdotL;
                totalWeight      += NdotL;
            }
        }
        prefilteredColor = prefilteredColor / totalWeight;

        out_color = vec4(prefilteredColor, 1.0);
    }
)#";


const std::string gBrdfLutFragmentShader = R"#(
    #version 400

)#" + common + R"#(

    in vec2 ex_uv;

    out vec4 out_color;

       
    float GeometrySchlickGGX(float NdotV, float roughness)
    {
        float a = roughness;
        float k = (a * a) / 2.0;

        float nom   = NdotV;
        float denom = NdotV * (1.0 - k) + k;

        return nom / denom;
    }

    float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
    {
        float NdotV = max(dot(N, V), 0.0);
        float NdotL = max(dot(N, L), 0.0);
        float ggx2 = GeometrySchlickGGX(NdotV, roughness);
        float ggx1 = GeometrySchlickGGX(NdotL, roughness);

        return ggx1 * ggx2;
    }  


    const uint SAMPLE_COUNT = 1024u;

    vec2 IntegrateBRDF(float NdotV, float roughness)
    {
        vec3 V;
        V.x = sqrt(1.0 - NdotV*NdotV);
        V.y = 0.0;
        V.z = NdotV;

        float A = 0.0;
        float B = 0.0;

        vec3 N = vec3(0.0, 0.0, 1.0);

        const uint SAMPLE_COUNT = 1024u;
        for(uint i = 0u; i < SAMPLE_COUNT; ++i)
        {
            vec2 Xi = Hammersley(i, SAMPLE_COUNT);
            vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
            vec3 L  = normalize(2.0 * dot(V, H) * H - V);

            float NdotL = max(L.z, 0.0);
            float NdotH = max(H.z, 0.0);
            float VdotH = max(dot(V, H), 0.0);

            if(NdotL > 0.0)
            {
                float G = GeometrySmith(N, V, L, roughness);
                float G_Vis = (G * VdotH) / (NdotH * NdotV);
                float Fc = pow(1.0 - VdotH, 5.0);

                A += (1.0 - Fc) * G_Vis;
                B += Fc * G_Vis;
            }
        }
        A /= float(SAMPLE_COUNT);
        B /= float(SAMPLE_COUNT);
        return vec2(A, B);
    }


    void main(void)
    {
        out_color = vec4(IntegrateBRDF(ex_uv.x, ex_uv.y), 0., 1.);
    }
)#";


inline const std::string gIblPbrFragmentShader = R"#(
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
in vec4 ex_position_view;
in vec4 ex_normal_world;
in vec4 ex_normal_view;
in vec4 ex_color;

uniform vec3 u_lightColor;
uniform vec4 u_baseColorFactor;
uniform float u_metallicFactor;
uniform float u_roughnessFactor;
uniform float u_maxReflectionLod;

uniform float       u_ambientFactor;
uniform samplerCube u_irradianceMap;
uniform samplerCube u_prefilterMap;
uniform sampler2D   u_brdfLut;

uniform vec3 u_cameraPosition_world;

uniform bool u_hdrTonemapping;
uniform bool u_gammaCorrect;
uniform int u_debugOutput;

out vec4 out_color;

void main()
{
    // The reflected vector must be in world space in order to index the prefiltered cubemap
    //vec3 n = normalize(vec3(ex_normal_view));
    //vec3 v = normalize(-vec3(ex_position_view));
    vec3 n = normalize(vec3(ex_normal_world));
    vec3 v = normalize(u_cameraPosition_world - ex_position_world.xyz);
    vec3 r = reflect(-v, n);

    vec4 materialColor = u_baseColorFactor * ex_color;

    float metallic = clamp(u_metallicFactor, 0.0, 1.0);
    float roughness = clamp(u_roughnessFactor, 0.0, 1.0);
    
    // Implements a tinted reflection for metals, and a hardcoded "monochrome" reflection for dielectrics
    vec3 F0 = mix(vec3(0.04), materialColor.rgb, metallic);
    // TODO should it be squared even more?
    //float alphaRoughness = roughness * roughness;

    vec3 L0 = vec3(0.0);

    //
    // Lights
    //
    //vec3 l = normalize(vec3(0., 0., 1.));      // Directional light, in view space
    vec3 l = normalize(u_cameraPosition_world);  // Directional light, in world space
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
