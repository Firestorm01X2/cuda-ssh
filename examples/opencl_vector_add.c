#define CL_TARGET_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

static const char *kernelSource =
    "__kernel void vadd(__global const float* a, __global const float* b, __global float* c) {\n"
    "  int id = get_global_id(0);\n"
    "  c[id] = a[id] + b[id];\n"
    "}\n";

static void abortWithMessage(const char *message, cl_int status) {
    fprintf(stderr, "OpenCL error: %s (code %d)\n", message, status);
    exit(EXIT_FAILURE);
}

static void printDeviceSummary(cl_platform_id platform, cl_device_id device) {
    char buf[512];
    size_t len = 0;

    if (clGetPlatformInfo(platform, CL_PLATFORM_NAME, sizeof(buf), buf, &len) == CL_SUCCESS) {
        printf("Platform: %s\n", buf);
    }
    if (clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, sizeof(buf), buf, &len) == CL_SUCCESS) {
        printf("  Vendor: %s\n", buf);
    }

    cl_device_type dtype = 0;
    if (clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(dtype), &dtype, NULL) == CL_SUCCESS) {
        const char *t = (dtype & CL_DEVICE_TYPE_GPU) ? "GPU" : (dtype & CL_DEVICE_TYPE_CPU) ? "CPU" : "Other";
        printf("Device type: %s\n", t);
    }

    if (clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(buf), buf, &len) == CL_SUCCESS) {
        printf("Device name: %s\n", buf);
    }
    if (clGetDeviceInfo(device, CL_DEVICE_VENDOR, sizeof(buf), buf, &len) == CL_SUCCESS) {
        printf("Device vendor: %s\n", buf);
    }
    if (clGetDeviceInfo(device, CL_DRIVER_VERSION, sizeof(buf), buf, &len) == CL_SUCCESS) {
        printf("Driver version: %s\n", buf);
    }
    if (clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(buf), buf, &len) == CL_SUCCESS) {
        printf("Device OpenCL version: %s\n", buf);
    }
}

static int stringContainsCaseInsensitive(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return 0;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; ++p) {
        size_t i = 0;
        while (i < nlen && p[i] && (tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))) {
            ++i;
        }
        if (i == nlen) return 1;
    }
    return 0;
}

