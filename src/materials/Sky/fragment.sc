#ifndef INSTANCING
  $input v_worldPos, v_underwaterRainTimeDay
#endif

#include <bgfx_shader.sh>

#ifndef INSTANCING
  #include <newb/main.sh>
  uniform vec4 TimeOfDay;
  uniform vec4 Day;
  uniform vec4 FogColor;
  uniform vec4 FogAndDistanceControl;
#endif

void main() {
  #ifndef INSTANCING
    vec3 viewDir = normalize(v_worldPos);

    nl_environment env;
    env.end = false;
    env.nether = false;
    env.underwater = v_underwaterRainTimeDay.x > 0.5;
    env.rainFactor = v_underwaterRainTimeDay.y;
    env.dayFactor = v_underwaterRainTimeDay.w;
    env.fogCol = FogColor.rgb;
    env = calculateSunParams(env, TimeOfDay.x, Day.x);

    nl_skycolor skycol = nlOverworldSkyColors(env);

    vec3 skyColor = nlRenderSky(skycol, env, -viewDir, v_underwaterRainTimeDay.z, true);
    #ifdef NL_SHOOTING_STAR
      skyColor += NL_SHOOTING_STAR*nlRenderShootingStar(viewDir, env.fogCol, v_underwaterRainTimeDay.z);
    #endif
    #ifdef NL_GALAXY_STARS
      skyColor += NL_GALAXY_STARS*nlRenderGalaxy(viewDir, env.fogCol, env, v_underwaterRainTimeDay.z);
    #endif

    vec3 cloudPos = viewDir / viewDir.y;
    #ifdef NP_CIRRUS_CLOUD
      float cirrus = NP_CloudCirrus(cloudPos, v_underwaterRainTimeDay.z);
      skyColor = mix(skyColor, 1.3*skycol.horizon, cirrus);
    #endif
    #ifdef NP_VOLUME_CLOUD
      vec4 cloudVol = vec4(0.0,0.0,0.0,0.0);
      #if NP_VOLUME_CLOUD == 2
        cloudPos.xz *= 0.6;
        cloudVol = NP_RenderClouds2D(cloudPos, normalize(cloudPos), v_underwaterRainTimeDay.z, v_underwaterRainTimeDay.w);
        cloudVol.rgb *= 1.3*skycol.horizon;
      #elif NP_VOLUME_CLOUD == 3
        cloudVol = vec4(0.0,0.0,0.0,0.0);
      #endif
      skyColor = mix(skyColor, cloudVol.rgb, cloudVol.a);
    #endif
    skyColor = colorCorrection(skyColor);

    gl_FragColor = vec4(skyColor, 1.0);
  #else
    gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
  #endif
}
