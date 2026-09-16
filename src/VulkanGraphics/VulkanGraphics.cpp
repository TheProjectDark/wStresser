#include "VulkanGraphics/VulkanGraphics.h"

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

//minimal math
namespace {

struct Mat4 {
    // column-major
    float m[16]{};

    static Mat4 Identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    static Mat4 Perspective(float fovY, float aspect, float zn, float zf) {
        Mat4 r{};
        float f = 1.0f / std::tan(fovY * 0.5f);
        r.m[0]  = f / aspect;
        r.m[5]  = -f;                       //Y-flip for Vulkan
        r.m[10] = zf / (zn - zf);
        r.m[11] = -1.0f;
        r.m[14] = (zf * zn) / (zn - zf);
        return r;
    }
    static Mat4 Translate(float x, float y, float z) {
        Mat4 r = Identity();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }
    static Mat4 RotateY(float a) {
        Mat4 r = Identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }
    static Mat4 RotateX(float a) {
        Mat4 r = Identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    sum += m[k * 4 + row] * o.m[c * 4 + k];
                r.m[c * 4 + row] = sum;
            }
        return r;
    }
};

struct Vertex {
    float pos[3];
    float color[3];

    static VkVertexInputBindingDescription GetBinding() {
        VkVertexInputBindingDescription b{};
        b.binding   = 0;
        b.stride    = sizeof(Vertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }
    static std::array<VkVertexInputAttributeDescription, 2> GetAttributes() {
        std::array<VkVertexInputAttributeDescription, 2> a{};
        a[0].binding  = 0;
        a[0].location = 0;
        a[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        a[0].offset   = offsetof(Vertex, pos);
        a[1].binding  = 0;
        a[1].location = 1;
        a[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        a[1].offset   = offsetof(Vertex, color);
        return a;
    }
};

struct UniformBufferObject {
    Mat4 mvp;
};

const std::vector<Vertex> kVertices = {
    // 8 углов куба с цветами
    {{-0.5f, -0.5f, -0.5f}, {1, 0, 0}},  // 0
    {{ 0.5f, -0.5f, -0.5f}, {0, 1, 0}},  // 1
    {{ 0.5f,  0.5f, -0.5f}, {0, 0, 1}},  // 2
    {{-0.5f,  0.5f, -0.5f}, {1, 1, 0}},  // 3
    {{-0.5f, -0.5f,  0.5f}, {1, 0, 1}},  // 4
    {{ 0.5f, -0.5f,  0.5f}, {0, 1, 1}},  // 5
    {{ 0.5f,  0.5f,  0.5f}, {1, 1, 1}},  // 6
    {{-0.5f,  0.5f,  0.5f}, {.5f,.5f,.5f}}, // 7
};

const std::vector<uint16_t> kIndices = {
    0,1,2, 2,3,0,   //back
    4,5,6, 6,7,4,   //front
    0,4,7, 7,3,0,   //left
    1,5,6, 6,2,1,   //right
    3,7,6, 6,2,3,   //top
    0,1,5, 5,4,0,   //bottom
};

} // namespace

//life cycle
VulkanGraphics::VulkanGraphics() {
    m_glfwInitialized = glfwInit() == GLFW_TRUE;
    if (!m_glfwInitialized) return;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    m_window = glfwCreateWindow(800, 600, "Vulkan graphics test", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        m_glfwInitialized = false;
        return;
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);

    m_initialized = InitializeVulkan();
    m_startTime = std::chrono::steady_clock::now();
    m_fpsTimer = m_startTime;
}

VulkanGraphics::~VulkanGraphics() {
    if (m_window && !glfwWindowShouldClose(m_window))
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);

    Cleanup();

    if (m_glfwInitialized) {
        glfwTerminate();
        m_glfwInitialized = false;
    }
}

void VulkanGraphics::FramebufferResizeCallback(GLFWwindow* window, int, int) {
    auto app = reinterpret_cast<VulkanGraphics*>(glfwGetWindowUserPointer(window));
    if (app) app->m_framebufferResized = true;
}

void VulkanGraphics::Show() {
    if (!m_window) return;
    if (!m_initialized) { m_initialized = InitializeVulkan(); }
    glfwShowWindow(m_window);
    glfwFocusWindow(m_window);
    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
}

void VulkanGraphics::Close() {
    if (m_window) glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    Cleanup();
    m_running = false;
}

bool VulkanGraphics::IsReady()     const { return m_initialized; }
bool VulkanGraphics::IsRunning()   const { return m_running; }
bool VulkanGraphics::ShouldClose() const {
    return m_window == nullptr || glfwWindowShouldClose(m_window);
}

//init
bool VulkanGraphics::InitializeVulkan() {
    if (!m_window) return false;

    if (!CreateInstance())       return false;
    if (!CreateSurface())        return false;
    if (!PickPhysicalDevice())   return false;
    if (!CreateLogicalDevice())  return false;
    if (!CreateSwapchain())      return false;
    if (!CreateImageViews())     return false;
    if (!CreateRenderPass())     return false;
    if (!CreateDescriptorSetLayout()) return false;
    if (!CreateGraphicsPipeline())    return false;
    if (!CreateDepthResources())      return false;
    if (!CreateFramebuffers())        return false;
    if (!CreateCommandPool())         return false;
    if (!CreateVertexBuffer())        return false;
    if (!CreateIndexBuffer())         return false;
    if (!CreateUniformBuffers())      return false;
    if (!CreateDescriptorPool())      return false;
    if (!CreateDescriptorSets())      return false;
    if (!CreateCommandBuffers())      return false;
    if (!CreateSyncObjects())         return false;

    return true;
}

bool VulkanGraphics::CreateInstance() {
    uint32_t extCount = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&extCount);

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "wEditor";
    app.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app.pEngineName = "wEditor";
    app.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = extCount;
    ci.ppEnabledExtensionNames = exts;

