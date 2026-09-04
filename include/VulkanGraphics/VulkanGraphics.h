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

#include <thread>

class VulkanGraphics {
public:
    VulkanGraphics();
    ~VulkanGraphics();

    void Show();
    void Close();
    bool IsReady() const;
    bool IsRunning() const;
    bool ShouldClose() const;

private:
    GLFWwindow* m_window = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    bool m_running = false;
    bool m_initialized = false;
    bool m_glfwInitialized = false;
    uint32_t m_graphicsQueueFamily = 0;

    bool InitializeVulkan();
    bool CreateInstance();
    bool CreateSurface();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    void RunLoop();
    void Cleanup();
};
