
static const char* source_vsh_Font = 
"#version 150\n"
"in mediump vec2 position;\n"
"out lowp vec4 clr;\n"
"uniform mediump vec2 rotx;\n"
"uniform mediump vec2 roty;\n"
"uniform mediump vec2 translation;\n"
"uniform lowp vec4 colour;\n"
"uniform mediump float fadedist;\n"
"void main()\n"
"{\n"
"    gl_Position.x = dot( position, rotx ) + translation.x;\n"
"    gl_Position.y = dot( position, roty ) + translation.y;\n"
"    gl_Position.z = 1.0;\n"
"    gl_Position.w = 1.0;\n"
"    clr = colour;\n"
"    if ( abs( gl_Position.x ) > fadedist || abs( gl_Position.y ) > fadedist )\n"
"        clr = colour;\n"
"}\n";


static const char* source_fsh_Font =
"#version 150\n"
"in  lowp vec4 clr;\n"
"out lowp vec4 fragColor;\n"
"void main()\n"
"{\n"
"    fragColor = clr;\n"
"}\n";


static const char* source_vsh_Main =
"#version 150\n"
"in mediump vec2 position;\n"
"out lowp vec4 clr;\n"
"uniform lowp vec4 colour;\n"
"uniform mediump float invaspect;\n"
"uniform mediump vec2 rotx;\n"
"uniform mediump vec2 roty;\n"
"uniform mediump vec2 translation;\n"
"void main()\n"
"{\n"
"    gl_Position.x = rotx.x * position.x + roty.x * position.y + translation.x;\n"
"    gl_Position.y = rotx.y * position.x + roty.y * position.y + translation.y;\n"
"    gl_Position.x *= invaspect;\n"
"    gl_Position.z = 1.0;\n"
"    gl_Position.w = 1.0;\n"
"    clr = colour;\n"
"}\n";


static const char* source_fsh_Main =
"#version 150\n"
"in  lowp vec4 clr;\n"
"out lowp vec4 fragColor;\n"
"void main()\n"
"{\n"
"    fragColor = clr;\n"
"}\n";


static const char* source_vsh_Star =
"#version 150\n"
"in mediump vec2 position;\n"
"in mediump vec2 displacement;\n"
"out lowp vec4 clr;\n"
"uniform lowp vec4 colour;\n"
"uniform mediump float invaspect;\n"
"uniform mediump vec2 rotx;\n"
"uniform mediump vec2 roty;\n"
"uniform mediump vec2 translation;\n"
"uniform mediump float splatscale;\n"
"void main()\n"
"{\n"
"    mediump vec2 p = displacement + position * splatscale;\n"
"    gl_Position.x = rotx.x * p.x + roty.x * p.y + translation.x;\n"
"    gl_Position.y = rotx.y * p.x + roty.y * p.y + translation.y;\n"
"    gl_Position.x *= invaspect;\n"
"    gl_Position.z = 1.0;\n"
"    gl_Position.w = 1.0;\n"
"    clr = colour;\n"
"}\n";


static const char* source_fsh_Star =
"#version 150\n"
"in  lowp vec4 clr;\n"
"out lowp vec4 fragColor;\n"
"void main()\n"
"{\n"
"    fragColor = clr;\n"
"}\n";

