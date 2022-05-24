#pragma once


namespace ad {
namespace gltfviewer {


inline const GLchar* gIblVertexShader = R"#(
    #version 400

    layout(location=0) in vec3 ve_position;

    uniform mat4 u_camera;
    uniform mat4 u_projection;

    out vec4 ex_color;
    out vec4 ex_position_view;
    out vec3 ex_position_local;

    void main(void)
    {
        ex_position_local = ve_position.xyz;

        mat4 modelViewTransform = u_camera;
        ex_position_view = modelViewTransform * vec4(ve_position, 1.);
        ex_color = vec4(1., 0., 0., 1.);

        gl_Position = u_projection * ex_position_view;
    }
)#";

inline const GLchar* gIblFragmentShader = R"#(
    #version 400

    in vec3 ex_position_local;
    in vec4 ex_color;

    out vec4 out_color;

    uniform samplerCube u_cubemap;

    void main(void)
    {
        out_color = texture(u_cubemap, ex_position_local);
    }
)#";


inline const GLchar* gEquirectangularFragmentShader = R"#(
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

        float sampleDelta = 0.01;
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
                irradiance += texture(u_equirectangularMap, uv).rgb * cos(theta) * sin(theta);
                nrSamples ++; // Note: Should it be related to sin(theta)?
                //nrSamples += sin(theta);
            }
        }
        irradiance = PI * irradiance * (1.0 / float(nrSamples));
        out_color = vec4(irradiance, 1.0);
    }
)#";


} // namespace gltfviewer
} // namespace ad
