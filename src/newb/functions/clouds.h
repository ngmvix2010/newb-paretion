#ifndef CLOUDS_H
#define CLOUDS_H

#include "detection.h"
#include "noise.h"
#include "sky.h"

// simple clouds 2D noise
float cloudNoise2D(vec2 p, highp float t, float rain) {
  t *= NL_CLOUD1_SPEED;
  p += t;
  p.y += 3.0*sin(0.3*p.x + 0.1*t);

  vec2 p0 = floor(p);
  vec2 u = p-p0;
  u *= u*(3.0-2.0*u);
  vec2 v = 1.0-u;

  float n = mix(
    mix(rand(p0),rand(p0+vec2(1.0,0.0)), u.x),
    mix(rand(p0+vec2(0.0,1.0)),rand(p0+vec2(1.0,1.0)), u.x),
    u.y
  );
  n *= 0.5 + 0.5*sin(p.x*0.6 - 0.5*t)*sin(p.y*0.6 + 0.8*t);
  n = min(n*(1.0+rain), 1.0);
  return n*n;
}

// simple clouds
vec4 renderCloudsSimple(nl_skycolor skycol, vec3 pos, highp float t, float rain) {
  pos.xz *= NL_CLOUD1_SCALE;
  float d = cloudNoise2D(pos.xz, t, rain);
  vec4 col = vec4(skycol.horizonEdge + skycol.zenith, smoothstep(0.1,0.6,d));
  col.rgb += 1.5*dot(col.rgb, vec3(0.3,0.4,0.3))*smoothstep(0.6,0.2,d)*col.a;
  col.rgb *= 1.0 - 0.8*rain;
  return col;
}

// rounded clouds

// rounded clouds 3D density map
float cloudDf(vec3 pos, float rain, vec2 boxiness) {
  boxiness *= 0.999;
  vec2 p0 = floor(pos.xz);
  vec2 u = max((pos.xz-p0-boxiness.x)/(1.0-boxiness.x), 0.0);
  u *= u*(3.0 - 2.0*u);

  vec4 r = vec4(rand(p0), rand(p0+vec2(1.0,0.0)), rand(p0+vec2(1.0,1.0)), rand(p0+vec2(0.0,1.0)));
  r = smoothstep(0.1001+0.2*rain, 0.1+0.2*rain*rain, r); // rain transition

  float n = mix(mix(r.x,r.y,u.x), mix(r.w,r.z,u.x), u.y);

  // round y
  n *= 1.0 - 1.5*smoothstep(boxiness.y, 2.0 - boxiness.y, 2.0*abs(pos.y-0.5));

  n = max(1.25*(n-0.2), 0.0); // smoothstep(0.2, 1.0, n)
  n *= n*(3.0 - 2.0*n);
  return n;
}

vec4 renderCloudsRounded(
    vec3 vDir, vec3 vPos, float rain, float time, vec3 horizonCol, vec3 zenithCol,
    const int steps, const float thickness, const float thickness_rain, const float speed,
    const vec2 scale, const float density, const vec2 boxiness
) {
  float height = 7.0*mix(thickness, thickness_rain, rain);
  float stepsf = float(steps);

  // scaled ray offset
  vec3 deltaP;
  deltaP.y = 1.0;
  deltaP.xz = height*scale*vDir.xz/(0.02+0.98*abs(vDir.y));

  // local cloud pos
  vec3 pos;
  pos.y = 0.0;
  pos.xz = scale*(vPos.xz + vec2(1.0,0.5)*(time*speed));
  pos += deltaP;

  deltaP /= -stepsf;

  // alpha, gradient
  vec2 d = vec2(0.0,1.0);
  for (int i=1; i<=steps; i++) {
    float m = cloudDf(pos, rain, boxiness);
    d.x += m;
    d.y = mix(d.y, pos.y, m);
    pos += deltaP;
  }
  d.x *= smoothstep(0.03, 0.1, d.x);
  d.x /= (stepsf/density) + d.x;

  if (vPos.y < 0.0) { // view from top
    d.y = 1.0 - d.y;
  }

  vec4 col = vec4(zenithCol + horizonCol, d.x);
  col.rgb += dot(col.rgb, vec3(0.3,0.4,0.3))*d.y*d.y;
  col.rgb *= 1.0 - 0.8*rain;
  return col;
}

float cloudsNoiseVr(vec2 p, float t) {
  float n = fastVoronoi2(p + t, 1.8);
  n *= fastVoronoi2(3.0*p + t, 1.5);
  n *= fastVoronoi2(9.0*p + t, 0.4);
  n *= fastVoronoi2(27.0*p + t, 0.1);
  n *= fastVoronoi2(82.0*p + t, 0.02); // more quality
  return n*n;
}

vec4 renderClouds(vec2 p, float t, float rain, vec3 horizonCol, vec3 zenithCol, const vec2 scale, const float velocity, const float shadow) {
  p *= scale;
  t *= velocity;

  // layer 1
  float a = cloudsNoiseVr(p, t);
  float b = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // layer 2
  p = 1.4 * p.yx + vec2(7.8, 9.2);
  t *= 0.5;
  float c = cloudsNoiseVr(p, t);
  float d = cloudsNoiseVr(p + NL_CLOUD3_SHADOW_OFFSET*scale, t);

  // higher = less clouds thickness
  // lower separation betwen x & y = sharper
  vec2 tr = vec2(0.6, 0.7) - 0.12*rain;
  a = smoothstep(tr.x, tr.y, a);
  c = smoothstep(tr.x, tr.y, c);

  // shadow
  b *= smoothstep(0.2, 0.8, b);
  d *= smoothstep(0.2, 0.8, d);

  vec4 col;
  col.a = a + c*(1.0-a);
  col.rgb = horizonCol + horizonCol.ggg;
  col.rgb = mix(col.rgb, 0.5*(zenithCol + zenithCol.ggg), shadow*mix(b, d, c));
  col.rgb *= 1.0-0.7*rain;

  return col;
}

