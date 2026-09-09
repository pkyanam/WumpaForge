#ifndef WRATH_PRESENTATION_FILTER_H
#define WRATH_PRESENTATION_FILTER_H
/* Original dependency-free five-tap spatial presentation filter. No temporal
 * state or additional pass. Set LINEAR + CLAMP_TO_EDGE on u_source, draw only
 * the aspect-correct viewport, disable blend/depth and preserve alpha.
 * u_sharpness=0 is exactly bilinear; menu strengths are0.25/0.5/0.75.
 * RGB filtering is in texture code-value space; no implicit gamma conversion.
 * This is not AMD CAS/FSR and contains no third-party implementation. */
static const char wrath_presentation_filter_glsl[] =
"#version 330 core\n"
"in vec2 v_uv;\n"
"uniform sampler2D u_source;\n"
"uniform vec2 u_source_size;\n"
"uniform float u_sharpness;\n"
"out vec4 color;\n"
"void main(){\n"
" vec4 c=texture(u_source,v_uv);\n"
" float s=clamp(u_sharpness,0.0,1.0);\n"
" if(s==0.0){color=c;return;}\n"
" vec2 d=1.0/max(u_source_size,vec2(1.0));\n"
" vec3 n=texture(u_source,v_uv+vec2(0,d.y)).rgb;\n"
" vec3 e=texture(u_source,v_uv+vec2(d.x,0)).rgb;\n"
" vec3 w=texture(u_source,v_uv-vec2(d.x,0)).rgb;\n"
" vec3 b=texture(u_source,v_uv-vec2(0,d.y)).rgb;\n"
" vec3 lo=min(c.rgb,min(min(n,e),min(w,b)));\n"
" vec3 hi=max(c.rgb,max(max(n,e),max(w,b)));\n"
" vec3 detail=c.rgb-0.25*(n+e+w+b);\n"
" color=vec4(clamp(c.rgb+s*detail,lo,hi),c.a);\n"
"}\n";
#endif