static cl_device_id chooseDevicePreferGPU(cl_platform_id *outPlatform) {
    cl_int status;
    cl_uint numPlatforms = 0;
    status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS || numPlatforms == 0) {
        abortWithMessage("No OpenCL platforms found", status);
    }
    cl_platform_id *platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * numPlatforms);
    status = clGetPlatformIDs(numPlatforms, platforms, NULL);
    if (status != CL_SUCCESS) abortWithMessage("clGetPlatformIDs", status);

    const char *preferVendor = getenv("OPENCL_PREFER_VENDOR");
    const int allowCPU = getenv("OPENCL_ALLOW_CPU") != NULL;

    cl_device_id selected = NULL;
    cl_platform_id selectedPlatform = NULL;

    // First pass: try to find a GPU, optionally preferring a vendor
    for (cl_uint pi = 0; pi < numPlatforms && !selected; ++pi) {
        cl_platform_id p = platforms[pi];

        // If vendor hint is set, skip non-matching platforms on the first pass
        if (preferVendor && *preferVendor) {
            char vendorBuf[256];
            size_t vlen = 0;
            if (clGetPlatformInfo(p, CL_PLATFORM_VENDOR, sizeof(vendorBuf), vendorBuf, &vlen) == CL_SUCCESS) {
                if (!stringContainsCaseInsensitive(vendorBuf, preferVendor)) {
                    continue;
                }
            }
        }

        cl_uint ndev = 0;
        status = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, NULL, &ndev);
        if (status != CL_SUCCESS || ndev == 0) continue;
        cl_device_id *devs = (cl_device_id *)malloc(sizeof(cl_device_id) * ndev);
        status = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, ndev, devs, NULL);
        if (status == CL_SUCCESS && ndev > 0) {
            selected = devs[0];
            selectedPlatform = p;
        }
        free(devs);
    }

    // Second pass: if vendor hint was set but no GPU found, try any vendor GPU
    if (!selected && preferVendor && *preferVendor) {
        for (cl_uint pi = 0; pi < numPlatforms && !selected; ++pi) {
            cl_platform_id p = platforms[pi];
            cl_uint ndev = 0;
            status = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, NULL, &ndev);
            if (status != CL_SUCCESS || ndev == 0) continue;
            cl_device_id *devs = (cl_device_id *)malloc(sizeof(cl_device_id) * ndev);
            status = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, ndev, devs, NULL);
            if (status == CL_SUCCESS && ndev > 0) {
                selected = devs[0];
                selectedPlatform = p;
            }
            free(devs);
        }
    }

    // Optional CPU fallback
    if (!selected && allowCPU) {
        for (cl_uint pi = 0; pi < numPlatforms && !selected; ++pi) {
            cl_platform_id p = platforms[pi];
            cl_uint ndev = 0;
            status = clGetDeviceIDs(p, CL_DEVICE_TYPE_CPU, 0, NULL, &ndev);
            if (status != CL_SUCCESS || ndev == 0) continue;
            cl_device_id *devs = (cl_device_id *)malloc(sizeof(cl_device_id) * ndev);
            status = clGetDeviceIDs(p, CL_DEVICE_TYPE_CPU, ndev, devs, NULL);
            if (status == CL_SUCCESS && ndev > 0) {
                selected = devs[0];
                selectedPlatform = p;
            }
            free(devs);
        }
    }

    free(platforms);
    if (!selected) {
        abortWithMessage("No suitable OpenCL device found (GPU required; set OPENCL_ALLOW_CPU=1 to allow CPU)", CL_DEVICE_NOT_AVAILABLE);
    }
    if (outPlatform) *outPlatform = selectedPlatform;
    return selected;
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

    // Pick device: prefer a real GPU across all platforms; vendor can be hinted using OPENCL_PREFER_VENDOR
    cl_platform_id chosenPlatform = NULL;
    cl_device_id device = chooseDevicePreferGPU(&chosenPlatform);
    printDeviceSummary(chosenPlatform, device);

    // Create context and command queue
#if defined(CL_VERSION_2_0)
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if (status != CL_SUCCESS || !context) abortWithMessage("clCreateContext", status);
    const cl_queue_properties qprops[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, qprops, &status);
#else
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &status);
    if (status != CL_SUCCESS || !context) abortWithMessage("clCreateContext", status);
    cl_command_queue queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &status);
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
    cl_event kernelEvent = NULL;
    status = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalWorkSize, NULL, 0, NULL, &kernelEvent);
    if (status != CL_SUCCESS) abortWithMessage("clEnqueueNDRangeKernel", status);

    // Make sure kernel finished (for timing)
    status = clWaitForEvents(1, &kernelEvent);
    if (status != CL_SUCCESS) abortWithMessage("clWaitForEvents", status);

    // Read back result
    status = clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, bytes, hostC, 0, NULL, NULL);
    if (status != CL_SUCCESS) abortWithMessage("clEnqueueReadBuffer", status);

    // Timing info
    cl_ulong tStart = 0, tEnd = 0;
    if (kernelEvent) {
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_START, sizeof(tStart), &tStart, NULL);
        clGetEventProfilingInfo(kernelEvent, CL_PROFILING_COMMAND_END, sizeof(tEnd), &tEnd, NULL);
        if (tEnd > tStart) {
            double us = (double)(tEnd - tStart) / 1000.0;
            printf("Kernel time: %.3f us (%.3f ms)\n", us, us / 1000.0);
        }
        clReleaseEvent(kernelEvent);
    }

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
    // platforms array is managed inside chooser

    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Build & run:
// gcc -O2 -std=c11 -o opencl_vector_add opencl_vector_add.c -lOpenCL
// ./opencl_vector_add
// To allow CPU fallback (not recommended when validating GPU path):
// OPENCL_ALLOW_CPU=1 ./opencl_vector_add