    return vkCreateInstance(&ci, nullptr, &m_instance) == VK_SUCCESS;
}

bool VulkanGraphics::CreateSurface() {
    return glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) == VK_SUCCESS;
}

bool VulkanGraphics::PickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) return false;

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    for (VkPhysicalDevice dev : devices) {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qs(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qs.data());

        int gfx = -1, present = -1;
        for (uint32_t i = 0; i < qCount; ++i) {
            if ((qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && gfx < 0) gfx = (int)i;
            VkBool32 supports = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &supports);
            if (supports && present < 0) present = (int)i;
        }

        if (gfx >= 0 && present >= 0) {
            m_physicalDevice = dev;
            m_graphicsQueueFamily = (uint32_t)gfx;
            m_presentQueueFamily  = (uint32_t)present;
            return true;
        }
    }
    return false;
}

bool VulkanGraphics::CreateLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> qcis;
    const float prio = 1.0f;
    for (uint32_t fam : {m_graphicsQueueFamily, m_presentQueueFamily}) {
        bool dup = false;
        for (auto& q : qcis) if (q.queueFamilyIndex == fam) dup = true;
        if (dup) continue;
        VkDeviceQueueCreateInfo q{};
        q.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q.queueFamilyIndex = fam;
        q.queueCount = 1;
        q.pQueuePriorities = &prio;
        qcis.push_back(q);
    }

    const char* deviceExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = (uint32_t)qcis.size();
    ci.pQueueCreateInfos = qcis.data();
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = deviceExts;
    ci.pEnabledFeatures = &features;

    if (vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device) != VK_SUCCESS)
        return false;

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily,  0, &m_presentQueue);
    return true;
}

//swapchain
bool VulkanGraphics::CreateSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosen = formats[0];
    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            chosen = f;

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> pms(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &pmCount, pms.data());
    for (auto pm : pms)
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) presentMode = pm;

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w, h;
        glfwGetFramebufferSize(m_window, &w, &h);
        extent.width  = std::max(caps.minImageExtent.width,  std::min(caps.maxImageExtent.width,  (uint32_t)w));
        extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, (uint32_t)h));
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = m_surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = chosen.format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t indices[] = { m_graphicsQueueFamily, m_presentQueueFamily };
    if (m_graphicsQueueFamily != m_presentQueueFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = indices;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain) != VK_SUCCESS)
        return false;

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
    m_swapchainImageFormat = chosen.format;
    m_swapchainExtent = extent;
    return true;
}

