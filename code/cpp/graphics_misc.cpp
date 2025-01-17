#include "graphics.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace gfx {
  void loadImage(const char *filePath, bool normalMap, VkImage *imageOut, VkDeviceMemory *memoryOut, VkImageView *viewOut) {
    VkFormat format = normalMap ? VK_FORMAT_R8G8B8A8_SNORM : VK_FORMAT_R8G8B8A8_UNORM;
    
    int width, height, componentsPerPixel;
    uint8_t *data = stbi_load(filePath, &width, &height, &componentsPerPixel, 4);
    SDL_assert_release(data != nullptr);
    
    if (normalMap) {
      for (uint32_t i = 0; i < width*height*4; i += 4) {
        data[i] = data[i] - 127;
        data[i+1] = data[i+2] / 2;
        data[i+2] = data[i+1] - 127;
      }
    }
    
    gfx::createImage(format, width, height, imageOut, memoryOut);
    
    gfx::setImageMemoryRGBA(*imageOut, *memoryOut, width, height, data);
    
    *viewOut = gfx::createImageView(*imageOut, format, VK_IMAGE_ASPECT_COLOR_BIT);
    stbi_image_free(data);
  }
    
  void submitCommandBuffer(VkCommandBuffer cmdBuffer, VkSemaphore optionalWaitSemaphore, VkPipelineStageFlags optionalWaitStage, VkSemaphore optionalSignalSemaphore, VkFence optionalFence) {
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    
    if (optionalWaitSemaphore != VK_NULL_HANDLE) {
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &optionalWaitSemaphore;
      submitInfo.pWaitDstStageMask = &optionalWaitStage;
    }
    
    if (optionalSignalSemaphore != VK_NULL_HANDLE) {
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &optionalSignalSemaphore;
    }
    
    auto result = vkQueueSubmit(queue, 1, &submitInfo, optionalFence);
    SDL_assert(result == VK_SUCCESS);
  }
  
  static void cmdTransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    
    barrier.image = image;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;
    
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else SDL_assert_release(false); // Unsupported layout transition
    
    vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }
  
  static void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    
    // Create, fill, and submit the command buffer
    auto cmdBuffer = createCommandBuffer();
    
    beginCommandBuffer(cmdBuffer);
    
    cmdTransitionImageLayout(cmdBuffer, image, oldLayout, newLayout);
    
    vkEndCommandBuffer(cmdBuffer);
    
    submitCommandBuffer(cmdBuffer);
    
    // Clean up
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
  }
  
  static void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    
    VkBufferImageCopy copyCmdInfo = {};
    copyCmdInfo.bufferOffset = 0;
    copyCmdInfo.bufferRowLength = 0;
    copyCmdInfo.bufferImageHeight = 0;

    copyCmdInfo.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyCmdInfo.imageSubresource.mipLevel = 0;
    copyCmdInfo.imageSubresource.baseArrayLayer = 0;
    copyCmdInfo.imageSubresource.layerCount = 1;

    copyCmdInfo.imageOffset = {0, 0, 0};
    copyCmdInfo.imageExtent = {width, height, 1};
    
    // Create, fill, and submit the command buffer
    auto cmdBuffer = createCommandBuffer();
    
    beginCommandBuffer(cmdBuffer);
    
    vkCmdCopyBufferToImage(cmdBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyCmdInfo);
    
    vkEndCommandBuffer(cmdBuffer);
    
    submitCommandBuffer(cmdBuffer);
    
    // Clean up
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
  }
  
  void setBufferMemory(VkDeviceMemory memory, uint64_t dataSize, const void *data) {
    uint8_t *mappedMemory;
    auto result = vkMapMemory(device, memory, 0, dataSize, 0, (void**)&mappedMemory);
    SDL_assert(result == VK_SUCCESS);
        
    memcpy(mappedMemory, data, dataSize);
    
    vkUnmapMemory(device, memory);
  }
  
  void setImageMemoryRGBA(VkImage image, VkDeviceMemory memory, uint32_t width, uint32_t height, const uint8_t *data) {
    
    uint64_t dataSizePerPixel = sizeof(uint8_t) * 4; // R+G+B+A
    uint64_t dataSizeTotal = dataSizePerPixel * width * height;
    
    // Create and fill a staging buffer with the image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, dataSizeTotal, &stagingBuffer, &stagingBufferMemory);
    setBufferMemory(stagingBufferMemory, dataSizeTotal, data);
    
    // Let the GPU optimise the image for receiving buffer data
    transitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // Copy the image data from the buffer to the image
    copyBufferToImage(stagingBuffer, image, width, height);
    
    // Let the GPU optimise the image for shader access
    transitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    // Clean up
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
  }
  
  void beginCommandBuffer(VkCommandBuffer cmdBuffer) {
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    
    // This call will implicitly reset the command buffer if it has previously been filled, thanks to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT.
    auto result = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    SDL_assert(result == VK_SUCCESS);
  }
  
  void cmdBeginRenderPass(VkRenderPass renderPass, uint32_t width, uint32_t height, vec3 clearColor, VkFramebuffer framebuffer, VkCommandBuffer cmdBuffer) {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = renderPass;
    info.framebuffer = framebuffer;
    
    static vector<VkClearValue> clearValues(3);
    
    clearValues[0].color.float32[0] = clearColor.x;
    clearValues[0].color.float32[1] = clearColor.y;
    clearValues[0].color.float32[2] = clearColor.z;
    clearValues[0].color.float32[3] = 1;
    
    // TODO: Is this unnecessary?
    clearValues[1].color.float32[0] = clearColor.x;
    clearValues[1].color.float32[1] = clearColor.y;
    clearValues[1].color.float32[2] = clearColor.z;
    clearValues[1].color.float32[3] = 1;
    
    clearValues[2].depthStencil.depth = 1;
    clearValues[2].depthStencil.stencil = 0;
    
    info.clearValueCount = (int)clearValues.size();
    info.pClearValues = clearValues.data();

    info.renderArea.offset = { 0, 0 };
    info.renderArea.extent.width = width;
    info.renderArea.extent.height = height;
    
    vkCmdBeginRenderPass(cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }
  
  void presentFrame(const SwapchainFrame *frame, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &frame->index;
    
    auto result = vkQueuePresentKHR(queue, &presentInfo);
    SDL_assert(result == VK_SUCCESS);
  }
}








