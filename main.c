#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

int main(int argc, char *argv[]) {
  SDL_Window *window;
  SDL_Renderer *renderer;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Could not init SDL\n");
    exit(1);
  }

  if (SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window,
                                  &renderer) < 0) {
    fprintf(stderr, "could not create sdl window and renderer\n");
    exit(1);
  }

  int running = 1;

  SDL_Event ev;
  while(running) {
    // event handling logic and state updates
    while(SDL_PollEvent(&ev)) {
      switch(ev.type) {
        case SDL_QUIT:
          running = 0;
          break;
        case SDL_WINDOWEVENT_CLOSE:
          running = 0;
          break;
        case SDL_KEYDOWN:
          if(ev.key.keysym.sym == SDLK_ESCAPE) {
            running = 0;
          }
          break;
      }
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  exit(0);
}