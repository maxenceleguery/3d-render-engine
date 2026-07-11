#pragma once

#include "Vector.hpp"
#include "Pixel.hpp"
#include "Matrix.hpp"
#include "Ray.hpp"
#include "utils/Array.hpp"
#include "utils/CudaReady.hpp"
#include <vector>
#include <iostream>
#include <fstream>
#include <png.h>

#include <cuda_runtime.h>

#define ROT_RIGHT 2000
#define ROT_FRONT 2001
#define ROT_UP 2002

struct CoordsPair {
    uint width;
    uint height;
};

class Camera : public SceneObject {
    private:
        Vector<float> pos_tmp;
        Vector<float> front_tmp = Vector<float>(1, 0, 0);

        uint width;
        uint height;
        float capteurWidth;
        float capteurHeight;
        float fov = 0.01;
        float gamma = 2.0;

        float current_fps = 0.;
        uint num_images_rendered = 0;

        Array<Pixel> pixels;
        //Array<Vector<float>> errors;

    public:
        bool is_raytrace_enable = false;
        uint threadsByRay = 1;

        __host__ Camera() {};
        __host__ Camera(Vector<float> pos, uint width0, uint height0) : width(width0), height(height0) {
            pos_tmp = pos;

            capteurWidth = (0.005*width0)/(1.*height0);
            capteurHeight = 0.005;
        };
        __host__ Camera(Vector<float> pos, Vector<float> front, uint width0, uint height0) : width(width0), height(height0) {
            pos_tmp = pos;
            front_tmp = front;

            capteurWidth = (0.005*width0)/(1.*height0);
            capteurHeight = 0.005;
        };

        __host__ void init() {
            pixels = Array<Pixel>(width*height*threadsByRay);
            //errors = Array<Vector<float>>(width*height*threadsByRay);
            setDefaultsTransforms(pos_tmp, Vector<float>(1, 1, 1), Vector<float>());
            setDefaultsOrientations(front_tmp.normalize(), Vector<float>(0,0,1).crossProduct(front_tmp).normalize(), Vector<float>(0,0,1));
        }

        __host__ void cuda() override {
            pixels.cuda();
            //errors.cuda();
            SceneObject::cuda();
        }

        __host__ void cpu() override {
            pixels.cpu();
            //errors.cpu();
            SceneObject::cpu();
        }

        __host__ void free() override {
            pixels.free();
            //errors.free();
            SceneObject::free();
        }

        __host__ void reset_progressive_rendering() {
            num_images_rendered = 0;
        }

        __host__ void toggleRaytracing() {
            is_raytrace_enable = !is_raytrace_enable;
            reset_progressive_rendering();
        }

        __host__ __device__ uint getWidth() const {
            return width;
        }

        __host__ __device__ uint getHeight() const {
            return height;
        }

        __host__ __device__ float getGamma() const {
            return gamma;
        }

        __host__ __device__ void setGamma(const float g) {
            gamma = g;
        }

        __host__ float getCurrentFPS() const {
            return current_fps;
        }

        __host__ void setCurrentFPS(const float fps) {
            current_fps = fps;
            num_images_rendered++;
        }

        __host__ __device__ Vector<float> getPixelCoordOnCapt(const float w, const float h) const {
            const float W = (1.f*w - width/2.f)*(1.f*capteurWidth/width);
            const float H = (-1.f*h + height/2.f)*(1.f*capteurHeight/height);
            return orientations[2]*H + -orientations[1]*W;
        }

        __host__ __device__ Ray generate_ray(const uint w, const uint h) const {
            const Vector<float> pos = getPixelCoordOnCapt(w,h);
            return Ray(transforms[0], orientations[0]*fov+pos);
        }

        __host__ __device__ Pixel getPixel(const uint index) const {
            return pixels[index];
        }

        __host__ __device__ void setPixel(const uint index, const Pixel& color) {
            pixels[index] = color;
        }

        __host__ __device__ void updatePixel(const uint index, const Pixel& color) {
            /*
            const Vector<float> y = color.toVector(); //- errors[index];
            const Vector<float> t = pixels[index].toVector()*num_images_rendered + y;
            //errors[index] = (t - pixels[index].toVector()*num_images_rendered) - y;
            pixels[index] = Pixel(t/(num_images_rendered+1));

            //pixels[index] = Pixel((pixels[index].toVector()*num_images_rendered + color.toVector()) / (num_images_rendered+1));
            //pixels[index] = Pixel( (pixels[index].toVector() + color.toVector()) / 2 );
            */

            const float weight = 1.f / (num_images_rendered + 1);
            //pixels[index] = Pixel( (pixels[index].toVector() * (1 - weight) + color.toVector() * weight).clamp(0.f, 1.f) );
            pixels[index] = Pixel( (pixels[index].toVector() + (color.toVector() - pixels[index].toVector()) * weight).clamp(0.f, 1.f) );
        }

        __host__ __device__ CoordsPair indexToCoord(const uint index) const {
            const uint idx = index%(width*height);
            return {idx%width, idx/width};
        }

        __host__ __device__ uint coordToIndex(const uint w, const uint h) const {
            return h*width+w;
        }

        __host__ __device__ Vector<float> getPosition() const {
            return transforms[0];
        }

        __host__ __device__ void setPosition(const Vector<float>& pos) {
            transforms[0]=pos;
        }

        __host__ __device__ float getFov() const {
            return fov;
        }

        __host__ void write_png_file(const char* filename, uint8_t* image_data) {
            FILE *fp = fopen(filename, "wb");
            png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
            png_infop info_ptr = png_create_info_struct(png_ptr);

            png_init_io(png_ptr, fp);

            png_set_IHDR(png_ptr, info_ptr, width, height,
                        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

            png_bytep row_pointers[height];
            for(uint y = 0; y < height; y++)
                row_pointers[y] = &image_data[y * width * 3];

            png_set_rows(png_ptr, info_ptr, row_pointers);
            png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

            fclose(fp);
        }

        __host__ void renderImage(const char* filename) {
            uint8_t* image_data = new uint8_t[width * height * 3];
            
            for(uint i = 0; i < pixels.size(); ++i) {
                uint8_t r = pixels[i].getR();
                uint8_t g = pixels[i].getG();
                uint8_t b = pixels[i].getB();

                image_data[i * 3] = r;
                image_data[i * 3 + 1] = g;
                image_data[i * 3 + 2] = b;
            }

            write_png_file(filename, image_data);

            delete[] image_data;
        }

        
};