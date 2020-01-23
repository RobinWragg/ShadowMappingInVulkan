#include "graphics.h"

namespace gfx {
  
  const VkFormat surfaceFormat            = VK_FORMAT_B8G8R8A8_UNORM;
  const VkColorSpaceKHR surfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  
  const vector<const char *> requiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
  
  VkInstance               instance         = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMsgr        = VK_NULL_HANDLE;
  VkSurfaceKHR             surface          = VK_NULL_HANDLE;
  VkPhysicalDevice         physDevice       = VK_NULL_HANDLE;
  VkDevice                 device           = VK_NULL_HANDLE;
  VkQueue                  queue            = VK_NULL_HANDLE;
  int                      queueFamilyIndex = -1;
  
  VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT msgType, const VkDebugUtilsMessengerCallbackDataEXT *data, void *pUserData) {

    printf("\n");

    switch (severity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: printf("verbose, "); break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: printf("info, "); break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: printf("WARNING, "); break;
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: printf("ERROR, "); break;
      default: printf("unknown, "); break;
    };

    switch (msgType) {
      case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: printf("general: "); break;
      case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: printf("validation: "); break;
      case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: printf("performance: "); break;
      default: printf("unknown: "); break;
    };

    printf("%s (%i objects reported)\n", data->pMessage, data->objectCount);
    fflush(stdout);

    switch (severity) {
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        SDL_assert_release(false);
        break;
      default: break;
    };

    return VK_FALSE;
  }
  
  static void createDebugMessenger() {
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    // createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
    createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    createInfo.pfnUserCallback = debugCallback;

    auto createDebugUtilsMessenger
      = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    
    createDebugUtilsMessenger(instance, &createInfo, nullptr, &debugMsgr);
  }
  
  static VkInstance createInstance(SDL_Window *window) {
    
    // Print available layers
    printf("\nAvailable instance layers:\n");
    vector<VkLayerProperties> availableLayers;
    getAvailableInstanceLayers(&availableLayers);
    for (const auto& layer : availableLayers) printf("\t%s\n", layer.layerName);
    
    // Get required extensions from SDL
    unsigned int extensionCount;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());
    
    #ifdef DEBUG
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    #endif
    
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.enabledExtensionCount = (int)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    // Enable the layers
    auto layers = getRequiredLayers();
    createInfo.enabledLayerCount = (int)layers.size();
    createInfo.ppEnabledLayerNames = layers.data();
    
    printf("\nEnabled instance layers:\n");
    for (auto &layer : layers) printf("\t%s\n", layer);
    printf("\n");
    
    vkCreateInstance(&createInfo, nullptr, &instance);
    
    return instance;
  }

  static VkDeviceQueueCreateInfo createQueueInfo() {
    
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &familyCount, families.data());

    // Choose the first queue family of the required type
    int familyIndex;
    for (familyIndex = 0; familyIndex < families.size(); familyIndex++) {
      VkBool32 hasSurfaceSupport;
      vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, familyIndex, surface, &hasSurfaceSupport);
      
      if (hasSurfaceSupport == VK_TRUE
        && families[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        break;
      }
    }

    SDL_assert_release(familyIndex < families.size());
    
    VkDeviceQueueCreateInfo info = {};
    
    info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    info.queueFamilyIndex = familyIndex;
    info.queueCount = 1;
    
    static float defaultPriority = 1.0f;
    info.pQueuePriorities = &defaultPriority;
    
    return info;
  }

  static void createDeviceAndQueue() {
    
    VkDeviceQueueCreateInfo queueInfo = createQueueInfo();
    
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueInfo;
    
    VkPhysicalDeviceFeatures enabledDeviceFeatures = {};
    deviceCreateInfo.pEnabledFeatures = &enabledDeviceFeatures;
    
    // Enable extensions
    deviceCreateInfo.enabledExtensionCount = (int)requiredDeviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
    
    // Enable validation layers for the device, same as the instance
    auto requiredLayers = getRequiredLayers();
    deviceCreateInfo.enabledLayerCount = (int)requiredLayers.size();
    deviceCreateInfo.ppEnabledLayerNames = requiredLayers.data();
    
    auto result = vkCreateDevice(physDevice, &deviceCreateInfo, nullptr, &device);
    SDL_assert_release(result == VK_SUCCESS);
    
    // Get a handle to the new queue
    vkGetDeviceQueue(device, queueInfo.queueFamilyIndex, 0, &queue);
    SDL_assert_release(queue != VK_NULL_HANDLE);
    
    queueFamilyIndex = queueInfo.queueFamilyIndex;
  }
  
  void createBuffer(VkBufferUsageFlagBits usage, uint64_t dataSize, VkBuffer *bufferOut, VkDeviceMemory *memoryOut) {
    
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = dataSize;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    auto result = vkCreateBuffer(device, &bufferInfo, nullptr, bufferOut);
    SDL_assert(result == VK_SUCCESS);
    
    VkMemoryRequirements memoryReqs;
    vkGetBufferMemoryRequirements(device, *bufferOut, &memoryReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryReqs.size;
    allocInfo.memoryTypeIndex = getMemoryType(memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(device, &allocInfo, nullptr, memoryOut);
    SDL_assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(device, *bufferOut, *memoryOut, 0);
    SDL_assert(result == VK_SUCCESS);
  }
  
  void createCoreHandles(SDL_Window *window) {
    instance = createInstance(window);
    
    #ifdef DEBUG
      createDebugMessenger();
    #endif
    
    auto result = SDL_Vulkan_CreateSurface(window, instance, &surface);
    SDL_assert_release(result == SDL_TRUE);
    
    physDevice = getPhysicalDevice(window);
    
    createDeviceAndQueue();
    
    SDL_assert_release(instance != VK_NULL_HANDLE);
    SDL_assert_release(surface != VK_NULL_HANDLE);
    SDL_assert_release(physDevice != VK_NULL_HANDLE);
    SDL_assert_release(device != VK_NULL_HANDLE);
    SDL_assert_release(queue != VK_NULL_HANDLE);
    SDL_assert_release(queueFamilyIndex >= 0);
  }
  
  VkImage createImage(VkFormat format) {
    
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    
    auto extent = getSurfaceExtent();
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkImage image;
    auto result = vkCreateImage(device, &imageInfo, nullptr, &image);
    SDL_assert_release(result == VK_SUCCESS);
    
    return image;
  }
  
  VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectMask) {
    
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    
    viewInfo.image = image;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectMask;
    
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkImageView view;
    auto result = vkCreateImageView(device, &viewInfo, nullptr, &view);
    SDL_assert_release(result == VK_SUCCESS);
    
    return view;
  }
}








