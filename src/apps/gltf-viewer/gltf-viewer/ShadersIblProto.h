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


} // namespace gltfviewer
} // namespace ad
