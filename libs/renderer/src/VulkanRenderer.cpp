#include "renderer/VulkanRenderer.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DirectX;

namespace renderer {

namespace {

void vkCheck(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer: " + std::string(what) +
                                  " failed (VkResult=" + std::to_string(static_cast<int>(result)) +
                                  ")");
    }
}

#ifdef _DEBUG
constexpr bool kEnableValidation = true;
#else
constexpr bool kEnableValidation = false;
#endif

bool validationLayerAvailable() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
    for (const auto& layer : layers) {
        if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) return true;
    }
    return false;
}

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupport querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    support.formats.resize(formatCount);
    if (formatCount > 0)
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    support.presentModes.resize(presentModeCount);
    if (presentModeCount > 0)
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                                   support.presentModes.data());
    return support;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    // FIFO is the only mode every Vulkan implementation is required to
    // support - fine for a debug visualizer that has never targeted a
    // specific frame pacing (the D3D11 pass used Present(0,0), i.e. also
    // vsync'd/FIFO-equivalent behavior).
    for (auto mode : modes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) return mode;
    }
    return modes[0];
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != 0xFFFFFFFF) {
        return capabilities.currentExtent;
    }
    VkExtent2D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                               capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                                capabilities.maxImageExtent.height);
    return extent;
}

bool deviceExtensionsSupported(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string> required = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (const auto& ext : available) required.erase(ext.extensionName);
    return required.empty();
}

// Returns {graphicsFamily, presentFamily} if a usable pair is found, or
// {UINT32_MAX, UINT32_MAX} otherwise. Deliberately prefers a single family
// that does both (the common case on desktop GPUs) but falls back to two
// distinct families rather than rejecting the device outright.
std::pair<uint32_t, uint32_t> findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (graphics == UINT32_MAX) graphics = i;
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                graphics = i;
                present = i;
                break;
            }
        }
    }
    if (present == UINT32_MAX) {
        for (uint32_t i = 0; i < count; ++i) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                present = i;
                break;
            }
        }
    }
    return {graphics, present};
}

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice) {
    const std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    throw std::runtime_error("VulkanRenderer: no supported depth format found");
}

// Unit box from -1 to +1 on each axis - drawWireBox()'s `halfExtents` scales
// this directly, so a halfExtents of (1,1,1) produces a 2x2x2 box. Ported
// unchanged from the D3D11 pass' kBoxVertices/kBoxIndices.
const std::array<XMFLOAT3, 8> kBoxVertices = {{
    {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},  {1.0f, 1.0f, 1.0f},  {-1.0f, 1.0f, 1.0f},
}};
const std::array<uint16_t, 24> kBoxIndices = {{
    0, 1, 1, 2, 2, 3, 3, 0, // back face
    4, 5, 5, 6, 6, 7, 7, 4, // front face
    0, 4, 1, 5, 2, 6, 3, 7, // connecting edges
}};

constexpr int kCircleSegments = 32;

std::vector<XMFLOAT3> buildCircleVertices() {
    std::vector<XMFLOAT3> verts;
    verts.reserve(static_cast<size_t>(kCircleSegments) * 2);
    for (int i = 0; i < kCircleSegments; ++i) {
        float angle0 = (static_cast<float>(i) / kCircleSegments) * XM_2PI;
        float angle1 = (static_cast<float>(i + 1) / kCircleSegments) * XM_2PI;
        verts.push_back({cosf(angle0), 0.0f, sinf(angle0)});
        verts.push_back({cosf(angle1), 0.0f, sinf(angle1)});
    }
    return verts;
}

} // namespace

VulkanRenderer::VulkanRenderer(HWND hwnd, int width, int height) : width_(width), height_(height) {
    createInstance();
    createSurface(hwnd);
    selectPhysicalDevice();
    createLogicalDevice();
    createAllocator();
    createSwapchain();
    createSwapchainImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandPoolAndBuffers();
    createSyncObjects();

    // Phases 2-5 (not yet implemented - see PLAN.md's Vulkan port plan).
    // Each is currently a no-op stub; beginFrame()/draw*() guard on the
    // relevant pipeline handle still being VK_NULL_HANDLE so this milestone
    // (device/swapchain bring-up) renders a valid clear-color frame with no
    // geometry, matching the same first checkpoint the D3D11 pass hit.
    createWireframePipeline();
    createMinimapPipeline();
    createLabelPipeline();
    createMeshPipelineObjects();
    createTerrainPipelineObjects();
    createFixedGeometry();
}

VulkanRenderer::~VulkanRenderer() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    // Phase 14 - any async upload still in flight is now guaranteed
    // complete (vkDeviceWaitIdle above), so every fence here is safe to
    // destroy without checking status. Command buffers are deliberately
    // NOT individually freed here - vkDestroyCommandPool below frees every
    // command buffer ever allocated from it, tracked or not; freeing them
    // twice would be undefined behavior.
    for (const auto& pending : pendingUploads_) {
        vmaDestroyBuffer(allocator_, pending.stagingBuffer, pending.stagingAllocation);
        vkDestroyFence(device_, pending.fence, nullptr);
    }
    pendingUploads_.clear();

    for (auto semaphore : imageAvailableSemaphores_) vkDestroySemaphore(device_, semaphore, nullptr);
    for (auto semaphore : renderFinishedSemaphores_) vkDestroySemaphore(device_, semaphore, nullptr);
    for (auto fence : inFlightFences_) vkDestroyFence(device_, fence, nullptr);

    if (commandPool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, commandPool_, static_cast<uint32_t>(commandBuffers_.size()),
                              commandBuffers_.data());
        vkDestroyCommandPool(device_, commandPool_, nullptr);
    }

    for (auto framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    vkDestroyRenderPass(device_, renderPass_, nullptr);

    vkDestroyPipeline(device_, wireframePipeline_, nullptr);
    vkDestroyPipeline(device_, minimapPipeline_, nullptr);
    vkDestroyPipelineLayout(device_, wireframePipelineLayout_, nullptr);
    vkDestroyPipeline(device_, labelPipeline_, nullptr);
    vkDestroyPipelineLayout(device_, labelPipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_, textureDescriptorSetLayout_, nullptr);
    vkDestroySampler(device_, labelSampler_, nullptr);
    vkDestroySampler(device_, meshTextureSampler_, nullptr);
    vkDestroyPipeline(device_, meshPipeline_, nullptr);
    vkDestroyPipelineLayout(device_, meshPipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_, meshDescriptorSetLayout_, nullptr);
    vkDestroyPipeline(device_, terrainPipeline_, nullptr);
    vkDestroyPipelineLayout(device_, terrainPipelineLayout_, nullptr);
    vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);

    for (size_t i = 0; i < labelVertexBuffers_.size(); ++i) {
        vmaDestroyBuffer(allocator_, labelVertexBuffers_[i], labelVertexAllocations_[i]);
    }
    for (size_t i = 0; i < meshUniformBuffers_.size(); ++i) {
        vmaDestroyBuffer(allocator_, meshUniformBuffers_[i], meshUniformAllocations_[i]);
    }

    // Every texture/mesh this renderer ever created (see ownedTextures_'s
    // header comment for why this renderer, not the caller, owns them) -
    // descriptor sets don't need individually freeing, destroying
    // descriptorPool_ above already reclaims all of them.
    for (const auto& texture : ownedTextures_) {
        vkDestroyImageView(device_, texture.view, nullptr);
        vmaDestroyImage(allocator_, texture.image, texture.allocation);
    }
    for (const auto& mesh : ownedMeshes_) {
        vmaDestroyBuffer(allocator_, mesh.vertexBuffer, mesh.vertexAllocation);
        vmaDestroyBuffer(allocator_, mesh.indexBuffer, mesh.indexAllocation);
    }
    for (const auto& chunk : ownedTerrainChunks_) {
        vmaDestroyBuffer(allocator_, chunk.vertexBuffer, chunk.vertexAllocation);
        vmaDestroyBuffer(allocator_, chunk.indexBuffer, chunk.indexAllocation);
    }

    vmaDestroyBuffer(allocator_, boxVertexBuffer_, boxVertexAllocation_);
    vmaDestroyBuffer(allocator_, boxIndexBuffer_, boxIndexAllocation_);
    vmaDestroyBuffer(allocator_, circleVertexBuffer_, circleVertexAllocation_);

    vkDestroyImageView(device_, depthImageView_, nullptr);
    vmaDestroyImage(allocator_, depthImage_, depthImageAllocation_);

    for (auto view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);

    if (allocator_ != VK_NULL_HANDLE) vmaDestroyAllocator(allocator_);
    if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
    if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
}

