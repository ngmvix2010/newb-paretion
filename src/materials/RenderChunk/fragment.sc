$input v_color0, v_color1, v_fog, v_refl, v_texcoord0, v_lightmapUV, v_extra, v_extra2, v_worldPos

#include <bgfx_shader.sh>
#include <newb/main.sh>

SAMPLER2D_AUTOREG(s_MatTexture);
SAMPLER2D_AUTOREG(s_SeasonsTexture);
SAMPLER2D_AUTOREG(s_LightMapTexture);

uniform vec4 RenderChunkFogAlpha;
uniform vec4 FogAndDistanceControl;
uniform vec4 ViewPositionAndTime;
uniform vec4 FogColor;
uniform vec4 DimensionID;
uniform vec4 TimeOfDay;
uniform vec4 Day;
uniform vec4 CameraPosition;

void main() {
  #if defined(DEPTH_ONLY_OPAQUE) || defined(DEPTH_ONLY) || defined(INSTANCING)
    gl_FragColor = vec4(1.0,1.0,1.0,1.0);
    return;
  #endif
  vec3 viewDir = normalize(v_worldPos);
  nl_environment env = nlDetectEnvironment(DimensionID.x, TimeOfDay.x, Day.x, FogColor.rgb, FogAndDistanceControl.xyz);
  nl_skycolor skycol = nlSkyColors(env);
  float t = ViewPositionAndTime.w;

  vec4 diffuse;
  #ifdef NP_LOWPOLY_CHUNK
    if (v_extra2.x > (NP_LOWPOLY_CHUNK_MIN_DISTANCE*4.0)){
      diffuse = texture2DLod(s_MatTexture, v_texcoord0, NP_LOWPOLY_CHUNK*4.0);
    } else if (v_extra2.x > (NP_LOWPOLY_CHUNK_MIN_DISTANCE*2.0)){
      diffuse = texture2DLod(s_MatTexture, v_texcoord0, NP_LOWPOLY_CHUNK*2.0);
    } else if (v_extra2.x > NP_LOWPOLY_CHUNK_MIN_DISTANCE){
      diffuse = texture2DLod(s_MatTexture, v_texcoord0, NP_LOWPOLY_CHUNK);
    } else {
      diffuse = texture2D(s_MatTexture, v_texcoord0);
    }
  #else
    diffuse = texture2D(s_MatTexture, v_texcoord0);
  #endif

  vec4 color = v_color0;

  #ifdef ALPHA_TEST
    if (diffuse.a < 0.6) {
      discard;
    }
  #endif

  #if defined(SEASONS) && (defined(OPAQUE) || defined(ALPHA_TEST))
    diffuse.rgb *= mix(vec3(1.0,1.0,1.0), texture2D(s_SeasonsTexture, v_color1.xy).rgb * 2.0, v_color1.z);
  #endif

  vec3 glow = nlGlow(s_MatTexture, v_texcoord0, v_extra.a);

  diffuse.rgb *= diffuse.rgb;

  #if defined(TRANSPARENT) && !(defined(SEASONS) || defined(RENDER_AS_BILLBOARDS))
    if (v_extra.b > 0.9) {
      diffuse.rgb = vec3_splat(1.0 - NL_WATER_TEX_OPACITY*(1.0 - diffuse.b*1.8));
      diffuse.a = color.a;
    }
  #else
    diffuse.a = 1.0;
  #endif

  diffuse.rgb *= color.rgb;
  diffuse.rgb += glow;
  #ifdef NP_VOLUME_CLOUD
    #ifdef NP_VOLUME_CLOUD_SHADOW
      float nightFactor = step(env.dayFactor, 0.0);
      float dawnFactor = 1.0-env.dayFactor*env.dayFactor;
      dawnFactor *= dawnFactor*dawnFactor;
      dawnFactor *= mix(1.0, dawnFactor*dawnFactor, nightFactor);
      vec3 mainLightDir = env.sunDir.y > 0.0 ? env.sunDir : env.moonDir;
      vec3 gPos = v_worldPos + CameraPosition.xyz;
      float cloudRelativeHeight = gPos.y-187.0;
      vec2 projectionOffset = cloudRelativeHeight*mainLightDir.xz/mainLightDir.y;
      vec2 projectedPos = gPos.xz + projectionOffset;
      float cloudFade = smoothstep(1.0, 0.5, length(0.002*(v_worldPos.xz + projectionOffset)));
      cloudFade *= (1.0-dawnFactor*dawnFactor)*clamp(-0.12*(cloudRelativeHeight-7.0), 0.0, 1.0);
      diffuse.rgb *= 1.0 - NP_VOLUME_CLOUD_SHADOW*smoothstep(0.6, 0.0, NP_CloudMap(projectedPos.xyy*0.3, t, 1.0, env.rainFactor)*cloudFade);
    #endif
  #endif

  if (v_extra.b > 0.9) {
    diffuse.rgb += v_refl.rgb*v_refl.a;
  } else if (v_refl.a > 0.0) {
    // reflective effect - only on xz plane
    float dy = abs(dFdy(v_extra.g));
    if (dy < 0.0002) {
      float mask = v_refl.a*(clamp(v_extra.r*10.0,8.2,8.8)-7.8);
      diffuse.rgb *= 1.0 - 0.6*mask;
      diffuse.rgb += v_refl.rgb*mask;
    }
  }

  diffuse.rgb = mix(diffuse.rgb, v_fog.rgb, v_fog.a);

  diffuse.rgb = colorCorrection(diffuse.rgb);

  gl_FragColor = diffuse;
}
