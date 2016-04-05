uniform sampler2D u_idxTex;
uniform sampler2D u_deempTex;
uniform sampler2D u_lookupTex;
uniform sampler2D u_noiseTex;
uniform vec3 u_mins;
uniform vec3 u_maxs;
uniform float u_brightness;
uniform float u_contrast;
uniform float u_color;
uniform float u_gamma;
uniform float u_noiseAmp;

varying vec2 v_uv;
varying vec2 v_deemp_uv;
varying vec2 v_noiseUV;

#define RESCALE(v_) ((v_) * (u_maxs-u_mins) + u_mins)

void main()
{
	float deemp = (255.0/511.0) * 2.0 * texture2D(u_deempTex, v_deemp_uv).r;
	float uv_y = (255.0/511.0) * texture2D(u_idxTex, v_uv).r + deemp;
	// Snatch in RGB PPU; uv.x is already calculated, so just read from lookup tex with u=1.0.
	vec3 yiq = RESCALE(texture2D(u_lookupTex, vec2(1.0, uv_y)).rgb);
	// Add noise to YIQ color, boost/reduce color and convert to RGB.
	yiq.r += u_noiseAmp * (texture2D(u_noiseTex, v_noiseUV).r - 0.5);
	yiq.gb *= u_color;
	vec3 result = clamp(c_convMat * yiq, 0.0, 1.0);
	// Gamma convert RGB from NTSC space to space similar to sRGB.
	result = pow(result, vec3(u_gamma));
	// NOTE: While this seems to be wrong (after gamma), it works well in practice...?
	result = clamp(u_contrast * result + u_brightness, 0.0, 1.0);
	// Write linear color (banding is not visible for pixelated graphics).
// TODO: test encode gamma
	gl_FragColor = vec4(result, 1.0);
}