void VulkanRenderer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "SWG Client - Crude Visualizer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "SWGClientNew";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME,
                                            VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

    std::vector<const char*> layers;
    if (kEnableValidation && validationLayerAvailable()) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    vkCheck(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
}

void VulkanRenderer::createSurface(HWND hwnd) {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = hwnd;
    createInfo.hinstance = GetModuleHandle(nullptr);
    vkCheck(vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_),
            "vkCreateWin32SurfaceKHR");
}

void VulkanRenderer::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("VulkanRenderer: no Vulkan-capable physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    uint32_t bestGraphics = UINT32_MAX;
    uint32_t bestPresent = UINT32_MAX;
    bool bestIsDiscrete = false;

    for (auto device : devices) {
        if (!deviceExtensionsSupported(device)) continue;
        SwapchainSupport support = querySwapchainSupport(device, surface_);
        if (support.formats.empty() || support.presentModes.empty()) continue;

        auto [graphics, present] = findQueueFamilies(device, surface_);
        if (graphics == UINT32_MAX || present == UINT32_MAX) continue;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        bool isDiscrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

        // Prefer the first discrete GPU found; otherwise take the first
        // usable device at all (integrated GPU, etc.) - this is a debug
        // visualizer, not a title with strict performance requirements.
        if (bestDevice == VK_NULL_HANDLE || (isDiscrete && !bestIsDiscrete)) {
            bestDevice = device;
            bestGraphics = graphics;
            bestPresent = present;
            bestIsDiscrete = isDiscrete;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanRenderer: no suitable Vulkan physical device found "
                                  "(needs VK_KHR_swapchain + a graphics/present-capable queue)");
    }

    physicalDevice_ = bestDevice;
    graphicsQueueFamily_ = bestGraphics;
    presentQueueFamily_ = bestPresent;
    depthFormat_ = findDepthFormat(physicalDevice_);
}

void VulkanRenderer::createLogicalDevice() {
    std::set<uint32_t> uniqueFamilies = {graphicsQueueFamily_, presentQueueFamily_};
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures features{}; // none needed yet (line-list topology and triangle-list
                                          // culling both work with default/no features enabled)

    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    vkCheck(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "vkCreateDevice");

    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);
}

void VulkanRenderer::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = physicalDevice_;
    allocatorInfo.device = device_;
    allocatorInfo.instance = instance_;
    vkCheck(static_cast<VkResult>(vmaCreateAllocator(&allocatorInfo, &allocator_)),
            "vmaCreateAllocator");
}

void VulkanRenderer::createSwapchain() {
    SwapchainSupport support = querySwapchainSupport(physicalDevice_, surface_);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
    VkExtent2D extent = chooseSwapExtent(support.capabilities, width_, height_);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {graphicsQueueFamily_, presentQueueFamily_};
    if (graphicsQueueFamily_ != presentQueueFamily_) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    vkCheck(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_),
            "vkCreateSwapchainKHR");

    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount, nullptr);
    swapchainImages_.resize(actualImageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &actualImageCount, swapchainImages_.data());

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
}

void VulkanRenderer::createSwapchainImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCheck(vkCreateImageView(device_, &viewInfo, nullptr, &swapchainImageViews_[i]),
                "vkCreateImageView (swapchain)");
    }
}

void VulkanRenderer::recreateSwapchain() {
    // The surface's own current capabilities are authoritative for the
    // real current size - independent of width_/height_'s possibly-stale
    // constructor-time hint, same query chooseSwapExtent() itself already
    // uses inside createSwapchain().
    SwapchainSupport support = querySwapchainSupport(physicalDevice_, surface_);
    VkExtent2D extent = chooseSwapExtent(support.capabilities, width_, height_);
    if (extent.width == 0 || extent.height == 0) {
        return; // fully minimized (or otherwise zero-sized) - nothing to create yet
    }

    vkDeviceWaitIdle(device_);

    for (auto framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffers_.clear();
    vkDestroyImageView(device_, depthImageView_, nullptr);
    vmaDestroyImage(allocator_, depthImage_, depthImageAllocation_);
    for (auto view : swapchainImageViews_) vkDestroyImageView(device_, view, nullptr);
    swapchainImageViews_.clear();
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);

    createSwapchain();
    createSwapchainImageViews();
    createDepthResources();
    createFramebuffers();
}

void VulkanRenderer::createDepthResources() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vkCheck(static_cast<VkResult>(vmaCreateImage(allocator_, &imageInfo, &allocInfo, &depthImage_,
                                                  &depthImageAllocation_, nullptr)),
            "vmaCreateImage (depth)");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (depthFormat_ == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat_ == VK_FORMAT_D24_UNORM_S8_UINT) {
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCheck(vkCreateImageView(device_, &viewInfo, nullptr, &depthImageView_),
            "vkCreateImageView (depth)");
}

void VulkanRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    vkCheck(vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_),
            "vkCreateRenderPass");
}

void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        std::array<VkImageView, 2> attachments = {swapchainImageViews_[i], depthImageView_};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        vkCheck(vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffers_[i]),
                "vkCreateFramebuffer");
    }
}

void VulkanRenderer::createCommandPoolAndBuffers() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamily_;
    vkCheck(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_),
            "vkCreateCommandPool");

    commandBuffers_.resize(kMaxFramesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    vkCheck(vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()),
            "vkAllocateCommandBuffers");
}

void VulkanRenderer::createSyncObjects() {
    imageAvailableSemaphores_.resize(kMaxFramesInFlight);
    inFlightFences_.resize(kMaxFramesInFlight);
    // renderFinishedSemaphores_ is sized per SWAPCHAIN IMAGE, not per
    // frame-in-flight, and indexed by currentImageIndex_ (not currentFrame_)
    // in endFrame() - a per-frame-in-flight semaphore here can still be
    // "in use" by the presentation engine for a previous frame's image when
    // vkQueueSubmit tries to re-signal it for a new frame using the same
    // slot, since frame-in-flight count and swapchain image count don't
    // necessarily march in lockstep. Confirmed as a real validation error
    // (VUID-vkQueueSubmit-pSignalSemaphores-00067) via this port's own
    // smoke test before this fix, not a hypothetical - see
    // https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html.
    renderFinishedSemaphores_.resize(swapchainImages_.size());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // starts signaled so frame 0's wait doesn't block

    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        vkCheck(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]),
                "vkCreateSemaphore (imageAvailable)");
        vkCheck(vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]),
                "vkCreateFence");
    }
    for (size_t i = 0; i < renderFinishedSemaphores_.size(); ++i) {
        vkCheck(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]),
                "vkCreateSemaphore (renderFinished)");
    }
}

