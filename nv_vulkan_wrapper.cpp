#include <vulkan.h>
#include <dlfcn.h>

#include <iostream>

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* pSupportedVersion);


#ifndef NV_DRIVER_PATH
#define NV_DRIVER_PATH "/usr/lib/x86_64-linux-gnu/nvidia/current/libGL.so.1"
#endif

class ScopedEnvOverride {
  std::string old;
  std::string name;
public:
  ScopedEnvOverride(std::string name, std::string value): name(name){
    char *prev = getenv(name.c_str());
    std::string old{prev};
    setenv(name.c_str(), ":8", 1);
  }
  ~ScopedEnvOverride(){
    setenv(name.c_str(),old.c_str(), 1);
  }
};

class StaticInitialize {
  void *nvDriver;
  void *glLibGL;
public:
  VKAPI_ATTR PFN_vkVoidFunction (*instanceProcAddr) (VkInstance instance,
                                               const char* pName);
  VKAPI_ATTR PFN_vkVoidFunction (*phyProcAddr) (VkInstance instance,
                                               const char* pName);
  VKAPI_ATTR VkResult VKAPI_CALL (*negotiateVersion)(uint32_t* pSupportedVersion);

  VKAPI_ATTR PFN_vkCreateInstance createInstance;
public:
  StaticInitialize(){
    std::cout << "Nvidia wrapper: loading\n";
    // Load libGL from LD_LIBRARY_PATH before loading the NV-driver (unluckily also named libGL
    // This ensures that ld.so will find this libGL before the Nvidia one, when
    // again asked to load libGL.
    glLibGL = dlopen("libGL.so.1", RTLD_GLOBAL | RTLD_NOW);

    {
      ScopedEnvOverride dpy ("DISPLAY", ":8");
      nvDriver = dlopen(NV_DRIVER_PATH, RTLD_LOCAL | RTLD_LAZY);
    }
    if(!nvDriver) {
      std::cerr << "PrimusVK: ERROR! Nvidia driver could not be loaded from '" NV_DRIVER_PATH "'.\n";
      return;
    }
    std::cout << "Nvidia wrapper: dlopen done\n";
    typedef void* (*dlsym_fn)(void *, const char*);
    static dlsym_fn real_dlsym = (dlsym_fn) dlsym(dlopen("libdl.so.2", RTLD_LAZY), "dlsym");
    instanceProcAddr = (decltype(instanceProcAddr)) real_dlsym(nvDriver, "vk_icdGetInstanceProcAddr");
    phyProcAddr = (decltype(phyProcAddr)) real_dlsym(nvDriver, "vk_icdGetPhysicalDeviceProcAddr");
    negotiateVersion = (decltype(negotiateVersion)) real_dlsym(nvDriver, "vk_icdNegotiateLoaderICDInterfaceVersion");
    {
      ScopedEnvOverride dpy ("DISPLAY", ":8");
      createInstance = (PFN_vkCreateInstance) instanceProcAddr(NULL, "vkCreateInstance");
    }
    std::cout << "Nvidia wrapper: dlsym done\n";
  }
  ~StaticInitialize(){
    if(nvDriver)
      dlclose(nvDriver);
    dlclose(glLibGL);
  }
  bool IsInited(){
    return nvDriver != nullptr;
  }
};
typedef VkResult (VKAPI_PTR *PFN_vkCreateInstance)(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);


StaticInitialize init;

extern "C" VKAPI_ATTR VkResult VKAPI_CALL myVkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance){
  std::cout << "entry vkCreateInstance Hook\n";
  auto result = init.createInstance(pCreateInfo, pAllocator, pInstance);
  std::cout << "exit vkCreateInstance Hook: "<< result << "\n";
  return result;
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(
                                               VkInstance instance,
                                               const char* pName){
  if (!init.IsInited()) return nullptr;
  std::cout << "Nvidia wrapper: GIPA for " << pName << "\n";
  auto res = PFN_vkVoidFunction{};
  std::string name(pName);
  if(name == "vkCreateInstance"){
    std::cout << "Injecting filter\n";
    res = (PFN_vkVoidFunction) &myVkCreateInstance;
  }else{
    res = init.instanceProcAddr(instance, pName);
  }
  std::cout << "Nvidia wrapper: GIPA for " << pName << " done\n";
  return res;
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction vk_icdGetPhysicalDeviceProcAddr(VkInstance instance,
						    const char* pName){
  if (!init.IsInited()) return nullptr;
  std::cout << "Nvidia wrapper: GPDPA\n";
  auto res = init.phyProcAddr(instance, pName);
  std::cout << "Nvidia wrapper: GPDPA done\n";
  return res;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* pSupportedVersion){
  if (!init.IsInited()) {
    return VK_ERROR_INCOMPATIBLE_DRIVER;
  }
  std::cout << "Nvidia wrapper: negotiate\n";
  ScopedEnvOverride dpy ("DISPLAY", ":8");
  auto res = init.negotiateVersion(pSupportedVersion);
  std::cout << "Nvidia wrapper: negotiate done\n";
  return res;
}
