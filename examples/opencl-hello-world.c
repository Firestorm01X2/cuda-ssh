#include <CL/cl.h>
#include <stdio.h>

int main(void) {
    cl_uint platformCount;
    cl_platform_id platforms[10];

    // Получаем список платформ
    cl_int err = clGetPlatformIDs(10, platforms, &platformCount);
    if (err != CL_SUCCESS) {
        printf("Ошибка clGetPlatformIDs: %d\n", err);
        return 1;
    }
    printf("Найдено платформ: %u\n", platformCount);

    for (cl_uint i = 0; i < platformCount; i++) {
        char name[128];
        clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, sizeof(name), name, NULL);
        printf("  Платформа %u: %s\n", i, name);

        cl_uint deviceCount;
        cl_device_id devices[10];
        clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 10, devices, &deviceCount);
        printf("    Устройств: %u\n", deviceCount);

        for (cl_uint j = 0; j < deviceCount; j++) {
            char devname[128];
            clGetDeviceInfo(devices[j], CL_DEVICE_NAME, sizeof(devname), devname, NULL);
            printf("      Устройство %u: %s\n", j, devname);
        }
    }

    return 0;
}

// gcc opencl-hello-world.c -o opencl-hello-world -lOpenCL
// ./opencl-hello-world
