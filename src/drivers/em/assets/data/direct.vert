attribute vec4 a_0;
attribute vec2 a_2;
uniform float u_vScale;
varying vec2 v_uv;
void main ()
{
  gl_Position = a_0;
  vec2 tmpvar_1;
  tmpvar_1.x = a_2.x;
  tmpvar_1.y = ((u_vScale * (a_2.y - 0.5)) + 0.5);
  v_uv = tmpvar_1;
}