VkCommandBuffer VulkanRenderer::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device_, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    // Setup-time only (never in the per-frame path) - a full queue wait is
    // fine here, no need for a fence-based async upload.
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
}

void VulkanRenderer::recordImageLayoutBarrier(VkCommandBuffer cmd, VkImage image,
                                               VkImageLayout oldLayout,
                                               VkImageLayout newLayout) const {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRenderer::submitAsyncUpload(VkCommandBuffer cmd, VkBuffer stagingBuffer,
                                        VmaAllocation stagingAllocation, uint64_t ownerKey) {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCheck(vkCreateFence(device_, &fenceInfo, nullptr, &fence), "vkCreateFence (async upload)");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkCheck(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, fence), "vkQueueSubmit (async upload)");

    pendingNotReady_.insert(ownerKey);
    pendingUploads_.push_back(
        PendingUpload{fence, cmd, stagingBuffer, stagingAllocation, ownerKey});
}

void VulkanRenderer::uploadDeviceLocalBuffer(const void* data, VkDeviceSize size,
                                              VkBufferUsageFlags usage, VkBuffer& outBuffer,
                                              VmaAllocation& outAllocation) const {
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;

    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo stagingAllocResult{};
    vkCheck(static_cast<VkResult>(vmaCreateBuffer(allocator_, &stagingInfo, &stagingAllocInfo,
                                                    &stagingBuffer, &stagingAllocation,
                                                    &stagingAllocResult)),
            "vmaCreateBuffer (staging)");
    std::memcpy(stagingAllocResult.pMappedData, data, static_cast<size_t>(size));

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufferAllocInfo{};
    bufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vkCheck(static_cast<VkResult>(vmaCreateBuffer(allocator_, &bufferInfo, &bufferAllocInfo,
                                                    &outBuffer, &outAllocation, nullptr)),
            "vmaCreateBuffer (device-local)");

    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferCopy copyRegion{0, 0, size};
    vkCmdCopyBuffer(cmd, stagingBuffer, outBuffer, 1, &copyRegion);
    endSingleTimeCommands(cmd);

    vmaDestroyBuffer(allocator_, stagingBuffer, stagingAllocation);
}

void VulkanRenderer::uploadDeviceLocalBufferAsync(const void* data, VkDeviceSize size,
                                                    VkBufferUsageFlags usage, VkBuffer& outBuffer,
                                                    VmaAllocation& outAllocation,
                                                    uint64_t ownerKey) {
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;

    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo stagingAllocResult{};
    vkCheck(static_cast<VkResult>(vmaCreateBuffer(allocator_, &stagingInfo, &stagingAllocInfo,
                                                    &stagingBuffer, &stagingAllocation,
                                                    &stagingAllocResult)),
            "vmaCreateBuffer (staging, async)");
    std::memcpy(stagingAllocResult.pMappedData, data, static_cast<size_t>(size));

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufferAllocInfo{};
    bufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vkCheck(static_cast<VkResult>(vmaCreateBuffer(allocator_, &bufferInfo, &bufferAllocInfo,
                                                    &outBuffer, &outAllocation, nullptr)),
            "vmaCreateBuffer (device-local, async)");

    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferCopy copyRegion{0, 0, size};
    vkCmdCopyBuffer(cmd, stagingBuffer, outBuffer, 1, &copyRegion);
    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (async upload)");

    // ownerKey==0 is a sentinel meaning "this buffer owns itself" - the
    // caller can't know outBuffer's identity before this call creates it
    // (needed for a resource's FIRST upload, e.g. a mesh's vertex buffer;
    // its second upload, e.g. the index buffer, passes the already-known
    // vertex buffer explicitly). 0 is never a valid non-null Vulkan handle,
    // so this is unambiguous.
    if (ownerKey == 0) {
        ownerKey = reinterpret_cast<uint64_t>(outBuffer);
    }
    submitAsyncUpload(cmd, stagingBuffer, stagingAllocation, ownerKey);
}

void VulkanRenderer::pollPendingUploads() {
    for (auto it = pendingUploads_.begin(); it != pendingUploads_.end();) {
        if (vkGetFenceStatus(device_, it->fence) != VK_SUCCESS) {
            ++it;
            continue;
        }

        vmaDestroyBuffer(allocator_, it->stagingBuffer, it->stagingAllocation);
        vkFreeCommandBuffers(device_, commandPool_, 1, &it->commandBuffer);
        vkDestroyFence(device_, it->fence, nullptr);
        uint64_t owner = it->ownerKey;
        it = pendingUploads_.erase(it);

        bool stillPending = std::any_of(
            pendingUploads_.begin(), pendingUploads_.end(),
            [owner](const PendingUpload& pending) { return pending.ownerKey == owner; });
        if (!stillPending) {
            pendingNotReady_.erase(owner);
        }
    }
}

void VulkanRenderer::createFixedGeometry() {
    uploadDeviceLocalBuffer(kBoxVertices.data(), sizeof(kBoxVertices),
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, boxVertexBuffer_,
                             boxVertexAllocation_);
    uploadDeviceLocalBuffer(kBoxIndices.data(), sizeof(kBoxIndices),
                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT, boxIndexBuffer_,
                             boxIndexAllocation_);

    std::vector<XMFLOAT3> circleVerts = buildCircleVertices();
    circleVertexCount_ = static_cast<uint32_t>(circleVerts.size());
    uploadDeviceLocalBuffer(circleVerts.data(), sizeof(XMFLOAT3) * circleVerts.size(),
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, circleVertexBuffer_,
                             circleVertexAllocation_);
}

namespace {
// Mirrors Wireframe.hlsl's PerDrawConstants struct byte-for-byte (worldViewProj
// mat4 + color vec4 = 80 bytes) - delivered via push constants, not a
// descriptor set, since it comfortably fits the guaranteed-minimum 128-byte
// push constant budget (see PLAN.md's "Baked pipeline state" / Mesh.hlsl's
// comment for the one case, drawMesh(), that does NOT fit and uses a
// uniform buffer instead).
struct WireframePushConstants {
    XMFLOAT4X4 worldViewProj;
    XMFLOAT4 color;
};
} // namespace

void VulkanRenderer::createWireframePipeline() {
    VkShaderModule vertModule = loadShaderModule("Wireframe", "VSMain");
    VkShaderModule fragModule = loadShaderModule("Wireframe", "PSMain");

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "VSMain";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "PSMain";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(XMFLOAT3);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute{};
    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attribute;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // irrelevant for LINE_LIST topology
    rasterizer.cullMode = VK_CULL_MODE_NONE;       // lines have no facing to cull
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(WireframePushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    vkCheck(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &wireframePipelineLayout_),
            "vkCreatePipelineLayout (wireframe)");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = wireframePipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    vkCheck(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &wireframePipeline_),
            "vkCreateGraphicsPipelines (wireframe)");

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

// Same wireframe shaders/vertex-input/pipeline layout as createWireframePipeline()
// - only the depth-test state differs (off here vs. test+write there). In
// Vulkan, depth-test on/off is baked into the VkPipeline itself rather than
// a swappable state object like D3D11's ID3D11DepthStencilState, so this is
// a second, separate pipeline object instead of a runtime toggle - see
// PLAN.md's "Baked pipeline state" known risk. Reloads the shader modules
// (createWireframePipeline() already destroyed its own copies once they were
// baked into that pipeline) rather than keeping them alive across two
// functions - a one-time setup cost, not a per-frame one.
void VulkanRenderer::createMinimapPipeline() {
    VkShaderModule vertModule = loadShaderModule("Wireframe", "VSMain");
    VkShaderModule fragModule = loadShaderModule("Wireframe", "PSMain");

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "VSMain";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "PSMain";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(XMFLOAT3);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute{};
    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attribute;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth test off: this pass' geometry has no meaningful relationship to
    // the main 3D scene's depth values in that same screen region (a
    // different, orthographic top-down projection) - always draw on top
    // instead of being incorrectly occluded, matching the D3D11 pass'
    // identical reasoning for its noDepthTestState_.
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = wireframePipelineLayout_; // shared with createWireframePipeline()
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    vkCheck(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &minimapPipeline_),
            "vkCreateGraphicsPipelines (minimap)");

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

