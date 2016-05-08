#ifndef _ES2_H_
#define _ES2_H_
#include "es2util.h"

// Set overscan on left and right sides as 12px (total 24px).
#define OVERSCAN_W  12
#define IDX_W       (INPUT_W + 2*OVERSCAN_W)
#define IDX_H       INPUT_H

typedef enum t_ES2Unif {
U_NONE = -1,
#define U(prog_, id_, name_) U_ ## id_,
#include "es2unif.inc.hpp"
#undef U
U_COUNT
} ES2Unif;

// Uniform locations.
typedef struct t_ES2Uniforms
{
	GLint u[2 * U_COUNT];

	GLint _downsample_offsets_loc;
	GLint _downsample_downsampleTex_loc;

	GLint _sharpen_kernel_loc;
	GLint _sharpen_convergence_loc;

	GLint _stretch_scanlines_loc;
	GLint _stretch_smoothenOffs_loc;

	GLint _screen_uv_scale_loc;
	GLint _screen_border_uv_offs_loc;
	GLint _screen_mvp_loc;

	GLint _tv_mvp_loc;

	GLint _combine_glow_loc;

	GLint _direct_v_scale_loc;

} ES2Uniforms;

typedef struct t_ES2
{
	GLuint idx_tex;     // Input indexed color texture (NES palette, 256x240).
	GLuint deemp_tex;   // Input de-emphasis bits per row (240x1).
	GLuint lookup_tex;  // Palette to voltage levels lookup texture.

	GLuint noise_tex;   // Noise texture.

	GLuint rgb_fb;      // Framebuffer for output RGB texture generation.
	GLuint rgb_tex;     // Output RGB texture.
	GLuint rgb_prog;    // Shader for RGB generation.

	GLuint ntsc_prog;    // Shader for NTSC emulation.

	GLuint sharpen_fb;   // Framebuffer for sharpened RGB texture.
	GLuint sharpen_tex;  // Sharpened RGB texture.
	GLuint sharpen_prog; // Shader for sharpening.

	GLuint stretch_fb;   // Framebuffer for stretched RGB texture.
	GLuint stretch_tex;  // Output stretched RGB texture.
	GLuint stretch_prog; // Shader for stretched RGB.

	GLuint screen_prog;  // Shader for screen.

	GLuint tv_fb;        // Framebuffer to render screen and TV.
	GLuint tv_tex;       // Texture for screen/TV framebuffer.
	GLuint tv_prog;      // Shader for tv.

	GLuint combine_prog; // Shader for combine.

	GLuint direct_prog;  // Shader for rendering texture directly to the screen.

	GLint viewport[4];  // Screen viewport.

	GLfloat mvp_mat[4*4]; // Perspective MVP matrix for the meshes.

	GLuint downsample_fb[6];  // Framebuffers for downscaling.
	GLuint downsample_tex[6]; // Downsample textures.
	GLuint downsample_prog;   // Shader for downscaling.

	ES2Mesh quad_mesh;
	ES2Mesh screen_mesh;
	ES2Mesh tv_mesh;

	GLubyte overscan_color;   // Current overscan color (background/zero color).
	GLubyte *overscan_pixels; // Temporary overscan pixels (1x240).

} ES2;

int ES2_Init(double aspect);
void ES2_UpdateController(int idx, double v);
void ES2_SetViewport(int width, int height);
void ES2_VideoChanged();
void ES2_Render(GLubyte *pixels, GLubyte *row_deemp, GLubyte overscan_color);

#endif