VkImageView VulkanGraphics::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = format;
    ci.subresourceRange.aspectMask = aspect;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;
    VkImageView view = VK_NULL_HANDLE;
    vkCreateImageView(m_device, &ci, nullptr, &view);
    return view;
}

bool VulkanGraphics::CreateImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
        m_swapchainImageViews[i] = CreateImageView(m_swapchainImages[i], m_swapchainImageFormat,
                                                   VK_IMAGE_ASPECT_COLOR_BIT);
    return true;
}

uint32_t VulkanGraphics::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((typeFilter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return 0;
}

bool VulkanGraphics::CreateImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageTiling tiling,
                                 VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                                 VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.extent = {w, h, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.format = fmt;
    ci.tiling = tiling;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.usage = usage;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &ci, nullptr, &image) != VK_SUCCESS) return false;

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(m_device, image, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
    if (vkAllocateMemory(m_device, &ai, nullptr, &memory) != VK_SUCCESS) return false;
    vkBindImageMemory(m_device, image, memory, 0);
    return true;
}

VkFormat VulkanGraphics::FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                             VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat f : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, f, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) return f;
        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (props.optimalTilingFeatures & features) == features) return f;
    }
    return VK_FORMAT_D32_SFLOAT;
}

VkFormat VulkanGraphics::FindDepthFormat() {
    return FindSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool VulkanGraphics::CreateDepthResources() {
    VkFormat fmt = FindDepthFormat();
    if (!CreateImage(m_swapchainExtent.width, m_swapchainExtent.height, fmt,
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_depthImage, m_depthMemory))
        return false;
    m_depthView = CreateImageView(m_depthImage, fmt, VK_IMAGE_ASPECT_DEPTH_BIT);
    return m_depthView != VK_NULL_HANDLE;
}

//pipeline/render pass
bool VulkanGraphics::CreateRenderPass() {
    VkAttachmentDescription color{};
    color.format = m_swapchainImageFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = FindDepthFormat();
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { color, depth };

    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = (uint32_t)attachments.size();
    ci.pAttachments = attachments.data();
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    return vkCreateRenderPass(m_device, &ci, nullptr, &m_renderPass) == VK_SUCCESS;
}

bool VulkanGraphics::CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ubo{};
    ubo.binding = 0;
    ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo.descriptorCount = 1;
    ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1;
    ci.pBindings = &ubo;

    return vkCreateDescriptorSetLayout(m_device, &ci, nullptr, &m_descriptorSetLayout) == VK_SUCCESS;
}

VkShaderModule VulkanGraphics::CreateShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule m = VK_NULL_HANDLE;
    vkCreateShaderModule(m_device, &ci, nullptr, &m);
    return m;
}

