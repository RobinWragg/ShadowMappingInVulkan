#version 450

// This shader is in view-space.

layout(location = 0) in vec3 surfacePos;
layout(location = 1) in vec3 interpVertNormalInWorld;
layout(location = 2) in vec3 lightPosInWorld;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform LightMatrices {
  mat4 view;
  mat4 proj;
} lightMatrices;

layout(set = 3, binding = 0) uniform LightViewOffsets {
  vec2 value0;
  vec2 value1;
  vec2 value2;
  vec2 value3;
  vec2 value4;
  vec2 value5;
  vec2 value6;
  vec2 value7;
  vec2 value8;
  vec2 value9;
  vec2 value10;
  vec2 value11;
  vec2 value12;
  vec2 value13;
} lightViewOffsets;

layout(push_constant) uniform Config {
  int shadowMapCount;
  int shadowAntiAliasSize;
  bool renderTexture; // Not used in this shader
  bool renderNormalMap; // Not used in this shader
} config;

layout(set = 4, binding = 0) uniform sampler2D shadowMap0;
layout(set = 5, binding = 0) uniform sampler2D shadowMap1;
layout(set = 6, binding = 0) uniform sampler2D shadowMap2;
layout(set = 7, binding = 0) uniform sampler2D shadowMap3;
layout(set = 8, binding = 0) uniform sampler2D shadowMap4;
layout(set = 9, binding = 0) uniform sampler2D shadowMap5;
layout(set = 10, binding = 0) uniform sampler2D shadowMap6;
layout(set = 11, binding = 0) uniform sampler2D shadowMap7;
layout(set = 12, binding = 0) uniform sampler2D shadowMap8;
layout(set = 13, binding = 0) uniform sampler2D shadowMap9;
layout(set = 14, binding = 0) uniform sampler2D shadowMap10;
layout(set = 15, binding = 0) uniform sampler2D shadowMap11;
layout(set = 16, binding = 0) uniform sampler2D shadowMap12;
layout(set = 17, binding = 0) uniform sampler2D shadowMap13;

float getShadowMapTexel(int index, vec2 texCoord) {
  switch (index) {
    case 0: return texture(shadowMap0, texCoord).r;
    case 1: return texture(shadowMap1, texCoord).r;
    case 2: return texture(shadowMap2, texCoord).r;
    case 3: return texture(shadowMap3, texCoord).r;
    case 4: return texture(shadowMap4, texCoord).r;
    case 5: return texture(shadowMap5, texCoord).r;
    case 6: return texture(shadowMap6, texCoord).r;
    case 7: return texture(shadowMap7, texCoord).r;
    case 8: return texture(shadowMap8, texCoord).r;
    case 9: return texture(shadowMap9, texCoord).r;
    case 10: return texture(shadowMap10, texCoord).r;
    case 11: return texture(shadowMap11, texCoord).r;
    case 12: return texture(shadowMap12, texCoord).r;
    case 13: return texture(shadowMap13, texCoord).r;
  };
  return 0;
}

vec2 getLightViewOffset(int index) {
  switch (index) {
    case 0: return lightViewOffsets.value0;
    case 1: return lightViewOffsets.value1;
    case 2: return lightViewOffsets.value2;
    case 3: return lightViewOffsets.value3;
    case 4: return lightViewOffsets.value4;
    case 5: return lightViewOffsets.value5;
    case 6: return lightViewOffsets.value6;
    case 7: return lightViewOffsets.value7;
    case 8: return lightViewOffsets.value8;
    case 9: return lightViewOffsets.value9;
    case 10: return lightViewOffsets.value10;
    case 11: return lightViewOffsets.value11;
    case 12: return lightViewOffsets.value12;
    case 13: return lightViewOffsets.value13;
  };
  return lightViewOffsets.value0;
}

// Returns the degree to which a world position is shadowed.
// 0 for no shadow, 1 for completely shadowed.
float getShadowFactorFromMap(vec3 posInWorld, int shadowMapIndex) {
  vec3 posInLightView = (lightMatrices.view * vec4(posInWorld, 1)).xyz;
  posInLightView.xy += getLightViewOffset(shadowMapIndex);
  
  // All shadowmaps have the same dimensions, so we just get the size of shadowmap 0.
  float texelSize = 1.0 / textureSize(shadowMap0, 0).r;
  
  const vec4 posInLightProj = lightMatrices.proj * vec4(posInLightView, 1);
  
  // This is the perspective division that transforms projection space into normalised device space.
  const vec3 normalisedDevicePos = posInLightProj.xyz / posInLightProj.w;
  
  // Change the bounds from [-1,1] to [0,1].
  vec2 centreTexCoord = normalisedDevicePos.xy * 0.5 + 0.5;
  
  const float posToLightPosDistance = length(posInLightView);
  
  // This is necessary due to floating point inaccuracy. This equates to a tenth of a millimeter in world/lightView space, so it's not noticeable.
  const float epsilon = 0.0001;
  
  int totalSampleCount = 0;
  int shadowSampleCount = 0;
  
  // Take samples from a square from a kernel.
  for (int x = -config.shadowAntiAliasSize; x <= config.shadowAntiAliasSize; x++) {
    for (int y = -config.shadowAntiAliasSize; y <= config.shadowAntiAliasSize; y++) {
      totalSampleCount++;
      
      const vec2 texCoordWithOffset = centreTexCoord + vec2(x, y) * texelSize;
      
      const float lightTravelDistance = getShadowMapTexel(shadowMapIndex, texCoordWithOffset);
      
      if (posToLightPosDistance - lightTravelDistance > epsilon) {
        shadowSampleCount++;
      }
    }
  }
  
  return float(shadowSampleCount) / totalSampleCount;
}

float getTotalShadowFactor(vec3 posInWorld) {
  float totalFactor = 0;
  
  for (int i = 0; i < config.shadowMapCount; i++) {
    totalFactor += getShadowFactorFromMap(posInWorld, i);
  }
  
  return totalFactor / config.shadowMapCount;
}

void main() {
  const float ambReflectionConst = 0;
  const float diffuseReflectionConst = 1;
  const float specReflectionConst = 0.5;
  const vec3 color = vec3(1);
  
  // Interpolation can cause normals to be non-unit length, so we re-normalise them here
  const vec3 surfaceNormal = normalize(interpVertNormalInWorld);
  
  const vec3 surfaceToLightDirectionUnit = normalize(lightPosInWorld - surfacePos);
  
  const vec3 reflectionDirectionUnit = reflect(surfaceToLightDirectionUnit, surfaceNormal);
  
  const float diffuseReflection = diffuseReflectionConst * dot(surfaceNormal, surfaceToLightDirectionUnit);
  
  // const float specReflection = specReflectionConst * dot(reflectionDirectionUnit, surfaceToLightDirectionUnit);
  
  const float totalReflection = ambReflectionConst + diffuseReflection;
  
  outColor = vec4(color * totalReflection, 1);
  
  // if (lightNormalDot > 0) {
  //   // Attenuate the fragment color if it is in shadow
  //   float maxLightAttenuation = 0.7;
  //   float lightAttenuation = getTotalShadowFactor(surfacePos) * maxLightAttenuation;
  //   outColor.rgb *= 1 - lightAttenuation;
  // }
}






