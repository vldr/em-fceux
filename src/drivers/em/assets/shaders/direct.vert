attribute vec4 a_0;
attribute vec2 a_2;

uniform float u_vScale;

varying vec2 v_uv;

void main()
{
	gl_Position = a_0;
    v_uv = vec2(a_2.x, u_vScale * (a_2.y-0.5) + 0.5);
}
