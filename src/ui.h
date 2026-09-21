#pragma once
#ifndef UI_H
#define UI_H

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <stdio.h>
#include <cstdlib>
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        std::abort();
}
#endif // !UI_H
int yearMin = 2000;
int yearMax = 6000;
class Ui
{
public:
    ImGui_ImplVulkanH_Window    g_MainWindowData;
    VkDescriptorPool            g_DescriptorPool = VK_NULL_HANDLE;
    int                         g_MinImageCount = 2;
    GLFWwindow*                 g_glfwWindow = nullptr;
    VkInstance                  g_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice            g_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice                    g_Device = VK_NULL_HANDLE;
    uint32_t                    g_QueueFamily = 0;
    VkAllocationCallbacks*      g_Allocator = nullptr;
    ImGuiIO                     io;
    VkRenderPass                m_renderPass = VK_NULL_HANDLE;
    bool                        labels[10] = {true, true, true, true, true, true, true, true, true, true};
    bool                        m_userParamterUpdated = false;
    bool                        weekdayLabels[7] = {true,true,true,true,true,true,true};
    
    Ui()
    {

    }

    Ui(GLFWwindow* glfwWindow,
        VkSurfaceKHR& YOUR_SURFACE,
        VkInstance& YOUR_INSTANCE, 
        VkPhysicalDevice& YOUR_PHYSICAL_DEVICE, 
        VkDevice& YOUR_DEVICE, 
        unsigned int YOUR_QUEUE_FAMILY,
        VkQueue& YOUR_QUEUE,
        VkRenderPass& YOUR_RENDERPASS,
        unsigned int NUM_OF_FRAMES_IN_FLIGHT,
        VkPipelineCache YOUR_PIPELINE_CACHE = VK_NULL_HANDLE,
        VkAllocationCallbacks* YOUR_ALLOCATOR = nullptr
        ):
        g_glfwWindow(glfwWindow),
        g_Instance(YOUR_INSTANCE),
        g_PhysicalDevice(YOUR_PHYSICAL_DEVICE),
        g_Device(YOUR_DEVICE),
        g_Allocator(YOUR_ALLOCATOR),
        g_QueueFamily(YOUR_QUEUE_FAMILY),
        g_MinImageCount(NUM_OF_FRAMES_IN_FLIGHT),
        m_renderPass(YOUR_RENDERPASS)
    {
        // Get min image count in swapchain
        VkSurfaceCapabilitiesKHR surface_capabilities;
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, YOUR_SURFACE, &surface_capabilities);
        if (result != VK_SUCCESS) throw std::runtime_error("\nERROR: Failed to obtain surface capabilites while constructing ui.");

        // Create Descriptor Pool
        VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        VkResult err = vkCreateDescriptorPool(YOUR_DEVICE, &pool_info, YOUR_ALLOCATOR, &g_DescriptorPool);
        check_vk_result(err);

        // Setup ImGui Window 
        /*
        int w, h;
        glfwGetFramebufferSize(g_glfwWindow, &w, &h);
        ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
        wd->Surface = YOUR_SURFACE;
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(YOUR_PHYSICAL_DEVICE, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);
        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
        wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(YOUR_PHYSICAL_DEVICE, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
        IM_ASSERT(g_MinImageCount >= 2);
        ImGui_ImplVulkanH_CreateOrResizeWindow(YOUR_INSTANCE, YOUR_PHYSICAL_DEVICE, YOUR_DEVICE, wd, YOUR_QUEUE_FAMILY, YOUR_ALLOCATOR, w, h, g_MinImageCount);
        */

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight()
        // 
        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(g_glfwWindow,true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = YOUR_INSTANCE;
        init_info.PhysicalDevice = YOUR_PHYSICAL_DEVICE;
        init_info.Device = YOUR_DEVICE;
        init_info.QueueFamily = YOUR_QUEUE_FAMILY;
        init_info.Queue = YOUR_QUEUE;
        init_info.PipelineCache = YOUR_PIPELINE_CACHE;
        init_info.DescriptorPool = g_DescriptorPool;
        init_info.RenderPass = m_renderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = 2;// surface_capabilities.minImageCount;
        init_info.ImageCount = 2;// g_MinImageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = YOUR_ALLOCATOR;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info);
    }

