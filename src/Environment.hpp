#pragma once

#include "Vector.hpp"
#include "Camera.hpp"
#include "Triangle.hpp"
#include "Line.hpp"
#include "Ray.hpp"

#include "shaders/Rasterize.hpp"
#include "shaders/RayTrace.hpp"
#include "shaders/Aggreg.hpp"
#include "shaders/Convolve.hpp"

#include "Tracing.hpp"

#include "Image.hpp"
#include "Obj.hpp"
#include "Mesh.hpp"
#include "utils/ProgressBar.hpp"

#include <stdlib.h>
#include <time.h>
#include "omp.h"
#include <chrono>

#include <cuda_runtime.h>

#define cudaErrorCheck(call){cudaAssert(call,__FILE__,__LINE__);}

enum Mode {
    SIMPLE_RENDER,
    RAYTRACING,
    BVH_RAYTRACING
};

class Environment {
    private:
        Camera* cam;
        Meshes meshes;
        uint samples = 5;

        uint samplesByThread = 2;

        Pixel backgroundColor = Pixel(0,0,0);
        Mode mode = BVH_RAYTRACING;
        Array<BVH> BVHs = Array<BVH>();

    public:
        Environment() {
            std::chrono::milliseconds ms = duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            );
            srand(ms.count());
        };
        Environment(Camera* cam0) : Environment() {
            cam = cam0;
        };
        ~Environment() {
            if (mode==BVH_RAYTRACING) {
                BVHs.cpu();
                BVHs.free();
            }
        };

        void addBackground(const Pixel& color) {
            backgroundColor = color;
            for(uint h = 0; h < cam->getHeight(); ++h) 
                for(uint w = 0; w < cam->getWidth(); ++w)
                    cam->setPixel(h*cam->getWidth()+w, color);
        }

        void compute_bvhs() {
            auto start = std::chrono::steady_clock::now();
            for (uint i=0; i<meshes.size(); i++) {
                BVHs.push_back(BVH(meshes[i]));
            }
            BVHs.cuda();
            auto end = std::chrono::steady_clock::now();
            std::chrono::duration<float> elapsed_seconds = end-start;
            std::cout << "BVHs on device:\t\t" << elapsed_seconds.count() << "s\n";
        }

        void setMode(const Mode m) {
            mode = m;
        }

        void addTriangle(Triangle& triangle) {
            meshes.push_back(Mesh(triangle));
        }

        void addSquare(Vector<float> v1, Vector<float> v2, Vector<float> v3, Vector<float> v4, Material mat) {
            Triangle triangle = Triangle(v1,mat);
            triangle.setvertex(1, v2);
            triangle.setvertex(2, v4);

            Triangle triangleBis = Triangle(v2,mat);
            triangleBis.setvertex(1, v3);
            triangleBis.setvertex(2, v4);

            Mesh mesh = Mesh(triangle);
            mesh.push_back(triangleBis);
            meshes.push_back(mesh);
        }

        void addSquare(Vector<float> v1, Vector<float> v2, Vector<float> v3, Vector<float> v4, Pixel color) {
            addSquare(v1, v2, v3, v4, Material(color));
        }

        void addObj(const std::string name, const Vector<float>& offset, const float scale, const Material mat) {
            std::cout << "Loading " << name.c_str() << std::endl;
            Obj obj = Obj(name);
            //obj.print();

            std::vector<Vector<float>> vertices = obj.getVertices();
            std::vector<Vector<float>> normal_vertices = obj.getNormalVertices();
            std::vector<std::vector<Vector<int>>> indexes = obj.getIndexes();

            float angle = 3.14159/2.0;
            float ux = 1;
            float uy = 0;
            float uz = 0;
            Matrix<float> P = Matrix<float>(ux*ux,ux*uy,ux*uz,ux*uy,uy*uy,uy*uz,ux*uz,uy*uz,uz*uz);
            Matrix<float> I = Matrix<float>(1.,MATRIX_EYE);
            Matrix<float> Q = Matrix<float>(0,-uz,uy,uz,0,-ux,-uy,ux,0);

            Matrix<float> R = P + (I-P)*std::cos(angle) + Q*std::sin(angle);

            /*
            The OBJ format can provide multiple vertices for one triangle. We have to convert it in triangles as follow.
            */
            Mesh mesh = Mesh();
            for (uint i=0;i<indexes.size();i++) {
                std::vector<Vector<int>> fi = indexes[i];

                for (uint v=2;v<fi.size();v++) {
                    Triangle triangle = Triangle(mat);
                    if (v==2) {
                        for (uint j = 0; j<3; j++) { 
                            triangle.setvertex(j, R*vertices[fi[j].getX()]*scale + offset );
                            triangle.setNormal(j, R*normal_vertices[fi[j].getZ()] );
                        }
                    } else {
                        triangle.setvertex(0, R*vertices[fi[v-3].getX()]*scale + offset );
                        triangle.setvertex(1, R*vertices[fi[v-1].getX()]*scale + offset );
                        triangle.setvertex(2, R*vertices[fi[v  ].getX()]*scale + offset );

                        triangle.setNormal(0, R*normal_vertices[fi[v-3].getZ()] );
                        triangle.setNormal(1, R*normal_vertices[fi[v-1].getZ()] );
                        triangle.setNormal(2, R*normal_vertices[fi[v  ].getZ()] );
                    }
                    mesh.push_back(triangle);
                    obj.nbTriangles += 1;
                }
            }
            meshes.push_back(mesh);
            std::cout << name.c_str() << " loaded with " << obj.nbTriangles << " triangles and " << obj.failedTriangles << " wrong ones." << std::endl;
        }        

        void render() {
            const uint H = cam->getHeight();
            const uint W = cam->getWidth();

            Array<BVH> BVHs = Array<BVH>();
            if (mode==BVH_RAYTRACING) {
                for (uint i=0; i<meshes.size(); i++) {
                    std::cout << "BVH " << i << std::endl;
                    BVHs.push_back(BVH(meshes[i]));
                }
                std::cout << "BVHs done" << std::endl;
            }

            //#pragma omp parallel for num_threads(omp_get_num_devices())
            for(uint h = 0; h < H; ++h) {
                //#pragma omp parallel for num_threads(omp_get_num_devices())
                for(uint w = 0; w < W; ++w) {
                    printProgress((h*W+(w+1))/(1.*H*W));

                    uint idx = h*W+w;

                    Pixel color;
                    Vector<float> colorVec;
                    
                    if (mode==SIMPLE_RENDER) {
                        Vector<float> direction = (cam->getVectFront()*cam->getFov()+cam->getPixelCoordOnCapt(w,h)).normalize();
                        Ray ray = Ray(cam->getPosition(),direction);

                        color = Tracing::simpleRayTraceHost(ray, meshes, backgroundColor);
                    }

                    else if (mode==RAYTRACING) {
                        Vector<float> vectTmp;
                        samples = 4; // has to be a perfect square
                        int samplesSqrt=(int)std::sqrt(samples);
                        
                        float dy=-(samplesSqrt-1)/2.;
                        do {
                            float dx=-(samplesSqrt-1)/2.;
                            do {
                                Vector<float> direction = (cam->getVectFront()*cam->getFov()+cam->getPixelCoordOnCapt(w+dx/(1.*samplesSqrt),h+dy/(1.*samplesSqrt))).normalize();
                                Ray ray = Ray(cam->getPosition(),direction);
                                
                                vectTmp = (Tracing::rayTraceHost(ray, meshes, idx)).toVector();

                                colorVec += vectTmp;
                                dx++;
                            } while (dx<(samplesSqrt-1)/2);
                            dy++;
                        } while (dy<(samplesSqrt-1)/2.);
                        colorVec/=(samples/2);
                        color=Pixel(colorVec);
                    }

                    else if (mode==BVH_RAYTRACING) {
                        Vector<float> vectTmp;
                        samples = 1; // has to be a perfect square
                        int samplesSqrt=(int)std::sqrt(samples);
                        
                        float dy=-(samplesSqrt-1)/2.;
                        do {
                            float dx=-(samplesSqrt-1)/2.;
                            do {
                                Vector<float> direction = (cam->getVectFront()*cam->getFov()+cam->getPixelCoordOnCapt(w+dx/(1.*samplesSqrt),h+dy/(1.*samplesSqrt))).normalize();
                                Ray ray = Ray(cam->getPosition(),direction);

                                vectTmp = (Tracing::rayTraceBVHHost(ray, BVHs, idx)).toVector();

                                colorVec += vectTmp;
                                dx++;
                            } while (dx<(samplesSqrt-1)/2);
                            dy++;
                        } while (dy<(samplesSqrt-1)/2.);
                        colorVec/=(samples/2);
                        color=Pixel(colorVec);
                    }
                    cam->setPixel(h*W+w, color);
                }
            }
            if (mode==BVH_RAYTRACING) {
                for (uint i=0; i<BVHs.size(); i++) {
                    BVHs[i].free();
                }
                BVHs.free();
            }
        }
        
        void renderCudaBVH() {
            auto start = std::chrono::steady_clock::now();
            int state  = rand() % 1000000000 + 1;
            //std::cout << state << std::endl;

            if (cam->is_raytrace_enable) {
                RayTraceShader raytrace = RayTraceShader({BVHs, *cam, samplesByThread}, state);
                compute_shader(raytrace);
                //ConvolutionShader denoise = ConvolutionShader({ {{1, 2, 1}, {2, 4, 2}, {1, 2, 1}}, *cam});
                //compute_shader(denoise);
            } else {
                RasterizeShader raster = RasterizeShader({BVHs, *cam}, state);
                compute_shader(raster);
            }

            //AggregShader shader2 = AggregShader({*cam});
            //compute_shader(shader2, state);

            auto end = std::chrono::steady_clock::now();
            std::chrono::duration<float> elapsed_seconds = end-start;
            cam->setCurrentFPS(1./(elapsed_seconds.count()));
        }
};