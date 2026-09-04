/*
 * wEditor
 * Copyright (C) 2026 TheProjectDark
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "VulkanGraphics/VulkanGraphics.h"

#include <vector>
#include <stdexcept>

VulkanGraphics::VulkanGraphics() {
    m_glfwInitialized = glfwInit() == GLFW_TRUE;
    if (!m_glfwInitialized) {
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    m_window = glfwCreateWindow(800, 600, "Vulkan graphics test", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        m_glfwInitialized = false;
        return;
    }

    InitializeVulkan();
    m_initialized = true;
}

VulkanGraphics::~VulkanGraphics() {
    if (m_window && !glfwWindowShouldClose(m_window)) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    if (m_window != nullptr || m_instance != VK_NULL_HANDLE || m_device != VK_NULL_HANDLE) {
        Cleanup();
    }

    if (m_glfwInitialized) {
        glfwTerminate();
        m_glfwInitialized = false;
    }
}

bool VulkanGraphics::IsReady() const {
    return m_initialized;
}

bool VulkanGraphics::IsRunning() const {
    return m_running;
}

bool VulkanGraphics::ShouldClose() const {
    return m_window == nullptr || glfwWindowShouldClose(m_window);
}

void VulkanGraphics::Show() {
    if (!m_window) {
        return;
    }

    if (!m_initialized) {
        InitializeVulkan();
        m_initialized = true;
    }

    glfwShowWindow(m_window);
    glfwFocusWindow(m_window);
    m_running = true;
}

void VulkanGraphics::Close() {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    if (m_window != nullptr || m_instance != VK_NULL_HANDLE || m_device != VK_NULL_HANDLE) {
        Cleanup();
    }

    m_running = false;
}

bool VulkanGraphics::InitializeVulkan() {
    if (!m_window) {
        return false;
    }

    if (m_instance == VK_NULL_HANDLE) {
        CreateInstance();
    }

    if (m_surface == VK_NULL_HANDLE) {
        CreateSurface();
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        PickPhysicalDevice();
    }

    if (m_device == VK_NULL_HANDLE) {
        CreateLogicalDevice();
    }

    return m_window != nullptr;
}

bool VulkanGraphics::CreateInstance() {
    uint32_t extensionCount = 0;
    const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "wStresser Vulkan demo";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName = "wStresser";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = requiredExtensions;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    return result == VK_SUCCESS && m_instance != VK_NULL_HANDLE;
}

bool VulkanGraphics::CreateSurface() {
    if (!m_instance) {
        return false;
    }

    VkResult result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
    return result == VK_SUCCESS && m_surface != VK_NULL_HANDLE;
}

bool VulkanGraphics::PickPhysicalDevice() {
    if (m_instance == VK_NULL_HANDLE) {
        return false;
    }

    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS) {
        return false;
    }

    for (VkPhysicalDevice device : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            VkBool32 presentSupported = VK_FALSE;
            if (glfwGetPhysicalDevicePresentationSupport(m_instance, device, i)) {
                presentSupported = VK_TRUE;
            }

            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupported) {
                m_physicalDevice = device;
                m_graphicsQueueFamily = i;
                return true;
            }
        }
    }

    return false;
}

bool VulkanGraphics::CreateLogicalDevice() {
    if (m_physicalDevice == VK_NULL_HANDLE) {
        return false;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    VkResult result = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    if (result != VK_SUCCESS || m_device == VK_NULL_HANDLE) {
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    return m_graphicsQueue != VK_NULL_HANDLE;
}

void VulkanGraphics::RunLoop() {
    while (m_window && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        glfwWaitEventsTimeout(0.016);
    }

    m_running = false;
    Cleanup();
}

void VulkanGraphics::Cleanup() {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    m_initialized = false;
}
