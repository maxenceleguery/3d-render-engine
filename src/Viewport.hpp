#pragma once

#include <thread>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "SceneObjectWindow.hpp"
#include "Environment.hpp"
#include "Camera.hpp"

class Viewport {
    private:
        Glib::RefPtr<Gtk::Application> window_runner;
        Environment* env;
        Camera* cam;

        SDL_Window* window;
        SDL_Renderer* renderer;
        
        SDL_Texture* texture;

        TTF_Font* font;
        SDL_Texture* fps_display;

        std::thread renderThread;
        std::thread sceneObjectWindowThread;
        bool sceneObjectWindowThreadOnline = false;

        uint FPS = 60;
        bool isOnBool = true;

    public:
        Viewport(Environment* env) : env(env), cam(env->cam) {};

        static void windowThread(Viewport* viewport, SceneObject obj) {
            viewport->window_runner = Gtk::Application::create();
            SceneObjectWindow scene(&obj, viewport->cam);
            viewport->window_runner->run(scene);
            viewport->sceneObjectWindowThreadOnline = false;
        }

        void run_window(SceneObject obj) {
            if (sceneObjectWindowThreadOnline) {
                window_runner->quit();
                sceneObjectWindowThreadOnline = false;
            }
            sceneObjectWindowThreadOnline = true;
            sceneObjectWindowThread = std::thread(windowThread, this, obj);
            sceneObjectWindowThread.detach();
        }

        bool isOn() const {
            return isOnBool;
        }

        void initWindow() {
            SDL_Init(SDL_INIT_EVERYTHING);
            TTF_Init();
            window = SDL_CreateWindow("Raytracer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cam->getWidth(), cam->getHeight(), 0);
            renderer = SDL_CreateRenderer(window, -1, 0);
            font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeSansBold.ttf", 24);
            if (font == nullptr) {
                font = TTF_OpenFont("/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf", 24);
                if (font == nullptr) {
                    std::cout << "Font not found" << std::endl;
                }
            }
        }

        void closeWindow() {
            SDL_DestroyTexture(texture);
            SDL_DestroyTexture(fps_display);
            TTF_CloseFont(font);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);

            window_runner->quit();
        }

        void updateTexture() {
            uint width = cam->getWidth();
            uint height = cam->getHeight();
            SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 24, 0, 0, 0, 0);
            unsigned char* surface_pixels = (unsigned char*)surface -> pixels;
            cam->cpu();
            for(uint h = 0; h < height; ++h) {
                for(uint w = 0; w < width; ++w) {
                    surface_pixels[3 * (h * surface->w + w) + 0] = cam->getPixel(h*width+w).getB();
                    surface_pixels[3 * (h * surface->w + w) + 1] = cam->getPixel(h*width+w).getG();
                    surface_pixels[3 * (h * surface->w + w) + 2] = cam->getPixel(h*width+w).getR();
                }
            }
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }

        void updateFPSDisplay() {
            SDL_Color White = {255, 255, 255, 255};
            std::string message = std::to_string(cam->getCurrentFPS()).substr(0, 5) + " FPS";
            SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, message.c_str(), White); 

            SDL_DestroyTexture(fps_display);
            fps_display = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
            SDL_FreeSurface(surfaceMessage);
        }

        static void threadLoop(Viewport* viewport) {
            std::cout << "Viewport launched" << std::endl;
            viewport->initWindow();
            SDL_Event e;
            bool running = true;
            while(running) {
                while (SDL_PollEvent(&e)) {
                    if (e.type == SDL_QUIT) running = false;

                    if (e.type == SDL_KEYDOWN) {
                        switch (e.key.keysym.sym) {
                            // Cam translation
                            case SDLK_UP:
                                viewport->cam->addRelativeOffset(Vector<float>(0.1, 0., 0.));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_DOWN:
                                viewport->cam->addRelativeOffset(Vector<float>(-0.1, 0., 0.));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_LEFT:
                                viewport->cam->addRelativeOffset(Vector<float>(0., 0.1, 0.));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_RIGHT:
                                viewport->cam->addRelativeOffset(Vector<float>(0., -0.1, 0.));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_SPACE:
                                viewport->cam->addRelativeOffset(Vector<float>(0., 0., 0.1));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_LSHIFT:
                                viewport->cam->addRelativeOffset(Vector<float>(0., 0., -0.1));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_r:
                                viewport->cam->toggleRaytracing();
                                SDL_Delay(50);
                                break;
                            
                            // Cam rotation
                            case SDLK_z:
                                viewport->cam->addRelativeRotation(Vector<float>(0, -1, 0));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_s:
                                viewport->cam->addRelativeRotation(Vector<float>(0, 1, 0));
                                viewport->cam->reset_progressive_rendering();
                                break;
                            case SDLK_a:
                                viewport->cam->addRelativeRotation(Vector<float>(1, 0, 0));
                                viewport->cam->reset_progressive_rendering();
                                break;    
                            case SDLK_e:
                                viewport->cam->addRelativeRotation(Vector<float>(-1, 0, 0));
                                viewport->cam->reset_progressive_rendering();
                                break;  
                            case SDLK_q:
                                viewport->cam->addRelativeRotation(Vector<float>(0, 0, 1));
                                viewport->cam->reset_progressive_rendering();
                                break;    
                            case SDLK_d:
                                viewport->cam->addRelativeRotation(Vector<float>(0, 0, -1));
                                viewport->cam->reset_progressive_rendering();
                                break;

                            case SDLK_c:
                                viewport->run_window(*(viewport->cam));
                                break;

                            default:
                                break;
                        }
                        viewport->cam->cuda();
                    }

                    if (e.type == SDL_MOUSEBUTTONDOWN) {
                        if (e.button.button == SDL_BUTTON_LEFT) {
                            Ray ray = viewport->cam->generate_ray(e.button.x, e.button.y);
                            Array<BVH> bvhs = viewport->env->BVHs;
                            bvhs.cpu();
                            Hit hit = Tracing::rayBVHs(ray, bvhs);
                            int index = hit.getBVHindex();
                            std::cout << "Clic :" << index << " " << e.button.x << " " << e.button.y << std::endl;

                            BVH bvh = viewport->env->BVHs[index];
                            viewport->run_window(bvh);
                            //bvh.addRelativeRotation(Vector<float>(0, 1, 0));
                            viewport->cam->reset_progressive_rendering();
                        }
                    }
                }
                viewport->updateTexture();
                SDL_RenderCopy(viewport->renderer, viewport->texture, nullptr, nullptr);

                SDL_Rect message_rect; //create a rect
                message_rect.x = 0;  //controls the rect's x coordinate 
                message_rect.y = 0; // controls the rect's y coordinte
                message_rect.w = 200; // controls the width of the rect
                message_rect.h = 30; // controls the height of the rect
                viewport->updateFPSDisplay();
                SDL_RenderCopy(viewport->renderer, viewport->fps_display, NULL, &message_rect);

                SDL_RenderPresent(viewport->renderer);
                SDL_Delay(1000*(Uint32)(1./viewport->FPS));
            }
            std::cout << "Viewport stopped" << std::endl;
            viewport->isOnBool = false;
            viewport->closeWindow();
        }

        void start() {
            renderThread = std::thread(threadLoop, this);
        }

        void stop() {
            renderThread.join();
        }
};

