#ifdef __APPLE__
#include <unistd.h>
#else
#include <direct.h>
#define chdir _chdir
#endif

#include <string>
#include <fstream>

#include "main.h"
#include "input.h"
#include "shadowMapViewer.h"
#include "graphics.h"
#include "ShadowMap.h"
#include "presentation.h"
#include "shadows.h"
#include "geometry.h"
#include "gui.h"
#include "settings.h"

double getTime() {
  static uint64_t startCount = SDL_GetPerformanceCounter();
  return (SDL_GetPerformanceCounter() - startCount) / (double)SDL_GetPerformanceFrequency();
}

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

void setWorkingDir() {
  char *path = SDL_GetBasePath();
  string assetsPath = string(path) + "/assets";
  auto result = chdir(assetsPath.c_str());
  SDL_assert_release(result == 0);
  SDL_free(path);
}

vector<uint8_t> loadBinaryFile(const char *filename) {
  ifstream file(filename, ios::ate | ios::binary);

  printf("LOADING: %s\n", filename);
  SDL_assert_release(file.is_open());

  vector<uint8_t> bytes(file.tellg());
  file.seekg(0);
  file.read((char*)bytes.data(), bytes.size());
  file.close();

  return bytes;
}

vector<ShadowMap> shadowMaps;

VkSemaphore imageAvailableSemaphore  = VK_NULL_HANDLE;
VkSemaphore renderCompletedSemaphore = VK_NULL_HANDLE;

void createSemaphores() {
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkCreateSemaphore(gfx::device, &semaphoreInfo, nullptr, &imageAvailableSemaphore);
  SDL_assert_release(vkCreateSemaphore(gfx::device, &semaphoreInfo, nullptr, &renderCompletedSemaphore) == VK_SUCCESS);
}

void renderNextFrame(float deltaTime) {
  presentation::update(deltaTime);
  shadows::update();
  
  gfx::SwapchainFrame *frame = gfx::getNextFrame(imageAvailableSemaphore);
  
  // Wait for the command buffer to finish executing
  vkQueueWaitIdle(gfx::queue);
  
  gfx::beginCommandBuffer(frame->cmdBuffer);
  
  shadows::performRenderPasses(frame->cmdBuffer);
  
  auto extent = gfx::getSurfaceExtent();
  vec3 clearColor = {0.5, 0.7, 1};
  gfx::cmdBeginRenderPass(gfx::renderPass, extent.width, extent.height, clearColor, frame->framebuffer, frame->cmdBuffer);
  presentation::render(frame->cmdBuffer, &shadowMaps);
  
  gui::render(frame->cmdBuffer);
  vkCmdEndRenderPass(frame->cmdBuffer);
  
  auto result = vkEndCommandBuffer(frame->cmdBuffer);
  SDL_assert(result == VK_SUCCESS);
  
  // Submit the command buffer
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  gfx::submitCommandBuffer(frame->cmdBuffer, imageAvailableSemaphore, waitStage, renderCompletedSemaphore, VK_NULL_HANDLE);
  
  gfx::presentFrame(frame, renderCompletedSemaphore);
}

int main(int argc, char* argv[]) {
  
  #ifdef _DEBUG
  printf("Debug build\n");
  printf("Validation enabled\n");
  const char *windowTitle = "Light and Shadow (debug build)";
  #else
  printf("Release build\n");
  printf("Validation disabled\n");
  const char *windowTitle = "Light and Shadow (release build)";
  #endif
  
  printf("main()\n");
  fflush(stdout);
  
  int result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
  SDL_assert_release(result == 0);
  
  setWorkingDir();

  // create a window slightly smaller than the display resolution
  int windowWidth;
  int windowHeight;
  {
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    int extraRoom = 200;
    windowWidth = displayMode.w - extraRoom;
    windowHeight = displayMode.h - extraRoom;
  }

  SDL_Window *window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_assert_release(window != NULL);
  printf("Created window\n");
  fflush(stdout);
  
  gfx::createCoreHandles(window);
  createSemaphores();
  
  for (int i = 0; i < MAX_LIGHT_SUBSOURCE_COUNT; i++) shadowMaps.push_back(ShadowMap(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION));
  
  geometry::init();
  shadows::init(&shadowMaps);
  presentation::init();
  shadowMapViewer::init(&shadowMaps);
  gui::init(window);
  
  bool running = true;
  
  printf("Beginning frame loop\n");
  fflush(stdout);
  
  while (running) {
    float deltaTime;
    {
      static double previousTime = 0;
      double timeNow = getTime();
      deltaTime = (float)(timeNow - previousTime);
      previousTime = timeNow;
    }
    
    SDL_Event event;
    input::handleMouseMotion(0, 0);
    while (SDL_PollEvent(&event)) {
      gui::processSdlEvent(&event);
      switch (event.type) {
        // Input for first-person mode not currently used.
        // case SDL_KEYDOWN: {
        //   if (!event.key.repeat) {
        //     input::handleKeyPress(event.key.keysym.sym);
        //   }
        // } break;
        // case SDL_KEYUP: {
        //   input::handleKeyRelease(event.key.keysym.sym);
        // } break;
        // case SDL_MOUSEMOTION: {
        //   input::handleMouseMotion(event.motion.xrel, event.motion.yrel);
        // } break;
        // case SDL_MOUSEBUTTONDOWN: {
        //   input::handleMouseClick(window);
        // } break;
        case SDL_QUIT: running = false; break;
      }
    }
    
    renderNextFrame(deltaTime);
    
    fflush(stdout);
  }
  
  printf("Quitting\n");
  SDL_Quit();
  return 0;
}




