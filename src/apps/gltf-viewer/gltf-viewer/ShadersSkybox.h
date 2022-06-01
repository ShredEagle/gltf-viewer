#pragma once


namespace ad {
namespace gltfviewer {


inline const GLchar* gSkyboxVertexShader = R"#(
    #version 400

    layout(location=0) in vec4 ve_position;

    uniform mat4 u_cameraOrientation;
    uniform mat4 u_projection;

    out vec3 ex_position_local;

    void main(void)
    {
        ex_position_local = ve_position.xyz;
        gl_Position = u_projection * u_cameraOrientation * ve_position;
    }
)#";


inline const GLchar* gEquirectangularSkyboxFragmentShader = R"#(
    #version 400

    in vec3 ex_position_local;

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
        // Gamma correct
        out_color = vec4(pow(out_color.rgb, vec3(1.0/2.2)), out_color.a);
    }
)#";


} // namespace gltfviewer
} // namespace ad
