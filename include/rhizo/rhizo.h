#ifndef RHIZO_RHIZO_H
#define RHIZO_RHIZO_H
#include <stdint.h>
#if defined(_WIN32)
  #if defined(RHIZO_BUILDING_LIBRARY)
    #define RHIZO_API __declspec(dllexport)
  #else
    #define RHIZO_API __declspec(dllimport)
  #endif
#else
  #define RHIZO_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define RHIZO_ABI_VERSION 1u
typedef struct RhizoContext RhizoContext;
typedef struct RhizoBuffer RhizoBuffer;
typedef enum RhizoResult {
  RHIZO_SUCCESS = 0, RHIZO_ERROR_INVALID_ARGUMENT = 1, RHIZO_ERROR_OUT_OF_MEMORY = 2,
  RHIZO_ERROR_VULKAN = 3, RHIZO_ERROR_UNSUPPORTED_DEVICE = 4, RHIZO_ERROR_INTERNAL = 5
} RhizoResult;
typedef struct RhizoCreateInfo {
  uint32_t struct_size;
  uint32_t abi_version;
  const char* application_name;
  uint32_t requested_vulkan_api;
  uint8_t enable_validation;
  uint8_t reserved[3];
} RhizoCreateInfo;
typedef enum RhizoBufferUsage {
  RHIZO_BUFFER_VERTEX = 1u << 0,
  RHIZO_BUFFER_INDEX = 1u << 1,
  RHIZO_BUFFER_UNIFORM = 1u << 2,
  RHIZO_BUFFER_STORAGE = 1u << 3,
  RHIZO_BUFFER_INDIRECT = 1u << 4
} RhizoBufferUsage;
typedef struct RhizoBufferCreateInfo {
  uint32_t struct_size;
  uint32_t usage;
  uint64_t size;
} RhizoBufferCreateInfo;
typedef struct RhizoDeviceInfo {
  uint32_t struct_size;
  uint32_t vendor_id;
  uint32_t device_id;
  uint32_t api_version;
  uint32_t driver_version;
  uint32_t device_type;
  char device_name[256];
  char driver_name[256];
} RhizoDeviceInfo;
RHIZO_API uint32_t rhizo_get_abi_version(void);
RHIZO_API RhizoResult rhizo_create(const RhizoCreateInfo* info, RhizoContext** out_context);
RHIZO_API RhizoResult rhizo_get_device_info(const RhizoContext* context, RhizoDeviceInfo* out_info);
RHIZO_API const char* rhizo_get_last_error(const RhizoContext* context);
RHIZO_API RhizoResult rhizo_buffer_create(RhizoContext* context, const RhizoBufferCreateInfo* info, RhizoBuffer** out_buffer);
RHIZO_API RhizoResult rhizo_buffer_write(RhizoBuffer* buffer, uint64_t offset, const void* data, uint64_t size);
RHIZO_API uint64_t rhizo_buffer_get_size(const RhizoBuffer* buffer);
RHIZO_API void rhizo_buffer_destroy(RhizoBuffer* buffer);
RHIZO_API void rhizo_destroy(RhizoContext* context);
#ifdef __cplusplus
}
#endif
#endif
