#pragma once

#include <type_traits>
#include "CudaReady.hpp"

#ifdef __CUDA_ARCH__
#define DATA data_gpu
#else
#define DATA data_cpu
#endif

#define cudaErrorCheck(call){cudaAssert(call,__FILE__,__LINE__);}

static std::string human_rep(const int num_bytes) {
    if (num_bytes < 1000) return std::to_string(num_bytes) + " B (" + std::to_string(num_bytes) + " B)";
    if (num_bytes < 1000 * 1000) return std::to_string((float)num_bytes/1000).substr(0, 5) + " KB (" + std::to_string((float)num_bytes/1024).substr(0, 5) + " KiB)";
    if (num_bytes < 1000 * 1000 * 1000) return std::to_string((float)num_bytes/(1000*1000)).substr(0, 5) + " MB (" + std::to_string((float)num_bytes/(1024*1024)).substr(0, 5) + " MiB)";
    return std::to_string((float)num_bytes/(1000*1000*1000)).substr(0, 5) + " GB (" + std::to_string((float)num_bytes/(1024*1024*1024)).substr(0, 5) + " GiB)";;
}

template<typename T>
class Array : public CudaReady {
    private:
        T* data_cpu;
        T* data_gpu = nullptr;
        uint data_size;

    protected:
        uint spaceUsed = 0;
        
    public:
        __host__ __device__ Array() : data_cpu(nullptr), data_size(0) {};

        __host__ Array(const uint data_size) : data_size(data_size) {
            data_cpu = new T[data_size];
        };

        __host__ Array(const T& item) : Array(1) {
            push_back(item);
        };

        __host__ uint push_back(const T item) {
            if (spaceUsed == data_size) {
                data_size++;
                T* tri_tmp = new T[data_size];
                for (uint i = 0; i < spaceUsed; i++) {
                    tri_tmp[i] = data_cpu[i];
                }
                if (data_cpu != nullptr)
                    delete[] data_cpu;
                data_cpu = tri_tmp;
            }
            data_cpu[spaceUsed++] = item;
            return spaceUsed-1;
        }

        __host__ __device__ uint size() const {
            return spaceUsed;
        }

        __host__ void clear() {
            spaceUsed = 0;
        }

        template<typename I>
        __host__ __device__ T operator[](const I i) const {
            if constexpr (std::is_signed_v<I>) {
                if (i < 0)
                    return DATA[(int)spaceUsed + i];
            }
            return DATA[i];
        }

        template<typename I>
        __host__ __device__ T& operator[](const I i) {            
            if constexpr (std::is_signed_v<I>) {
                if (i < 0)
                    return DATA[(int)spaceUsed + i];
            }
            return DATA[i];
        }
        
        __host__ void cuda() override {
            if (data_size == 0) return;

            if constexpr (std::is_base_of<CudaReady, T>::value) {
                // Maybe size() must be replace by data_size to handle reserved but no set memory
                for (uint i=0; i<size(); i++) {
                    data_cpu[i].cuda();
                }
            }
            if (data_gpu == nullptr) {
                std::cout << "Allocating : " << human_rep(data_size*sizeof(T));
                //allocated_cuda_memory += data_size*sizeof(T);
                cudaErrorCheck(cudaMalloc(&data_gpu, data_size*sizeof(T)));
                std::cout << " Done" << std::endl;
            }
            //std::cout << "Copying to device.";
            cudaErrorCheck(cudaMemcpy(data_gpu, data_cpu, data_size*sizeof(T), cudaMemcpyHostToDevice));
            //std::cout << " Done" << std::endl;
        }

        __host__ void cpu() override {
            if (data_size == 0) return;

            if (data_gpu != nullptr) {
                cudaErrorCheck(cudaMemcpy(data_cpu, data_gpu, data_size*sizeof(T), cudaMemcpyDeviceToHost));
            }
            if constexpr (std::is_base_of<CudaReady, T>::value) {
                for (uint i=0; i<size(); i++) {
                    data_cpu[i].cpu();
                }
            }
        }

        __host__ void free() override {
            if (data_size == 0) return;
            
            if constexpr (std::is_base_of<CudaReady, T>::value) {
                for (uint i=0; i<size(); i++) {
                    data_cpu[i].free();
                }
            }
            if (data_cpu != nullptr) {
                delete[] data_cpu;
                data_cpu = nullptr;
            }
            if (data_gpu != nullptr) {
                cudaErrorCheck(cudaFree(data_gpu));
                //allocated_cuda_memory -= data_size*sizeof(T);
                data_gpu = nullptr;
            }
        }
};