namespace {
// Matches Label.hlsl's VSInput (position float3 + uv float2).
struct LabelVertex {
    XMFLOAT3 position;
    XMFLOAT2 uv;
};

// Matches Label.hlsl's push-constant LabelConstants (viewProj mat4 only -
// world space quad corners are computed CPU-side in drawLabel(), same as
// the D3D11 pass).
struct LabelPushConstants {
    XMFLOAT4X4 viewProj;
};

// How many label quads worth of scratch vertex space each frame-in-flight
// gets (see labelVertexBuffers_'s header comment) - generous headroom for a
// debug visualizer that only ever labels nearby objects.
constexpr uint32_t kMaxLabelsPerFrame = 256;
// How many distinct label textures (and, later, mesh uniform-buffer sets)
// the shared descriptor pool can ever hand out over the renderer's whole
// lifetime - labelCache in tools/dummyclient never evicts, so this needs
// enough headroom for every distinct object name ever seen in one session,
// not just one frame's worth.
constexpr uint32_t kMaxDescriptorSets = 4096;
} // namespace

void VulkanRenderer::createLabelPipeline() {
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerBinding;
    vkCheck(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &textureDescriptorSetLayout_),
            "vkCreateDescriptorSetLayout (label)");

    // Shared by the label pipeline (combined image samplers, one set per
    // distinct label texture) and, later, the mesh pipeline (uniform
    // buffers, one set per frame-in-flight - see createMeshPipelineObjects()).
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxDescriptorSets};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     static_cast<uint32_t>(kMaxFramesInFlight)};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = kMaxDescriptorSets + static_cast<uint32_t>(kMaxFramesInFlight);
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    vkCheck(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_),
            "vkCreateDescriptorPool");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCheck(vkCreateSampler(device_, &samplerInfo, nullptr, &labelSampler_),
            "vkCreateSampler (label)");

    // Per-frame-in-flight host-visible, persistently-mapped scratch vertex
    // buffer for drawLabel()'s dynamic quads - see the header's comment on
    // labelVertexBuffers_ for why this is per-frame rather than re-mapped
    // per draw call the way the D3D11 pass' single dynamic buffer was.
    labelVertexBuffers_.resize(kMaxFramesInFlight);
    labelVertexAllocations_.resize(kMaxFramesInFlight);
    labelVertexMapped_.resize(kMaxFramesInFlight);
    VkDeviceSize labelBufferSize = sizeof(LabelVertex) * 4 * kMaxLabelsPerFrame;
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = labelBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocResult{};
        vkCheck(static_cast<VkResult>(vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                                                        &labelVertexBuffers_[i],
                                                        &labelVertexAllocations_[i], &allocResult)),
                "vmaCreateBuffer (label vertex scratch)");
        labelVertexMapped_[i] = allocResult.pMappedData;
    }

    VkShaderModule vertModule = loadShaderModule("Label", "VSMain");
    VkShaderModule fragModule = loadShaderModule("Label", "PSMain");

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "VSMain";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "PSMain";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(LabelVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LabelVertex, position)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(LabelVertex, uv)};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Test depth, don't write it - matches the D3D11 pass' labelDepthStencilState_
    // exactly (labels should be occluded by nearer geometry but never
    // themselves occlude anything in the depth buffer).
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(LabelPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &textureDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    vkCheck(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &labelPipelineLayout_),
            "vkCreatePipelineLayout (label)");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = labelPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    vkCheck(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &labelPipeline_),
            "vkCreateGraphicsPipelines (label)");

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

namespace {
// Matches Mesh.hlsl's VSInput (position float3 + normal float3 + uv
// float2) - `uv` added Phase 19 (real mesh textures); assets::MeshData
// carried UV0 from the start but this was the first pass to actually
// upload/consume it.
struct MeshVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
};

// Matches Mesh.hlsl's PerDrawConstants uniform buffer (worldViewProj +
// world + color = 144 bytes) - delivered via a dynamic uniform buffer, not
// push constants, since it exceeds the guaranteed-minimum 128-byte push
// constant budget (see Mesh.hlsl's own comment and PLAN.md's "Baked
// pipeline state" risk).
struct MeshDrawConstants {
    XMFLOAT4X4 worldViewProj;
    XMFLOAT4X4 world;
    XMFLOAT4 color;
};

// How many drawMesh() calls' worth of uniform-buffer scratch space each
// frame-in-flight gets - generous headroom, mirrors kMaxLabelsPerFrame's
// reasoning above.
constexpr uint32_t kMaxMeshDrawsPerFrame = 512;

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
    return (value + alignment - 1) / alignment * alignment;
}
} // namespace

void VulkanRenderer::createMeshPipelineObjects() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboBinding;
    vkCheck(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &meshDescriptorSetLayout_),
            "vkCreateDescriptorSetLayout (mesh)");

    // REPEAT-addressed (unlike labelSampler_'s CLAMP) - real mesh UVs
    // commonly exceed [0,1] to tile a small real texture across a large
    // real surface (a building wall, a floor). Only ever samples a single
    // real mip level (see assets::DdsImageData's own comment on why only
    // the base mip is uploaded), so mip filtering settings don't matter.
    VkSamplerCreateInfo meshSamplerInfo{};
    meshSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    meshSamplerInfo.magFilter = VK_FILTER_LINEAR;
    meshSamplerInfo.minFilter = VK_FILTER_LINEAR;
    meshSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    meshSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    meshSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    meshSamplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    meshSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCheck(vkCreateSampler(device_, &meshSamplerInfo, nullptr, &meshTextureSampler_),
            "vkCreateSampler (mesh)");

    // A real 1x1 opaque-white texture - the fallback drawMesh() binds for
    // any MeshHandle whose own textureDescriptorSet is VK_NULL_HANDLE (see
    // MeshHandle's own comment for why a fallback is needed at all, not
    // just an unbound set).
    const uint8_t kWhitePixel[4] = {255, 255, 255, 255};
    whiteFallbackTexture_ =
        uploadTextureImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, kWhitePixel, sizeof(kWhitePixel),
                            meshTextureSampler_);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    meshUniformAlignment_ =
        alignUp(sizeof(MeshDrawConstants), props.limits.minUniformBufferOffsetAlignment);

    meshUniformBuffers_.resize(kMaxFramesInFlight);
    meshUniformAllocations_.resize(kMaxFramesInFlight);
    meshUniformMapped_.resize(kMaxFramesInFlight);
    meshUniformDescriptorSets_.resize(kMaxFramesInFlight);

    VkDeviceSize bufferSize = meshUniformAlignment_ * kMaxMeshDrawsPerFrame;
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocResult{};
        vkCheck(static_cast<VkResult>(vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                                                        &meshUniformBuffers_[i],
                                                        &meshUniformAllocations_[i], &allocResult)),
                "vmaCreateBuffer (mesh uniform)");
        meshUniformMapped_[i] = allocResult.pMappedData;

        VkDescriptorSetAllocateInfo setAllocInfo{};
        setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAllocInfo.descriptorPool = descriptorPool_;
        setAllocInfo.descriptorSetCount = 1;
        setAllocInfo.pSetLayouts = &meshDescriptorSetLayout_;
        vkCheck(vkAllocateDescriptorSets(device_, &setAllocInfo, &meshUniformDescriptorSets_[i]),
                "vkAllocateDescriptorSets (mesh)");

        VkDescriptorBufferInfo bufferDescInfo{};
        bufferDescInfo.buffer = meshUniformBuffers_[i];
        bufferDescInfo.offset = 0;
        bufferDescInfo.range = sizeof(MeshDrawConstants);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = meshUniformDescriptorSets_[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        write.pBufferInfo = &bufferDescInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    VkShaderModule vertModule = loadShaderModule("Mesh", "VSMain");
    VkShaderModule fragModule = loadShaderModule("Mesh", "PSMain");

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "VSMain";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "PSMain";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal)};
    attributes[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, uv)};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Cull mode NONE - matches the D3D11 pass exactly: its single shared
    // rasterizerState_ (D3D11_CULL_NONE) was set once in beginFrame() and
    // never swapped out for drawMesh(), so real meshes were never
    // backface-culled there either. Not revisiting that choice in this
    // port - see PLAN.md's "Winding/culling" known risk for why culling
    // specifically was flagged as something to verify empirically before
    // ever turning it on, not something assumed safe by default.
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Two descriptor sets: set 0 = the per-draw dynamic UBO (bound once per
    // frame-in-flight, dynamic offset per draw), set 1 = the per-submesh
    // real texture (a different descriptor set per drawMesh() call, unlike
    // set 0 - see drawMesh()'s own comment). textureDescriptorSetLayout_ is
    // the same shared layout the label pipeline already uses (see its own
    // comment) - a real texture's descriptor set is directly bindable here
    // with no extra plumbing.
    std::array<VkDescriptorSetLayout, 2> meshSetLayouts = {meshDescriptorSetLayout_,
                                                              textureDescriptorSetLayout_};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(meshSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = meshSetLayouts.data();
    vkCheck(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &meshPipelineLayout_),
            "vkCreatePipelineLayout (mesh)");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = meshPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    vkCheck(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &meshPipeline_),
            "vkCreateGraphicsPipelines (mesh)");

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

