/*
 * wEditor
 * Copyright (C) 2026 TheProjectDark
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class VulkanGraphics {
public:
    VulkanGraphics();
    ~VulkanGraphics();

    void Show();
    void Close();
    void DrawFrame();          //call every frame to render

    bool IsReady() const;
    bool IsRunning() const;
    bool ShouldClose() const;

private:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    //instance
    GLFWwindow*         m_window      = nullptr;
    VkInstance          m_instance    = VK_NULL_HANDLE;
    VkSurfaceKHR        m_surface     = VK_NULL_HANDLE;
    VkPhysicalDevice    m_physicalDevice = VK_NULL_HANDLE;
    VkDevice            m_device      = VK_NULL_HANDLE;
    VkQueue             m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue             m_presentQueue  = VK_NULL_HANDLE;
    uint32_t            m_graphicsQueueFamily = 0;
    uint32_t            m_presentQueueFamily  = 0;

    //swapchain
    VkSwapchainKHR           m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>     m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat                 m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D               m_swapchainExtent{};

    //depth
    VkImage        m_depthImage   = VK_NULL_HANDLE;
    VkDeviceMemory m_depthMemory  = VK_NULL_HANDLE;
    VkImageView    m_depthView    = VK_NULL_HANDLE;

    //pipeline
    VkRenderPass     m_renderPass     = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_graphicsPipeline = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    //geometry
    VkBuffer       m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       m_indexBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory  = VK_NULL_HANDLE;
    uint32_t       m_indexCount = 0;

    //uniforms
    std::vector<VkBuffer>       m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;
    std::vector<void*>          m_uniformBuffersMapped;

    VkDescriptorPool             m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    //commands
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    //sync
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;
    uint32_t                 m_currentFrame = 0;

    //state
    bool m_running = false;
    bool m_initialized = false;
    bool m_glfwInitialized = false;
    bool m_framebufferResized = false;

    std::chrono::steady_clock::time_point m_startTime;

    //init
    bool InitializeVulkan();
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain();
    bool CreateImageViews();
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateDescriptorSetLayout();
    bool CreateGraphicsPipeline();
    bool CreateFramebuffers();
    bool CreateCommandPool();
    bool CreateVertexBuffer();
    bool CreateIndexBuffer();
    bool CreateUniformBuffers();
    bool CreateDescriptorPool();
    bool CreateDescriptorSets();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();

    //helpers
    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props,
                      VkBuffer& buffer, VkDeviceMemory& memory);
    bool CreateImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                     VkImage& image, VkDeviceMemory& memory);
    VkImageView CreateImageView(VkImage image, VkFormat format,
                                VkImageAspectFlags aspect);
    VkFormat FindDepthFormat();
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                 VkImageTiling tiling, VkFormatFeatureFlags features);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    static std::vector<char> ReadFile(const std::string& filename);

    void RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void UpdateUniformBuffer(uint32_t currentImage);

    void RecreateSwapchain();
    void CleanupSwapchain();
    void Cleanup();

    static void FramebufferResizeCallback(GLFWwindow* window, int w, int h);
};