#pragma once


namespace ad {
namespace gltfviewer {


inline const GLchar* gIblVertexShader = R"#(
    #version 400

    layout(location=0) in vec3 ve_position;
    layout(location=2) in vec3 ve_normal;

    uniform mat4 u_camera;
    uniform mat4 u_projection;

    out vec4 ex_color;
    out vec3 ex_position_local;
    out vec4 ex_position_view;
    out vec4 ex_normal_world;
    out vec4 ex_normal_view;

    void main(void)
    {
        mat4 model = mat4(1.0); // hardcoded atm

        ex_position_local = ve_position.xyz;

        ex_normal_world = model * vec4(ve_normal, 0.);

        mat4 modelViewTransform = u_camera * model;
        ex_position_view = modelViewTransform * vec4(ve_position, 1.);
        ex_normal_view = vec4(
            normalize(transpose(inverse(mat3(modelViewTransform))) * ve_normal),
            0.);

        ex_color = vec4(.5, .5, .5, 1.);

        gl_Position = u_projection * ex_position_view;
    }
)#";

inline const GLchar* gIblCubemapFragmentShader = R"#(
    #version 400

    in vec3 ex_position_local;
    in vec4 ex_color;

    out vec4 out_color;

    uniform samplerCube u_cubemap;

    void main(void)
    {
        out_color = texture(u_cubemap, ex_position_local);

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


inline const GLchar* gConvolutionFragmentShader = R"#(
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
            for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
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
in vec4 ex_normal_world;
in vec4 ex_normal_view;
in vec4 ex_color;

uniform vec4 u_baseColorFactor;
uniform float u_metallicFactor;
uniform float u_roughnessFactor;

uniform float u_ambientFactor;
uniform samplerCube u_irradianceMap;

uniform int u_debugOutput;

out vec4 out_color;

void main()
{
    vec3 n = normalize(vec3(ex_normal_view));
    vec3 v = normalize(-vec3(ex_position_view));

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
    vec3 lightColor = vec3(3.);
    vec3 l = normalize(vec3(0., 0., 1.));      // Directional light
    vec3 h = normalize(v + l);

    float NdotL = clampedDot(n, l);
    float VdotH = clampedDot(v, h);

    // TODO implement light attenuation
    float attenuation = 1.;
    vec3 intensity = lightColor * attenuation;

    vec3 F = fresnelSchlick(VdotH, F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    // No diffuse component for metals
    kD *= 1.0 - metallic; // Corresponding to: vec3 c_diff = mix(materialColor.rgb, vec3(0), metallic);


    float NDF = DistributionGGX(n, h, roughness);
    float G   = GeometrySmith(n, v, l, roughness);
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0)  + 0.0001;
    vec3 specular     = numerator / denominator;  

    vec3 diffuse = kD * materialColor.rgb / PI;

    L0 += (diffuse + specular) * intensity * NdotL; 

    //
    // Ambient IBL
    //
    float VdotN = clampedDot(v, n);
    {
        vec3 kS_Ibl = fresnelSchlick(VdotN, F0);
        vec3 kD_Ibl = 1.0 - kS_Ibl;
        kD_Ibl *= 1.0 - metallic;	  
        vec3 irradiance = texture(u_irradianceMap, ex_normal_world.xyz).rgb;
        vec3 diffuse      = irradiance * materialColor.rgb;
        vec3 ambient = kD_Ibl * diffuse * u_ambientFactor;
        
        L0 += ambient;
    }

    vec3 color = L0;

    // HDR tonemapping
    //color = color / (color + vec3(1.0));
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
    case 4:
        out_color = vec4(F, 1.0);
        break;
    case 5:
        out_color = vec4(n, 1.0);
        break;
    case 6:
        out_color = vec4(v, 1.0);
        break;
    case 7:
        out_color = vec4(h, 1.0);
        break;
    case 8:
        out_color = vec4(vec3(NdotL), 1.0);
        break;
    case 9:
        out_color = vec4(vec3(VdotH), 1.0);
        break;
    case 10:
        out_color = vec4(vec3(NDF), 1.0);
        break;
    case 11:
        out_color = vec4(vec3(G), 1.0);
        break;
    case 12:
        out_color = vec4(diffuse, 1.0);
        break;
    case 13:
        out_color = vec4(specular, 1.0);
        break;
    default:
        out_color = vec4(color, materialColor.a);
        break;
    }
} 

)#";


} // namespace gltfviewer
} // namespace ad