namespace {
// Matches Terrain.hlsl's VSInput (position/normal/color, all float3).
struct TerrainVertexGpu {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT3 color;
};

// Matches Terrain.hlsl's PerDrawConstants push constant (viewProj only, 64
// bytes - terrain vertices are already baked in world space, no per-draw
// model matrix needed, unlike Mesh.hlsl).
struct TerrainPushConstants {
    XMFLOAT4X4 viewProj;
};
} // namespace

// Mirrors createMeshPipelineObjects()'s vertex-input/rasterizer shape
// (TRIANGLE_LIST, cull mode NONE - same reasoning as drawMesh(): winding
// order was flagged as a real risk to verify empirically for terrain
// specifically (its normals are the first in this renderer that are
// dynamically derived, not baked into an asset) rather than assumed safe,
// so not culling sidesteps it entirely rather than risking invisible
// terrain if the winding turns out backwards), but delivers its per-draw
// constant via a push constant like createWireframePipeline() instead of
// mesh's dynamic-UBO indirection - see Terrain.hlsl's own comment for why.
void VulkanRenderer::createTerrainPipelineObjects() {
    VkShaderModule vertModule = loadShaderModule("Terrain", "VSMain");
    VkShaderModule fragModule = loadShaderModule("Terrain", "PSMain");

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "VSMain";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "PSMain";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertStage, fragStage};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(TerrainVertexGpu);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertexGpu, position)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertexGpu, normal)};
    attributes[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertexGpu, color)};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(TerrainPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    vkCheck(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &terrainPipelineLayout_),
            "vkCreatePipelineLayout (terrain)");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = terrainPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    vkCheck(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &terrainPipeline_),
            "vkCreateGraphicsPipelines (terrain)");

    vkDestroyShaderModule(device_, vertModule, nullptr);
    vkDestroyShaderModule(device_, fragModule, nullptr);
}

VkShaderModule VulkanRenderer::loadShaderModule(const char* spirvFileBaseName,
                                                 const char* entryPoint) const {
    std::string path = std::string(SWG_SHADER_SPV_DIR) + "/" + spirvFileBaseName + "_" +
                        entryPoint + ".spv";
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("VulkanRenderer: failed to open compiled shader: " + path);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule module = VK_NULL_HANDLE;
    vkCheck(vkCreateShaderModule(device_, &createInfo, nullptr, &module), "vkCreateShaderModule");
    return module;
}

void VulkanRenderer::beginFrame(float clearR, float clearG, float clearB) {
    pollPendingUploads();

    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    // A real, previously-unhandled crash (Phase 19 live test): the
    // swapchain going out of date (resize/minimize/DPI change/display
    // change) used to fall straight through to vkCheck() below and abort
    // the whole process. Recreate and retry instead - bounded (not an
    // infinite loop) so a genuinely stuck case (e.g. a surface that never
    // reports a real size again) still surfaces as a real error rather than
    // hanging forever; recreateSwapchain() itself no-ops harmlessly while
    // the window is minimized (zero-sized), so this also naturally waits
    // out a minimize without a separate code path for it.
    VkResult acquireResult =
        vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                               imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE,
                               &currentImageIndex_);
    for (int attempt = 0; acquireResult == VK_ERROR_OUT_OF_DATE_KHR && attempt < 100; ++attempt) {
        recreateSwapchain();
        Sleep(10);
        acquireResult =
            vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                   imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE,
                                   &currentImageIndex_);
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        vkCheck(acquireResult, "vkAcquireNextImageKHR");
    }

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

    currentCmd_ = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(currentCmd_, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkCheck(vkBeginCommandBuffer(currentCmd_, &beginInfo), "vkBeginCommandBuffer");

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{clearR, clearG, clearB, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = framebuffers_[currentImageIndex_];
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(currentCmd_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Negative-height viewport flips Vulkan's clip-space Y (opposite of
    // Direct3D's by default) so DirectXMath's existing LH projection/view
    // matrices need zero changes - see PLAN.md's "NDC Y-flip" known risk.
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(swapchainExtent_.height);
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = -static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(currentCmd_, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, swapchainExtent_};
    vkCmdSetScissor(currentCmd_, 0, 1, &scissor);

    labelVertexBufferOffset_ = 0;
    meshUniformOffset_ = 0;

    if (wireframePipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframePipeline_);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(currentCmd_, 0, 1, &boxVertexBuffer_, &offset);
        vkCmdBindIndexBuffer(currentCmd_, boxIndexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    }
}

void VulkanRenderer::setViewProjection(const XMMATRIX& view, const XMMATRIX& projection) {
    viewProjection_ = view * projection;
}

void VulkanRenderer::drawWireBox(const XMFLOAT3& center, const XMFLOAT3& halfExtents,
                                  float yawRadians, const XMFLOAT4& color) {
    if (wireframePipeline_ == VK_NULL_HANDLE) return;

    XMMATRIX world = XMMatrixScaling(halfExtents.x, halfExtents.y, halfExtents.z) *
                     XMMatrixRotationY(yawRadians) * XMMatrixTranslation(center.x, center.y, center.z);
    WireframePushConstants pc{};
    // HLSL cbuffers/push-constant blocks default to column-major layout,
    // matched to `mul(vector, matrix)`'s expected operand order in the
    // shader by transposing here - XMMATRIX itself is row-major in CPU
    // memory. Same convention the D3D11 pass used for its constant buffers.
    XMStoreFloat4x4(&pc.worldViewProj, XMMatrixTranspose(world * viewProjection_));
    pc.color = color;
    vkCmdPushConstants(currentCmd_, wireframePipelineLayout_,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc),
                        &pc);
    vkCmdDrawIndexed(currentCmd_, static_cast<uint32_t>(kBoxIndices.size()), 1, 0, 0, 0);
}

void VulkanRenderer::drawCircle(const XMFLOAT3& center, float radius, const XMFLOAT4& color) {
    if (wireframePipeline_ == VK_NULL_HANDLE) return;

    XMMATRIX world =
        XMMatrixScaling(radius, radius, radius) * XMMatrixTranslation(center.x, center.y, center.z);
    WireframePushConstants pc{};
    XMStoreFloat4x4(&pc.worldViewProj, XMMatrixTranspose(world * viewProjection_));
    pc.color = color;
    vkCmdPushConstants(currentCmd_, wireframePipelineLayout_,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc),
                        &pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &circleVertexBuffer_, &offset);
    vkCmdDraw(currentCmd_, circleVertexCount_, 1, 0, 0);

    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &boxVertexBuffer_, &offset);
    vkCmdBindIndexBuffer(currentCmd_, boxIndexBuffer_, 0, VK_INDEX_TYPE_UINT16);
}