    void update(GeoLife::UserParameters& ubo, float my_tex_w, float my_tex_h, VkDescriptorSet tex_id)
    {
        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        //ImGui::ShowDemoWindow();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        bool bool1, bool2;
        ImVec4 my_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Geo Life Parameters");   
        ImGui::Text("Time Frame:");
        ImGui::DragIntRange2("Year Min/Max", &ubo.yearMin, &ubo.yearMax, 0.05, 2007, 2013);
        ImGui::DragIntRange2("Month Min/Max", &ubo.monthMin, &ubo.monthMax, 0.05, 1, 12);
        ImGui::DragIntRange2("Day Min/Max", &ubo.dayMin, &ubo.dayMax, 0.05, 1, 31);
        ImGui::DragIntRange2("Hour Min/Max", &ubo.hourMin, &ubo.hourMax, 0.05, 0, 23 );
        ImGui::Checkbox("Mo", &weekdayLabels[0]); ImGui::SameLine();
            ImGui::Checkbox("Tu", &weekdayLabels[1]); ImGui::SameLine();
            ImGui::Checkbox("We", &weekdayLabels[2]); ImGui::SameLine();
            ImGui::Checkbox("Th", &weekdayLabels[3]); ImGui::SameLine();
            ImGui::Checkbox("Fr", &weekdayLabels[4]); ImGui::SameLine();
            ImGui::Checkbox("Sa", &weekdayLabels[5]); ImGui::SameLine();
            ImGui::Checkbox("Su", &weekdayLabels[6]); 
        ImGui::Text("Means of Transport:");
        ImGui::Checkbox("unkown", &labels[0]); ImGui::SameLine(); 
        ImGui::Checkbox("walk", &labels[1]); ImGui::SameLine(); 
            ImGui::Checkbox("bike", &labels[2]); ImGui::SameLine();
            ImGui::Checkbox("bus", &labels[3]); ImGui::SameLine(); 
            ImGui::Checkbox("car", &labels[4]); 
        ImGui::Checkbox("motor bike", &labels[9]); ImGui::SameLine(); 
            ImGui::Checkbox("subway", &labels[5]); ImGui::SameLine(); 
            ImGui::Checkbox("train", &labels[6]); ImGui::SameLine(); 
            ImGui::Checkbox("other", &labels[7]);
        
        ubo.labelNA          = labels[0] ? 1 : 0;
        ubo.labelWALK        = labels[1] ? 1 : 0;
        ubo.labelBIKE        = labels[2] ? 1 : 0;
        ubo.labelBUS         = labels[3] ? 1 : 0;
        ubo.labelCAR         = labels[4] ? 1 : 0;
        ubo.labelSUBWAY      = labels[5] ? 1 : 0;
        ubo.labelTRAIN       = labels[6] ? 1 : 0;
        ubo.labelAIRPLANE    = labels[7] ? 1 : 0;
        ubo.labelBOAT        = labels[7] ? 1 : 0;
        ubo.labelMOTORCYCLE  = labels[9] ? 1 : 0;
        ubo.labelMonday      = weekdayLabels[0] ? 1 : 0;
        ubo.labelTuesday     = weekdayLabels[1] ? 1 : 0;
        ubo.labelWednesday   = weekdayLabels[2] ? 1 : 0;
        ubo.labelThursday    = weekdayLabels[3] ? 1 : 0;
        ubo.labelFriday      = weekdayLabels[4] ? 1 : 0;
        ubo.labelSaturday    = weekdayLabels[5] ? 1 : 0;
        ubo.labelSunday      = weekdayLabels[6] ? 1 : 0;
     
        ImGui::Text("KDE Parameters:");               // Display some text (you can use a format strings too)
        ImGui::SliderFloat("Sigma", &ubo.sigma, 0.0f , 3.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::SliderInt("Kernel Size", &ubo.kernelSize, 1, 5);
        ImVec2 uv_min = ImVec2(0.0f,0.0f);                 // Top-left
        ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
        ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        ImGui::Text(" Max"); //ImGui::SameLine();
        ImGui::Image((ImTextureID)tex_id, ImVec2( my_tex_h, 0.3*my_tex_w), uv_min, uv_max, tint_col, border_col); //ImGui::SameLine();
        ImGui::Text(" Min");
        ImGui::End();
    }

    // Rendering within command buffer
    void recordCommandBufferForRendering(VkCommandBuffer& commandBuffer)
    {
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            // Record dear imgui primitives into command buffer
            ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
        }
    }

    void cleanup()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};
