#include <iostream> 
#include "Vector.hpp"
#include "Camera.hpp"
#include "Viewport.hpp"
#include "Pixel.hpp"
#include "Environment.hpp"
#include "Triangle.hpp"
#include "Matrix.hpp"
#include "Line.hpp"

#include <cuda_runtime.h>

#include <omp.h>
#include <string>
#include <chrono>
#include <thread>

void objRender() {
	Vector<float> origine = Vector<float>(-3.,0.,1.5);
	Vector<float> front = Vector<float>(1,0,-0.2);
	Camera cam = Camera(origine,front,1280,720);
	Viewport viewport = Viewport(&cam);
	Environment env = Environment(cam);

	cam.move(-Vector<float>(5.0,0.,-1.5));

	Material light = Materials::LIGHT;

	env.addSquare(Vector(20.,20.,0.),Vector(-20.,20.,0.),Vector(-20.,-20.,0.),Vector(20.,-20.,0.), Colors::WHITE);

	light.setColor(Colors::RED);
	env.addSquare(Vector(0.,-2.,0.)*2,Vector(0.,-2.,2.)*2,Vector(2.,-2.,2.)*2,Vector(2.,-2.,0.)*2, light); // left panel 
	light.setColor(Colors::GREEN);
	env.addSquare(Vector(0.,2.,0.)*2,Vector(2.,2.,0.)*2,Vector(2.,2.,2.)*2,Vector(0.,2.,2.)*2, light); // right panel

	//env.addSquare(Vector(0.,0.,0.),Vector(0.,0.,2.),Vector(2.,2.,2.),Vector(2.,2.,0.), Material(Colors::WHITE, MaterialType::GLASS));
	//env.addSquare(Vector(0.,0.,0.),Vector(2.,-2.,0.),Vector(2.,-2.,2.),Vector(0.,0.,2.), Material(Colors::WHITE, MaterialType::GLASS));
	//env.addSquare(Vector(0.,0.,2.),Vector(2.,-2.,1.),Vector(2.,0.,2.),Vector(2.,2.,2.), Material(Colors::WHITE, MaterialType::GLASS));

	env.addObj("knight.obj",Vector<float>(0,0,0),0.5, Colors::WHITE);

	env.addBackground(Colors::BLACK);
	env.setMode(Mode::BVH_RAYTRACING);

	for (uint i = 0; i<10; i++) {
		env.renderCudaBVH();
		cam.move(Vector<float>(0., -0.5, 0.));
	}
	std::string path = "./render4/image";
	std::string format = ".png";
	path.append(std::to_string(0));
	path.append(format);
	cam.renderImage(path.c_str());
	viewport.stop();
}

void animObj() {
	Vector<float> origine = Vector<float>(-3.,0.,1.5);
	Vector<float> front = Vector<float>(1,0,-0.2);
	Camera cam = Camera(origine,front,1280,720);
	Viewport viewport = Viewport(&cam);

	cam.move(-Vector<float>(5.0,0.,-1.5));
	cam.cuda();

	Environment env = Environment(cam);
	
	Material light = Materials::LIGHT;

	env.addSquare(Vector(20.,20.,0.),Vector(-20.,20.,0.),Vector(-20.,-20.,0.),Vector(20.,-20.,0.), Colors::WHITE);

	light.setColor(Colors::GREEN);
	env.addSquare(Vector(0.,-2.,0.)*2,Vector(0.,-2.,2.)*2,Vector(2.,-2.,2.)*2,Vector(2.,-2.,0.)*2, light); // left panel 
	light.setColor(Colors::RED);
	env.addSquare(Vector(0.,2.,0.)*2,Vector(2.,2.,0.)*2,Vector(2.,2.,2.)*2,Vector(0.,2.,2.)*2, light); // right panel
	light.setColor(Colors::WHITE);

	env.addSquare(Vector(2.,1.,0.)*2,Vector(2.,-1.,0.)*2,Vector(2.,-1.,2.)*2,Vector(2.,1.,2.)*2, light); // right panel

	//env.addSquare(Vector(0.,0.,0.),Vector(0.,0.,2.),Vector(2.,2.,2.),Vector(2.,2.,0.), Material(Colors::WHITE, MaterialType::GLASS));
	//env.addSquare(Vector(0.,0.,0.),Vector(2.,-2.,0.),Vector(2.,-2.,2.),Vector(0.,0.,2.), Material(Colors::WHITE, MaterialType::GLASS));
	//env.addSquare(Vector(0.,0.,2.),Vector(2.,-2.,1.),Vector(2.,0.,2.),Vector(2.,2.,2.), Material(Colors::WHITE, MaterialType::GLASS));

	env.addObj("knight.obj",Vector<float>(0,0,0), 0.5, Material(Colors::WHITE, MaterialType::GLASS));

	//env.addBackground(Colors::BLACK);
	env.setMode(Mode::BVH_RAYTRACING);
	env.compute_bvhs();

	while (viewport.isOn()) {
		env.renderCudaBVH();
	}

	/*
	std::string path = "./render4/image";
	std::string format = ".png";
	path.append(std::to_string(0));
	path.append(format);
	cam.renderImage(path.c_str());*/
	viewport.stop();
	cam.cpu();
	cam.free();
}

int main() {
	static_assert(std::is_base_of<CudaReady, Pixel>::value == false);
	static_assert(std::is_base_of<CudaReady, Array<double>>::value == true);
	static_assert(std::is_base_of<CudaReady, BVH>::value == true);

	auto start = std::chrono::steady_clock::now();
	animObj();
	auto end = std::chrono::steady_clock::now();

	std::chrono::duration<float> elapsed_seconds = end-start;
	std::cout << "Render time:\t\t" << elapsed_seconds.count() << "s\n";

	return EXIT_SUCCESS; 
}