void VulkanRenderer::beginMinimapPass(int x, int y, int width, int height) {
    if (minimapPipeline_ == VK_NULL_HANDLE) return;

    // Negative-height viewport for the same NDC Y-flip reasoning as
    // beginFrame() - see that function's comment.
    VkViewport viewport{};
    viewport.x = static_cast<float>(x);
    viewport.y = static_cast<float>(y + height);
    viewport.width = static_cast<float>(width);
    viewport.height = -static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(currentCmd_, 0, 1, &viewport);

    VkRect2D scissor{{x, y}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};
    vkCmdSetScissor(currentCmd_, 0, 1, &scissor);

    // Rebind the wireframe box buffers/pipeline explicitly - the label pass
    // that likely ran just before this one (see beginLabelPass()) left a
    // different pipeline/vertex buffer bound, same defensive re-bind the
    // D3D11 pass' beginMinimapPass() did for its own state.
    vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, minimapPipeline_);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &boxVertexBuffer_, &offset);
    vkCmdBindIndexBuffer(currentCmd_, boxIndexBuffer_, 0, VK_INDEX_TYPE_UINT16);
}

TextureHandle VulkanRenderer::createTextTexture(const std::wstring& text, int& outPixelWidth,
                                                 int& outPixelHeight) {
    // ---- GDI text rasterization: identical to the D3D11 pass' own
    // createTextTexture() (same font, same white-on-black + luminance-as-alpha
    // trick) - GDI itself has nothing to do with the graphics backend. ----
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    HFONT font = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, font));

    SIZE textExtent{};
    GetTextExtentPoint32W(memDC, text.c_str(), static_cast<int>(text.size()), &textExtent);
    int width = std::max(1, static_cast<int>(textExtent.cx));
    int height = std::max(1, static_cast<int>(textExtent.cy));

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // negative = top-down, matches TextOutW(0,0,...)
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, dib));

    RECT rect{0, 0, width, height};
    FillRect(memDC, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));
    TextOutW(memDC, 0, 0, text.c_str(), static_cast<int>(text.size()));

    std::vector<uint8_t> rgbaData(static_cast<size_t>(width) * height * 4);
    auto* pixels = static_cast<const uint32_t*>(bits);
    for (int i = 0; i < width * height; ++i) {
        uint8_t alpha = static_cast<uint8_t>(pixels[i] & 0xFF);
        rgbaData[static_cast<size_t>(i) * 4 + 0] = 255;
        rgbaData[static_cast<size_t>(i) * 4 + 1] = 255;
        rgbaData[static_cast<size_t>(i) * 4 + 2] = 255;
        rgbaData[static_cast<size_t>(i) * 4 + 3] = alpha;
    }

    SelectObject(memDC, oldBitmap);
    SelectObject(memDC, oldFont);
    DeleteObject(dib);
    DeleteObject(font);
    DeleteDC(memDC);

    outPixelWidth = width;
    outPixelHeight = height;
    return uploadTextureImage(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                               VK_FORMAT_R8G8B8A8_UNORM, rgbaData.data(), rgbaData.size(),
                               labelSampler_);
}

// The real "upload raw pixel/block bytes as a sampled VkImage + view +
// descriptor set" sequence - originally createTextTexture()'s own back
// half, factored out here (Phase 19) so loadTexture() (real compressed mesh
// textures) can reuse it verbatim instead of re-deriving the same
// staging-buffer/barrier/descriptor-set boilerplate a second time. Neither
// caller's own pixel format/sampler choice matters to this function - it
// only ever moves bytes into a 2D sampled image and wires up a descriptor
// set against the shared textureDescriptorSetLayout_.
TextureHandle VulkanRenderer::uploadTextureImage(uint32_t width, uint32_t height, VkFormat format,
                                                  const void* pixelData, size_t dataSize,
                                                  VkSampler sampler) {
    TextureHandle handle;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vkCheck(static_cast<VkResult>(vmaCreateImage(allocator_, &imageInfo, &allocInfo, &handle.image,
                                                  &handle.allocation, nullptr)),
            "vmaCreateImage (texture)");

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(dataSize);
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;

    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = imageSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo stagingAllocResult{};
    vkCheck(static_cast<VkResult>(vmaCreateBuffer(allocator_, &stagingInfo, &stagingAllocInfo,
                                                    &stagingBuffer, &stagingAllocation,
                                                    &stagingAllocResult)),
            "vmaCreateBuffer (texture staging)");
    std::memcpy(stagingAllocResult.pMappedData, pixelData, static_cast<size_t>(imageSize));

    // Async (Phase 14) - a new texture streams in the first time it's
    // needed, live, not just at startup. Barrier+copy+barrier recorded into
    // ONE command buffer/ONE submission (rather than 3 separate blocking
    // round trips) so the whole sequence completes as a single fence wait,
    // polled by pollPendingUploads() - drawLabel()/drawMesh() skip a
    // texture that isn't ready yet. The image ends the recorded sequence in
    // SHADER_READ_ONLY_OPTIMAL, so once the fence signals it's already in
    // the correct layout to sample - no separate "wait then transition"
    // step needed.
    VkCommandBuffer cmd = beginSingleTimeCommands();
    recordImageLayoutBarrier(cmd, handle.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, handle.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                            &region);
    recordImageLayoutBarrier(cmd, handle.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer (texture upload)");
    submitAsyncUpload(cmd, stagingBuffer, stagingAllocation,
                       reinterpret_cast<uint64_t>(handle.image));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = handle.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCheck(vkCreateImageView(device_, &viewInfo, nullptr, &handle.view), "vkCreateImageView (texture)");

    VkDescriptorSetAllocateInfo setAllocInfo{};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = descriptorPool_;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &textureDescriptorSetLayout_;
    vkCheck(vkAllocateDescriptorSets(device_, &setAllocInfo, &handle.descriptorSet),
            "vkAllocateDescriptorSets (texture)");

    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.sampler = sampler;
    imageDescInfo.imageView = handle.view;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = handle.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescInfo;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    ownedTextures_.push_back(handle);
    return handle;
}

TextureHandle VulkanRenderer::loadTexture(const assets::DdsImageData& dds) {
    VkFormat format;
    switch (dds.format) {
        case assets::DdsBlockFormat::Bc1: format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; break;
        case assets::DdsBlockFormat::Bc2: format = VK_FORMAT_BC2_UNORM_BLOCK; break;
        case assets::DdsBlockFormat::Bc3: format = VK_FORMAT_BC3_UNORM_BLOCK; break;
        default: throw std::runtime_error("VulkanRenderer::loadTexture: unhandled DdsBlockFormat");
    }
    return uploadTextureImage(dds.width, dds.height, format, dds.blockData.data(),
                               dds.blockData.size(), meshTextureSampler_);
}

void VulkanRenderer::beginLabelPass() {
    if (labelPipeline_ == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, labelPipeline_);

    LabelPushConstants pc{};
    XMStoreFloat4x4(&pc.viewProj, XMMatrixTranspose(viewProjection_));
    vkCmdPushConstants(currentCmd_, labelPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc),
                        &pc);
}