bool VulkanGraphics::CreateGraphicsPipeline() {
    auto vertCode = ReadFile("shaders/cube.vert.spv");
    auto fragCode = ReadFile("shaders/cube.frag.spv");
    if (vertCode.empty() || fragCode.empty()) return false;

    VkShaderModule vertMod = CreateShaderModule(vertCode);
    VkShaderModule fragMod = CreateShaderModule(fragCode);
    if (!vertMod || !fragMod) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    auto binding = Vertex::GetBinding();
    auto attrs = Vertex::GetAttributes();

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
    vi.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rast{};
    rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.lineWidth = 1.0f;
    rast.cullMode = VK_CULL_MODE_NONE;
    rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &m_descriptorSetLayout;

    if (vkCreatePipelineLayout(m_device, &plci, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(m_device, vertMod, nullptr);
        vkDestroyShaderModule(m_device, fragMod, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount = 2;
    pci.pStages = stages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rast;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dyn;
    pci.layout = m_pipelineLayout;
    pci.renderPass = m_renderPass;
    pci.subpass = 0;

    VkResult res = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pci, nullptr, &m_graphicsPipeline);

    vkDestroyShaderModule(m_device, vertMod, nullptr);
    vkDestroyShaderModule(m_device, fragMod, nullptr);
    return res == VK_SUCCESS;
}

bool VulkanGraphics::CreateFramebuffers() {
    m_framebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = { m_swapchainImageViews[i], m_depthView };

        VkFramebufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = m_renderPass;
        ci.attachmentCount = 2;
        ci.pAttachments = attachments;
        ci.width = m_swapchainExtent.width;
        ci.height = m_swapchainExtent.height;
        ci.layers = 1;

        if (vkCreateFramebuffer(m_device, &ci, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

bool VulkanGraphics::CreateCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = m_graphicsQueueFamily;
    return vkCreateCommandPool(m_device, &ci, nullptr, &m_commandPool) == VK_SUCCESS;
}

//buffers
bool VulkanGraphics::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags props,
                                  VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &ci, nullptr, &buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(m_device, buffer, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
    if (vkAllocateMemory(m_device, &ai, nullptr, &memory) != VK_SUCCESS) return false;
    vkBindBufferMemory(m_device, buffer, memory, 0);
    return true;
}

bool VulkanGraphics::CreateVertexBuffer() {
    VkDeviceSize size = sizeof(Vertex) * kVertices.size();
    if (!CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      m_vertexBuffer, m_vertexBufferMemory)) return false;

    void* data = nullptr;
    vkMapMemory(m_device, m_vertexBufferMemory, 0, size, 0, &data);
    std::memcpy(data, kVertices.data(), (size_t)size);
    vkUnmapMemory(m_device, m_vertexBufferMemory);
    return true;
}

bool VulkanGraphics::CreateIndexBuffer() {
    VkDeviceSize size = sizeof(uint16_t) * kIndices.size();
    if (!CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      m_indexBuffer, m_indexBufferMemory)) return false;

    void* data = nullptr;
    vkMapMemory(m_device, m_indexBufferMemory, 0, size, 0, &data);
    std::memcpy(data, kIndices.data(), (size_t)size);
    vkUnmapMemory(m_device, m_indexBufferMemory);
    m_indexCount = (uint32_t)kIndices.size();
    return true;
}

bool VulkanGraphics::CreateUniformBuffers() {
    VkDeviceSize size = sizeof(UniformBufferObject);
    m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (!CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          m_uniformBuffers[i], m_uniformBuffersMemory[i]))
            return false;
        vkMapMemory(m_device, m_uniformBuffersMemory[i], 0, size, 0, &m_uniformBuffersMapped[i]);
    }
    return true;
}

//descriptors
bool VulkanGraphics::CreateDescriptorPool() {
    VkDescriptorPoolSize size{};
    size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    size.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &size;
    ci.maxSets = MAX_FRAMES_IN_FLIGHT;
    return vkCreateDescriptorPool(m_device, &ci, nullptr, &m_descriptorPool) == VK_SUCCESS;
}

bool VulkanGraphics::CreateDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_descriptorPool;
    ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    ai.pSetLayouts = layouts.data();

    m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_device, &ai, m_descriptorSets.data()) != VK_SUCCESS)
        return false;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = m_uniformBuffers[i];
        bi.offset = 0;
        bi.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = m_descriptorSets[i];
        w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(m_device, 1, &w, 0, nullptr);
    }
    return true;
}

bool VulkanGraphics::CreateCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = m_commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    return vkAllocateCommandBuffers(m_device, &ai, m_commandBuffers.data()) == VK_SUCCESS;
}

bool VulkanGraphics::CreateSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_device, &si, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(m_device, &si, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) return false;
        if (vkCreateFence(m_device, &fi, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) return false;
    }
    return true;
}

//files
std::vector<char> VulkanGraphics::ReadFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    size_t size = (size_t)file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
}

//drawing
void VulkanGraphics::UpdateUniformBuffer(uint32_t frame) {
    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - m_startTime).count();

    Mat4 model = Mat4::RotateY(t * 1.2f) * Mat4::RotateX(t * 0.7f);
    Mat4 view  = Mat4::Translate(0.0f, 0.0f, -3.0f);
    Mat4 proj  = Mat4::Perspective(45.0f * 3.14159265f / 180.0f,
                                   m_swapchainExtent.width / (float)m_swapchainExtent.height,
                                   0.1f, 10.0f);

    UniformBufferObject ubo{};
    ubo.mvp = proj * view * model;
    std::memcpy(m_uniformBuffersMapped[frame], &ubo, sizeof(ubo));
}

