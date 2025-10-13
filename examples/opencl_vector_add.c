#define CL_TARGET_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *kernelSource =
    "__kernel void vadd(__global const float* a, __global const float* b, __global float* c) {\n"
    "  int id = get_global_id(0);\n"
    "  c[id] = a[id] + b[id];\n"
    "}\n";

static void abortWithMessage(const char *message, cl_int status) {
    fprintf(stderr, "OpenCL error: %s (code %d)\n", message, status);
    exit(EXIT_FAILURE);
}

int main(void) {
    const size_t numElements = 1 << 20; // 1M elements
    const size_t bytes = numElements * sizeof(float);

    float *hostA = (float *)malloc(bytes);
    float *hostB = (float *)malloc(bytes);
    float *hostC = (float *)malloc(bytes);
    if (!hostA || !hostB || !hostC) {
        fprintf(stderr, "Failed to allocate host buffers\n");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < numElements; ++i) {
        hostA[i] = (float)i * 0.5f;
        hostB[i] = (float)i * 2.0f;
    }

    cl_int status;

    // Query platforms
    cl_uint numPlatforms = 0;
    status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS || numPlatforms == 0) {
        abortWithMessage("No OpenCL platforms found", status);
    }
    cl_platform_id *platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * numPlatforms);
    status = clGetPlatformIDs(numPlatforms, platforms, NULL);
    if (status != CL_SUCCESS) abortWithMessage("clGetPlatformIDs", status);

    cl_platform_id chosenPlatform = platforms[0];

    // Try to get a GPU device first, otherwise fall back to any device
    cl_device_id device = NULL;
    cl_uint numDevices = 0;
    status = clGetDeviceIDs(chosenPlatform, CL_DEVICE_TYPE_GPU, 1, &device, &numDevices);
    if (status != CL_SUCCESS || numDevices == 0) {
        status = clGetDeviceIDs(chosenPlatform, CL_DEVICE_TYPE_DEFAULT, 1, &device, &numDevices);
    }
    if (status != CL_SUCCESS || numDevices == 0) abortWithMessage("No OpenCL devices found", status);

    // Create context and command queue
#if defined(CL_VERSION_2_0)
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if (status != CL_SUCCESS || !context) abortWithMessage("clCreateContext", status);
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, NULL, &status);
#else
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if (status != CL_SUCCESS || !context) abortWithMessage("clCreateContext", status);
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &status);
#endif
    if (status != CL_SUCCESS || !queue) abortWithMessage("create command queue", status);

    // Create buffers
    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, hostA, &status);
    if (status != CL_SUCCESS) abortWithMessage("clCreateBuffer A", status);
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, hostB, &status);
    if (status != CL_SUCCESS) abortWithMessage("clCreateBuffer B", status);
    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, &status);
    if (status != CL_SUCCESS) abortWithMessage("clCreateBuffer C", status);

    // Build program
    const char *src = kernelSource;
    const size_t srcLen = strlen(kernelSource);
    cl_program program = clCreateProgramWithSource(context, 1, &src, &srcLen, &status);
    if (status != CL_SUCCESS) abortWithMessage("clCreateProgramWithSource", status);
    status = clBuildProgram(program, 1, &device, "", NULL, NULL);
    if (status != CL_SUCCESS) {
        // Print build log for easier debugging
        size_t logSize = 0;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        char *log = (char *)malloc(logSize + 1);
        if (log) {
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log, NULL);
            log[logSize] = '\0';
            fprintf(stderr, "Build log:\n%s\n", log);
            free(log);
        }
        abortWithMessage("clBuildProgram", status);
    }

    cl_kernel kernel = clCreateKernel(program, "vadd", &status);
    if (status != CL_SUCCESS || !kernel) abortWithMessage("clCreateKernel", status);

    // Set args
    status  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufA);
    status |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufB);
    status |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufC);
    if (status != CL_SUCCESS) abortWithMessage("clSetKernelArg", status);

    // Enqueue kernel
    const size_t globalWorkSize = numElements;
    status = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalWorkSize, NULL, 0, NULL, NULL);
    if (status != CL_SUCCESS) abortWithMessage("clEnqueueNDRangeKernel", status);

    // Read back result
    status = clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, bytes, hostC, 0, NULL, NULL);
    if (status != CL_SUCCESS) abortWithMessage("clEnqueueReadBuffer", status);

    // Validate
    int errors = 0;
    for (size_t i = 0; i < numElements; ++i) {
        float expected = hostA[i] + hostB[i];
        if (hostC[i] != expected) {
            errors = 1;
            fprintf(stderr, "Mismatch at %zu: got %f, expected %f\n", i, hostC[i], expected);
            break;
        }
    }

    if (!errors) {
        printf("OpenCL vector add succeeded for %zu elements.\n", numElements);
    }

    // Cleanup
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(hostA);
    free(hostB);
    free(hostC);
    free(platforms);

    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

//gcc -O2 -std=c11 -o opencl_vector_add opencl_vector_add.c -lOpenCL
//./opencl_vector_add