void VulkanRenderer::drawLabel(const XMFLOAT3& worldCenter, float width, float height,
                                const XMFLOAT3& right, const XMFLOAT3& up,
                                const TextureHandle& texture) {
    if (labelPipeline_ == VK_NULL_HANDLE) return;
    if (pendingNotReady_.count(reinterpret_cast<uint64_t>(texture.image)) > 0) return;

    XMVECTOR centerVec = XMLoadFloat3(&worldCenter);
    XMVECTOR rightVec = XMVectorScale(XMLoadFloat3(&right), width * 0.5f);
    XMVECTOR upVec = XMVectorScale(XMLoadFloat3(&up), height * 0.5f);

    XMFLOAT3 bottomLeft, bottomRight, topLeft, topRight;
    XMStoreFloat3(&bottomLeft, XMVectorSubtract(XMVectorSubtract(centerVec, rightVec), upVec));
    XMStoreFloat3(&bottomRight, XMVectorSubtract(XMVectorAdd(centerVec, rightVec), upVec));
    XMStoreFloat3(&topLeft, XMVectorAdd(XMVectorSubtract(centerVec, rightVec), upVec));
    XMStoreFloat3(&topRight, XMVectorAdd(XMVectorAdd(centerVec, rightVec), upVec));

    // Triangle strip order (BL, BR, TL, TR) covers the quad as (BL,BR,TL) +
    // (TL,BR,TR); UV (0,0) at top-left matches the top-down DIB
    // createTextTexture() produces - identical layout to the D3D11 pass.
    LabelVertex verts[4] = {
        {bottomLeft, {0.0f, 1.0f}},
        {bottomRight, {1.0f, 1.0f}},
        {topLeft, {0.0f, 0.0f}},
        {topRight, {1.0f, 0.0f}},
    };

    VkDeviceSize quadBytes = sizeof(verts);
    VkDeviceSize maxBytes = sizeof(LabelVertex) * 4 * kMaxLabelsPerFrame;
    if (labelVertexBufferOffset_ + quadBytes > maxBytes) {
        // Scratch buffer for this frame is full (see kMaxLabelsPerFrame) -
        // drop the label rather than overrun the buffer. A debug visualizer
        // showing fewer labels than usual in an extreme case is fine; memory
        // corruption is not.
        return;
    }

    auto* dst = reinterpret_cast<uint8_t*>(labelVertexMapped_[currentFrame_]) +
                labelVertexBufferOffset_;
    std::memcpy(dst, verts, sizeof(verts));

    VkDeviceSize offset = labelVertexBufferOffset_;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &labelVertexBuffers_[currentFrame_], &offset);
    vkCmdBindDescriptorSets(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, labelPipelineLayout_, 0, 1,
                             &texture.descriptorSet, 0, nullptr);
    vkCmdDraw(currentCmd_, 4, 1, 0, 0);

    labelVertexBufferOffset_ += quadBytes;
}

MeshHandle VulkanRenderer::loadStaticMesh(const assets::MeshData& mesh) {
    std::vector<MeshVertex> vertices(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        vertices[i].position = XMFLOAT3(mesh.positions[i].x, mesh.positions[i].y, mesh.positions[i].z);
        vertices[i].normal = XMFLOAT3(mesh.normals[i].x, mesh.normals[i].y, mesh.normals[i].z);
        // uv0 is parsed for every real mesh (see assets::StaticMesh/
        // SkeletalMesh) but a caller building MeshData by hand (e.g. a
        // building cell's own merged geometry) may leave it empty - default
        // to (0,0) rather than indexing out of range.
        if (i < mesh.uv0.size()) {
            vertices[i].uv = XMFLOAT2(mesh.uv0[i].x, mesh.uv0[i].y);
        } else {
            vertices[i].uv = XMFLOAT2(0.0f, 0.0f);
        }
    }

    MeshHandle handle;
    handle.indexCount = static_cast<uint32_t>(mesh.indices.size());

    // Async (Phase 14) - real objects stream in live as they come into
    // view, so this can no longer block the render thread the way it could
    // when meshes only ever loaded at startup. Both uploads share
    // handle.vertexBuffer as their owner key; drawMesh() below skips this
    // mesh until pollPendingUploads() confirms both have completed.
    uploadDeviceLocalBufferAsync(vertices.data(), sizeof(MeshVertex) * vertices.size(),
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, handle.vertexBuffer,
                                  handle.vertexAllocation, 0); // 0 = owns itself, see comment
    uint64_t ownerKey = reinterpret_cast<uint64_t>(handle.vertexBuffer);
    uploadDeviceLocalBufferAsync(mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size(),
                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT, handle.indexBuffer,
                                  handle.indexAllocation, ownerKey);

    ownedMeshes_.push_back(handle);
    return handle;
}

void VulkanRenderer::drawMesh(const MeshHandle& handle, const XMFLOAT3& center, float yawRadians,
                               const XMFLOAT4& color) {
    if (meshPipeline_ == VK_NULL_HANDLE) return;
    if (pendingNotReady_.count(reinterpret_cast<uint64_t>(handle.vertexBuffer)) > 0) return;

    VkDeviceSize maxBytes = meshUniformAlignment_ * kMaxMeshDrawsPerFrame;
    if (meshUniformOffset_ + meshUniformAlignment_ > maxBytes) {
        // Scratch uniform space for this frame is full (see
        // kMaxMeshDrawsPerFrame) - drop the draw rather than overrun the
        // buffer, same graceful-degradation choice as drawLabel().
        return;
    }

    XMMATRIX world = XMMatrixRotationY(yawRadians) * XMMatrixTranslation(center.x, center.y, center.z);
    MeshDrawConstants constants{};
    XMStoreFloat4x4(&constants.worldViewProj, XMMatrixTranspose(world * viewProjection_));
    XMStoreFloat4x4(&constants.world, XMMatrixTranspose(world));
    constants.color = color;

    auto* dst = reinterpret_cast<uint8_t*>(meshUniformMapped_[currentFrame_]) + meshUniformOffset_;
    std::memcpy(dst, &constants, sizeof(constants));

    vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline_);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &handle.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(currentCmd_, handle.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    uint32_t dynamicOffset = static_cast<uint32_t>(meshUniformOffset_);
    vkCmdBindDescriptorSets(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineLayout_, 0, 1,
                             &meshUniformDescriptorSets_[currentFrame_], 1, &dynamicOffset);

    // Set 1: this submesh's real texture, or the shared white fallback if
    // it has none (see MeshHandle::textureDescriptorSet's own comment) -
    // Vulkan requires every set the pipeline layout declares to be bound.
    VkDescriptorSet textureSet = handle.textureDescriptorSet != VK_NULL_HANDLE
                                      ? handle.textureDescriptorSet
                                      : whiteFallbackTexture_.descriptorSet;
    vkCmdBindDescriptorSets(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineLayout_, 1, 1,
                             &textureSet, 0, nullptr);

    vkCmdDrawIndexed(currentCmd_, handle.indexCount, 1, 0, 0, 0);

    meshUniformOffset_ += meshUniformAlignment_;

    // Restore the wireframe pipeline + box vertex/index buffers so
    // subsequent drawWireBox() calls this frame work unchanged - same
    // reasoning as drawCircle(), and matches the D3D11 pass' own
    // drawMesh(), which had the identical restore-state requirement at
    // its end.
    if (wireframePipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframePipeline_);
        VkDeviceSize boxOffset = 0;
        vkCmdBindVertexBuffers(currentCmd_, 0, 1, &boxVertexBuffer_, &boxOffset);
        vkCmdBindIndexBuffer(currentCmd_, boxIndexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    }
}

