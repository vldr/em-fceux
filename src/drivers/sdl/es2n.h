#ifndef _ES2N_H_
#define _ES2N_H_

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>

typedef struct t_es2n_controls
{
    // Control values should be in range [-1,1], default is 0.
    GLfloat brightness; // Brightness control.
    GLfloat contrast; // Contrast control.
    GLfloat color; // Color control.
    GLfloat gamma; // Gamma control.
    GLfloat rgbppu; // RGB PPU control.

    // Uniform locations.
    GLint _brightness_loc;
    GLint _contrast_loc;
    GLint _color_loc;
    GLint _gamma_loc;
    GLint _rgbppu_loc;

} es2n_controls;

typedef struct t_es2n
{
    GLuint quad_buf;    // Fullscreen quad vertex buffer.

    GLuint idx_tex;     // Input indexed color texture (NES palette, 256x240).
    GLuint deemp_tex;   // Input de-emphasis bits per row (240x1).
    GLuint lookup_tex;  // Palette to voltage levels lookup texture.

    GLuint rgb_fb;      // Framebuffer for output RGB texture generation.
    GLuint rgb_tex;     // Output RGB texture (1024x256x3).
    GLuint rgb_prog;    // Shader for RGB.

    GLuint disp_prog;   // Shader for final display.

    GLint viewport[4];  // Original viewport.

    GLuint crt_verts_buf;   // Vertex buffer for CRT.
    GLuint crt_elems_buf;   // Element buffer for CRT.
    int crt_enabled;        // Zero to disable CRT display, otherwise enabled.

    GLubyte overscan_color;   // Current overscan color (background/zero color).
    GLubyte *overscan_pixels; // Temporary overscan pixels (1x240).

    GLfloat yiq_mins[3];
    GLfloat yiq_maxs[3];

    es2n_controls controls;
} es2n;

void es2nInit(es2n *p, int left, int right, int top, int bottom);
void es2nUpdateControls(es2n *p);
void es2nDeinit(es2n *p);
void es2nRender(es2n *p, GLubyte *pixels, GLubyte *row_deemp, GLubyte overscan_color);

#endif