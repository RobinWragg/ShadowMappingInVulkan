#pragma once

#include "graphics.h"

class ShadowMap {
public:
  VkFormat format;
  uint32_t width, height;
  
  VkImage image;
  VkDeviceMemory imageMemory;
  VkImageView imageView;
  VkImageView depthImageView;
  
  VkSampler sampler;
  VkDescriptorSet samplerDescriptorSet;
  
  ShadowMap(uint32_t w, uint32_t h) {
    format = VK_FORMAT_R16_SFLOAT;
    
    width = w;
    height = h;
    
    gfx::createImage(format, width, height, &image, &imageMemory);
    imageView = gfx::createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);
    depthImageView = gfx::createDepthImageAndView(width, height);
    
    sampler = gfx::createSampler();
    samplerDescriptorSet = gfx::createDescSet(imageView, sampler);
  }
};