// aurora is rendered on clouds layer
#ifdef NL_AURORA
vec4 renderAurora(vec3 p, float t, float rain, vec3 FOG_COLOR) {
  t *= NL_AURORA_VELOCITY;
  p.xz *= NL_AURORA_SCALE;
  p.xz += 0.05*sin(p.x*4.0 + 20.0*t);

  float d0 = sin(p.x*0.1 + t + sin(p.z*0.2));
  float d1 = sin(p.z*0.1 - t + sin(p.x*0.2));
  float d2 = sin(p.z*0.1 + 1.0*sin(d0 + d1*2.0) + d1*2.0 + d0*1.0);
  d0 *= d0; d1 *= d1; d2 *= d2;
  d2 = d0/(1.0 + d2/NL_AURORA_WIDTH);

  float mask = (1.0-0.8*rain)*max(1.0 - 4.0*max(FOG_COLOR.b, FOG_COLOR.g), 0.0);
  return vec4(NL_AURORA*mix(NL_AURORA_COL1,NL_AURORA_COL2,d1),1.0)*d2*mask;
}
#endif

vec4 nlCloudAuroraReflection(nl_skycolor skycol, nl_environment env, vec3 viewDir, vec3 wPos, vec3 CAMERA_POS, highp float t) {
  vec2 cloudPos = wPos.xz;
  cloudPos += (187.0-(wPos.y+CAMERA_POS.y))*viewDir.xz/viewDir.y;
  float fade = clamp(2.0 - 0.005*length(cloudPos), 0.0, 1.0);
  cloudPos += CAMERA_POS.xz;

  vec4 refl = vec4_splat(0.0);

  #ifdef NL_AURORA
    vec4 aurora = renderAurora(cloudPos.xyy, t, env.rainFactor, env.fogCol);
    aurora.a *= fade;
    refl = vec4(2.0*aurora.rgb*aurora.a, aurora.a);
  #endif

  #if NL_CLOUD_TYPE == 1
    vec4 clouds = renderCloudsSimple(skycol, cloudPos.xyy, t, env.rainFactor);
    clouds.a *= fade;
    refl = vec4(mix(refl.rgb, clouds.rgb, clouds.a), min(refl.a + clouds.a, 1.0));
  #endif

  return refl;
}

/* Newb X Paretion Zone */

// Simple 2D Cirrus Clouds For NXP
float NP_CloudCirrus(vec3 pos, float time){
    float v = NP_CIRRUS_CLOUD_SPEED;
	  vec2 uv = pos.xz * NP_CIRRUS_CLOUD_SCALE;
    
    float df = 0.75*smoothstep(0.4, 0.7, fbm(vec2(uv.x + time*v*0.3, uv.y*0.2 + time*v)));
    df *= fbm(uv*2.0);
	  return df;
}

float NP_CloudMap(vec3 pos, float time, float dh, float getRain) {
    float rainCloudFactor = 1.0 - getRain;
    float density;
    float density2;
    vec3 p = pos*0.86;
    p.x += NP_VOLUME_CLOUD_SPEED*time;
    //p.xz += 0.2*fastVoronoi2(p.xz*2.0, 3.0);
    
    density *= fbm(p.xz);
    density = fbm(p.xz);
    if (NP_VORO_ENABLED == 1){
    	density *= fastVoronoi2(p.xz, 1.8);
    }
    density *= smoothstep((1.0 - NP_VOLUME_CLOUD_DENSITY1)*rainCloudFactor, 1.0, density);
    
    density2 = fbm(p.xz*0.5); 
    if (NP_VORO_ENABLED == 1){
    	density2*= fastVoronoi2(p.xz, 0.8);
    }
    density2 *= smoothstep((1.0 - NP_VOLUME_CLOUD_DENSITY2)*rainCloudFactor , 0.7, density2);
    //density2 *= noise3D(pos)*smoothstep(0.1, 0.3, density2);
    
    float fndf = min(1.0, (density + density2));
    if (NP_VORO_ENABLED == 1){
    	fndf *= mix(1.0, fastVoronoi2(p.xz*2.0 + fbm(p.xz), 3.0), density);
    }
    
    return fndf;
}

vec4 NP_RenderClouds2D(vec3 pos, vec3 viewdir, float time, float getRain){
    float dh = 0.0;
    vec3 cloudC = vec3(0.3);
    float alpha = 0.0;
    float df;
    float d2 = 0.0;

    float cloud_height = 0.0;
    float cloud_thickness = 15.0;

    float dist_to_cloud = abs(pos.y - cloud_height);

    float cloud_factor = smoothstep(cloud_thickness, 0.0, dist_to_cloud);

    float scale_factor = mix(8.0, 3.0, cloud_factor)*1.0; 
    pos.xz *= scale_factor;

    for (int i = 0; i < 10; i++){
        if (dh <= 5.0){
            d2 += 0.2;
            pos.xz *= 0.97;
        } else {
            d2 -= 0.2;
            pos.xz *= 1.03;
        }
        alpha *= 0.86;
        
        df = NP_CloudMap(pos, time, d2, getRain);
        
        df *= cloud_factor;

        alpha = mix(alpha, 1.0, df);
        cloudC = mix(cloudC, cloudC + 0.05, 1.0 - alpha);
    }
    
    return vec4(cloudC, alpha);
}

#endif
