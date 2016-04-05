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

varying vec2 v_uv[int(NUM_TAPS)];
varying vec2 v_deemp_uv;
varying vec2 v_noiseUV;

#define P(i_)  p = floor(IDX_W * v_uv[i_])
#define U(i_)  (mod(p.x - p.y, 3.0)*NUM_SUBPS*NUM_TAPS + subp*NUM_TAPS + float(i_)) / (LOOKUP_W-1.0)
#define V(i_)  ((255.0/511.0) * texture2D(u_idxTex, v_uv[i_]).r + deemp)
#define UV(i_) uv = vec2(U(i_), V(i_))
#define RESCALE(v_) ((v_) * (u_maxs-u_mins) + u_mins)
#define SMP(i_) P(i_); UV(i_); yiq += RESCALE(texture2D(u_lookupTex, uv).rgb)

void main()
{
	float deemp = (255.0/511.0) * 2.0 * texture2D(u_deempTex, v_deemp_uv).r;
	float subp = mod(floor(NUM_SUBPS*IDX_W * v_uv[int(NUM_TAPS)/2].x), NUM_SUBPS);
	vec2 p;
	vec2 la;
	vec2 uv;
	vec3 yiq = vec3(0.0);
	SMP(0);
	SMP(1);
	SMP(2);
	SMP(3);
	SMP(4);
	// Working multiplier for filtered chroma to match PPU is 2/5 (for CW2=12).
	// Is this because color fringing with composite?
	yiq *= (8.0/2.0) / vec3(YW2, CW2-2.0, CW2-2.0);
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
