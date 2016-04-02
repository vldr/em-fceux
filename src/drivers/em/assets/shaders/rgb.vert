attribute vec4 a_0;
attribute vec2 a_2;

uniform vec2 u_noiseRnd;

varying vec2 v_uv;
varying vec2 v_deemp_uv;
varying vec2 v_noiseUV;

void main()
{
	v_uv = a_2;
	v_deemp_uv = vec2(v_uv.y, 0.0);
	v_noiseUV = vec2(IDX_W/NOISE_W, IDX_H/NOISE_H)*a_2 + u_noiseRnd;
	gl_Position = a_0;
}
