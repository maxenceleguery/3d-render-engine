#pragma once

#include <cuda_runtime.h>

#define cudaErrorCheck(call){cudaAssert(call,__FILE__,__LINE__);}

template<typename T>
class Array {
    private:
        T* data;
        T* data_cpu;
        T* data_gpu = nullptr;
        uint data_size;
        uint spaceUsed = 0;
        
    public:
        __host__ __device__ Array() : data(nullptr), data_cpu(nullptr), data_size(0) {};

        __host__ Array(const uint data_size) : data_size(data_size) {
            data_cpu = new T[data_size];
            data = data_cpu;
        };

        __host__ Array(const T& tri) : Array(1) {
            push_back(tri);
        };

        __host__ uint push_back(const T item) {
            if (spaceUsed == data_size) {
                data_size++;
                T* tri_tmp = new T[data_size];
                for (uint i = 0; i < spaceUsed; i++) {
                    tri_tmp[i] = data[i];
                }
                if (data != nullptr)
                    delete[] data;
                data = tri_tmp;
            }
            data[spaceUsed++] = item;
            data_cpu = data;
            return spaceUsed-1;
        }

        __host__ __device__ uint size() const {
            return spaceUsed;
        }

        template<typename I>
        __host__ __device__ T operator[](const I i) const {
            return data[i];
        }

        template<typename I>
        __host__ __device__ T& operator[](const I i) {
            return data[i];
        }

        __host__ void cuda() {
            if (data_gpu == nullptr) {
                //std::cout << "Allocating : " << data_size*sizeof(T) << " bytes" << std::endl;
                cudaErrorCheck(cudaMalloc(&data_gpu, data_size*sizeof(T)));
                cudaErrorCheck(cudaMemcpy(data_gpu, data_cpu, data_size*sizeof(T), cudaMemcpyHostToDevice));
            }
            data = data_gpu;
        }

        __host__ void cpu() {
            if (data_gpu != nullptr) {
                cudaErrorCheck(cudaMemcpy(data_cpu, data_gpu, data_size*sizeof(T), cudaMemcpyDeviceToHost));
                cudaErrorCheck(cudaFree(data_gpu));
                data_gpu = nullptr;
            }
            data = data_cpu;
        }

        __host__ void free() {
            if (data != nullptr) {
                delete[] data;
                data = nullptr;
                data_cpu = nullptr;
            }
            if (data_gpu != nullptr) {
                cudaErrorCheck(cudaFree(data_gpu));
                data_gpu = nullptr;
            }
        }
};