TerrainChunkHandle VulkanRenderer::loadTerrainChunk(const terrain::TerrainMeshData& mesh) {
    std::vector<TerrainVertexGpu> vertices(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& v = mesh.vertices[i];
        vertices[i].position = XMFLOAT3(v.position.x, v.position.y, v.position.z);
        vertices[i].normal = XMFLOAT3(v.normal.x, v.normal.y, v.normal.z);
        vertices[i].color = XMFLOAT3(v.color.x, v.color.y, v.color.z);
    }

    TerrainChunkHandle handle;
    handle.indexCount = static_cast<uint32_t>(mesh.indices.size());

    // Async (Phase 14) - chunks stream in continuously as the player roams,
    // so this can no longer block the render thread. Same owner-key pattern
    // as loadStaticMesh(): drawTerrainChunk() skips this chunk until both
    // uploads complete.
    uploadDeviceLocalBufferAsync(vertices.data(), sizeof(TerrainVertexGpu) * vertices.size(),
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, handle.vertexBuffer,
                                  handle.vertexAllocation, 0); // 0 = owns itself
    uint64_t ownerKey = reinterpret_cast<uint64_t>(handle.vertexBuffer);
    uploadDeviceLocalBufferAsync(mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size(),
                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT, handle.indexBuffer,
                                  handle.indexAllocation, ownerKey);

    ownedTerrainChunks_.push_back(handle);
    return handle;
}

void VulkanRenderer::unloadTerrainChunk(const TerrainChunkHandle& handle) {
    uint64_t ownerKey = reinterpret_cast<uint64_t>(handle.vertexBuffer);
    if (pendingNotReady_.count(ownerKey) > 0) {
        // Rare edge case: eviction requested before this chunk's async
        // upload finished (would need crossing a 128m chunk boundary within
        // a frame or two of it first loading - not expected in practice).
        // Destroying a buffer a submitted GPU command still references
        // would be a real use-after-free, so wait specifically for this
        // chunk's own pending fences rather than risk it - a rare-path
        // blocking wait, not a reason to redesign the common path.
        for (auto& pending : pendingUploads_) {
            if (pending.ownerKey == ownerKey) {
                vkWaitForFences(device_, 1, &pending.fence, VK_TRUE, UINT64_MAX);
            }
        }
        pollPendingUploads();
    }

    // The far more common case: this chunk has already been uploaded and
    // drawn by a recent frame's command buffer, which - with
    // kMaxFramesInFlight == 2 - may STILL be executing on the GPU right
    // now (beginFrame() only waits on the CURRENT slot's fence before
    // recording a new frame, never on every in-flight frame). Destroying
    // this chunk's buffers while a not-yet-finished command buffer's draw
    // call still references them is a real use-after-free the validation
    // layer correctly flagged (VUID-vkDestroyBuffer-buffer-00922,
    // confirmed live and reproducible every session terrain chunks stream
    // in/out - not fixed by the pending-upload wait above, which only
    // covers the much narrower "evicted before its own upload finished"
    // race).
    //
    // This function runs mid-frame - after this frame's own beginFrame()
    // already reset inFlightFences_[currentFrame_] to unsignaled (it only
    // gets re-signaled once THIS frame's endFrame() submits it, later in
    // this same call stack), so waiting on that particular fence here
    // would deadlock forever (confirmed live: the very first live test of
    // an earlier "wait on every fence" version hung the whole process
    // solid). beginFrame()'s own wait already proved every PRIOR use of
    // that slot finished before this frame started recording, so it's
    // provably safe to skip. Every OTHER slot, though, may still be
    // executing whatever the previous frame(s) submitted - waiting on
    // those is what actually closes the race.
    for (size_t i = 0; i < inFlightFences_.size(); ++i) {
        if (i == currentFrame_) {
            continue;
        }
        vkWaitForFences(device_, 1, &inFlightFences_[i], VK_TRUE, UINT64_MAX);
    }

    vmaDestroyBuffer(allocator_, handle.vertexBuffer, handle.vertexAllocation);
    vmaDestroyBuffer(allocator_, handle.indexBuffer, handle.indexAllocation);

    auto it = std::find_if(ownedTerrainChunks_.begin(), ownedTerrainChunks_.end(),
                            [&](const TerrainChunkHandle& owned) {
                                return owned.vertexBuffer == handle.vertexBuffer;
                            });
    if (it != ownedTerrainChunks_.end()) {
        ownedTerrainChunks_.erase(it);
    }
}

void VulkanRenderer::drawTerrainChunk(const TerrainChunkHandle& handle) {
    if (terrainPipeline_ == VK_NULL_HANDLE) return;
    if (pendingNotReady_.count(reinterpret_cast<uint64_t>(handle.vertexBuffer)) > 0) return;

    TerrainPushConstants pc{};
    XMStoreFloat4x4(&pc.viewProj, XMMatrixTranspose(viewProjection_));

    vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipeline_);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &handle.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(currentCmd_, handle.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdPushConstants(currentCmd_, terrainPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                        sizeof(pc), &pc);

    vkCmdDrawIndexed(currentCmd_, handle.indexCount, 1, 0, 0, 0);

    // Restore the wireframe pipeline + box vertex/index buffers, same
    // reasoning as drawMesh()'s own identical restore-state requirement.
    if (wireframePipeline_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframePipeline_);
        VkDeviceSize boxOffset = 0;
        vkCmdBindVertexBuffers(currentCmd_, 0, 1, &boxVertexBuffer_, &boxOffset);
        vkCmdBindIndexBuffer(currentCmd_, boxIndexBuffer_, 0, VK_INDEX_TYPE_UINT16);
    }
}

void VulkanRenderer::endFrame() {
    vkCmdEndRenderPass(currentCmd_);
    vkCheck(vkEndCommandBuffer(currentCmd_), "vkEndCommandBuffer");

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // Indexed by the acquired swapchain image, not the frame-in-flight slot
    // - see createSyncObjects()'s comment on renderFinishedSemaphores_.
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentImageIndex_]};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &currentCmd_;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkCheck(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]),
            "vkQueueSubmit");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImageIndex_;

    // Same real crash this session's own console log caught
    // (VkResult=-1000001004 = VK_ERROR_OUT_OF_DATE_KHR) - by the time
    // present reports this, the frame is already submitted/gone, so there's
    // nothing to retry here; just recreate so the NEXT beginFrame() gets a
    // valid swapchain instead of repeating the same failure forever.
    // VK_SUBOPTIMAL_KHR also triggers a recreate (still presentable, but a
    // real signal the swapchain no longer exactly matches the surface -
    // e.g. mid-resize) rather than being silently ignored like before.
    VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        vkCheck(presentResult, "vkQueuePresentKHR");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

} // namespace renderer