void VulkanGraphics::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clears[2];
    clears[0].color = {{ 0.05f, 0.07f, 0.10f, 1.0f }};
    clears[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = m_renderPass;
    rp.framebuffer = m_framebuffers[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = m_swapchainExtent;
    rp.clearValueCount = 2;
    rp.pClearValues = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    VkViewport vp{};
    vp.x = 0; vp.y = 0;
    vp.width  = (float)m_swapchainExtent.width;
    vp.height = (float)m_swapchainExtent.height;
    vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = m_swapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    VkBuffer vbs[] = { m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);

    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

void VulkanGraphics::DrawFrame() {
    if (!m_initialized || m_device == VK_NULL_HANDLE) return;

    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                         m_imageAvailableSemaphores[m_currentFrame],
                                         VK_NULL_HANDLE, &imageIndex);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) { RecreateSwapchain(); return; }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) return;

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    UpdateUniformBuffer(m_currentFrame);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    RecordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSem[] = { m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSem;
    si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    VkSemaphore signalSem[] = { m_renderFinishedSemaphores[m_currentFrame] };
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = signalSem;

    if (vkQueueSubmit(m_graphicsQueue, 1, &si, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
        return;

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = signalSem;
    VkSwapchainKHR scs[] = { m_swapchain };
    pi.swapchainCount = 1;
    pi.pSwapchains = scs;
    pi.pImageIndices = &imageIndex;

    res = vkQueuePresentKHR(m_presentQueue, &pi);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        RecreateSwapchain();
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    //FPS
    ++m_frameCount;
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_fpsTimer).count();
    if (elapsed >= 0.5f) {
        m_fps = m_frameCount / elapsed;
        m_frameCount = 0;
        m_fpsTimer = now;
        if (m_window) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Vulkan graphics test — %.1f FPS", m_fps);
            glfwSetWindowTitle(m_window, buf);
        }
    }
}

//re-creating swapchain and cleaning up resources
void VulkanGraphics::CleanupSwapchain() {
    if (m_depthView)    { vkDestroyImageView(m_device, m_depthView, nullptr); m_depthView = VK_NULL_HANDLE; }
    if (m_depthImage)   { vkDestroyImage(m_device, m_depthImage, nullptr); m_depthImage = VK_NULL_HANDLE; }
    if (m_depthMemory)  { vkFreeMemory(m_device, m_depthMemory, nullptr); m_depthMemory = VK_NULL_HANDLE; }

    for (auto fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();

    for (auto v : m_swapchainImageViews) vkDestroyImageView(m_device, v, nullptr);
    m_swapchainImageViews.clear();

    if (m_swapchain) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
}

void VulkanGraphics::RecreateSwapchain() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(m_window, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(m_device);

    CleanupSwapchain();
    CreateSwapchain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
}

void VulkanGraphics::Cleanup() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);

        CleanupSwapchain();

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }

        if (m_commandPool) vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        for (size_t i = 0; i < m_uniformBuffers.size(); ++i) {
            vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr);
            vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
        }
        if (m_indexBuffer)  { vkDestroyBuffer(m_device, m_indexBuffer, nullptr);  vkFreeMemory(m_device, m_indexBufferMemory, nullptr); }
        if (m_vertexBuffer) { vkDestroyBuffer(m_device, m_vertexBuffer, nullptr); vkFreeMemory(m_device, m_vertexBufferMemory, nullptr); }

        if (m_descriptorPool)      vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        if (m_descriptorSetLayout) vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        if (m_graphicsPipeline)    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        if (m_pipelineLayout)      vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        if (m_renderPass)          vkDestroyRenderPass(m_device, m_renderPass, nullptr);

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface) { vkDestroySurfaceKHR(m_instance, m_surface, nullptr); m_surface = VK_NULL_HANDLE; }
    if (m_instance) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }
    if (m_window)   { glfwDestroyWindow(m_window); m_window = nullptr; }

    m_initialized = false;
}