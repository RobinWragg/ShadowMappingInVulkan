#ifdef __APPLE__
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <cstdio>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

// #include <vulkan/vulkan.h>

using namespace glm;
using namespace std;

float getTime() {
	static uint64_t startCount = SDL_GetPerformanceCounter();
	return (SDL_GetPerformanceCounter() - startCount) / (double)SDL_GetPerformanceFrequency();
}

void monitorFramerate(float deltaTime) {
	static vector<float> frameTimes;

	frameTimes.push_back(deltaTime);

	if (frameTimes.size() >= 100) {
		float worstTime = 0;
		for (auto time : frameTimes) if (time > worstTime) worstTime = time;
		frameTimes.resize(0);
		printf("Worst frame out of 100: %.2f ms (%.1f fps)\n", worstTime * 1000, 1 / worstTime);
	}
}

SDL_Renderer *renderer;
int rendererWidth, rendererHeight;

int main(int argc, char* argv[]) {
	int result = SDL_Init(SDL_INIT_EVERYTHING);
	SDL_assert_release(result == 0);
    
	char *path = SDL_GetBasePath();
    #ifdef __APPLE__
    SDL_assert_release(chdir(path) == 0);
    #else
    SDL_assert_release(SetCurrentDirectory(path));
    #endif
	SDL_free(path);

	// create a 4:3 SDL window
	int windowWidth = 600;
	int windowHeight = 400;

	SDL_Window *window = SDL_CreateWindow(
		"Shadow Mapping", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		windowWidth, windowHeight, SDL_WINDOW_VULKAN);
	SDL_assert_release(window != NULL);
	
	bool running = true;
	while (running) {
		float deltaTime;
		{
			static double previousTime = 0;
			double timeNow = getTime();
			deltaTime = (float)(timeNow - previousTime);
			previousTime = timeNow;
		}
		
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT: running = false; break;
			}
		}
		
		printf("(render)\n");
	}

	SDL_Quit();

	return 0;
}




