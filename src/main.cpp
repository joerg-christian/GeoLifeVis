//Don't use
// #include <vulkan/vulkan.h>
// Instead use
#define GLFW_INCLUDE_VULKAN

// Our favorite Image loading header
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_FORCE_RADIANS // Ensures that all GLM functions eat radians
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // This will force GLM to use a version of vec2 and mat4 that has the alignment requirements already specified for us
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstring> // Necessary for strcmp / memcpy
#include <string>
#include <filesystem>
#include <vector>
#include <array>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <optional> 
#include <set>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <fstream>


//Own headers
#include "geoLifeData.h"
#include "ui.h"

bool global_leftMouseButtonReleased = false;
bool global_firstFrame = true;

// Struct to store different queus that are available
struct QueueFamilyIndices 
{
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        // complete if quese supports both: graphics and present functionality
        return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};



// Struct to store info about swap chain support
struct SwapChainSupportDetails 
{
    VkSurfaceCapabilitiesKHR capabilities;      // min/max number of images in swap chain, min/max width and height of images etc.
    std::vector<VkSurfaceFormatKHR> formats;    // Surface formats (pixel format, color space)
    std::vector<VkPresentModeKHR> presentModes; // Conditions for swapping images to the screen
};

// Auxillary function: find address of vkCreateDebugUtilsMessengerEXT for validation layers debugging
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) 
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) 
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

// Auxillary function:  destroy Debug Messenger
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, debugMessenger, pAllocator);
}


// Vertices
struct Vertex 
{
    glm::vec3 pos;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() 
    {
        VkVertexInputBindingDescription bindingDescription{};

            //index of the binding in the array of bindings.
            /*
            binding is an index into the pBuffers array bound by vkCmdBindVertexBuffers.Each vertex attribute has a binding that says which buffer binding index the attribute gets its 
            data from.But the buffer itself is not specified at creation time.You set it dynamically with vkCmdBindVertexBuffers.
            */
            bindingDescription.binding = 0; 
            bindingDescription.stride = sizeof(Vertex);

            // VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
            // VK_VERTEX_INPUT_RATE_INSTANCE : Move to the next data entry after each instance
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; 
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() 
    {
        // We have position and color as vertex attributes >> need 2 Attribute descriptions
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // position
        // Note: For position use same formats as for colors usually like with this scheme:
        //  float: VK_FORMAT_R32_SFLOAT, vec2: VK_FORMAT_R32G32_SFLOAT, vec3 : VK_FORMAT_R32G32B32_SFLOAT, vec4 : VK_FORMAT_R32G32B32A32_SFLOAT
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Texture
        attributeDescriptions[1].binding = 0; 
        attributeDescriptions[1].location = 1; 
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT; 
        attributeDescriptions[1].offset = offsetof(Vertex, texCoord); 

        return attributeDescriptions;
    }
};

const std::vector<Vertex> verticesForQuad = 
{
    {{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},  // right bottom
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},  // left bottom
    {{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}},  // left top
    {{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}}   // right top
};
const std::vector<uint16_t> indicesForQuad = {0, 1, 2, 2, 3, 0};
bool useindices = true;
std::vector<Vertex> vertices = verticesForQuad; // shorter naming 

// Mouse Control
class MouseControl
{
public:
    glm::vec2 m_cursorPosition; // position wrt upper left corner of window
    glm::vec2 m_relativeMousePosition;
    float m_zoom = 1.f; // A value between 0 and 1. The whole map has sidelength 1. No zoom means m_zoom = 1. Maximum Zoom means m_zoom = 0.1
    float m_incrementFromMouse = 0.;
    bool m_updateZoom = false;
    glm::vec2 m_origin = glm::vec2(0.f,0.f);

    // width / height are window resolution
    void update(GLFWwindow* window, GeoLife::UserParameters& ubo, int width, int height)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        m_cursorPosition = glm::vec2((float)x, (float)y);
        if(m_updateZoom)
        {            
            // calculate normalized relative position of cursor wrt upper left window corner in current frame
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            glm::vec2 relativeCursorPosition((float)mouseX / width, (float)mouseY / height); 

            // calculate global position of cursor in current frame (value in [0,1] wrt complete maptexture)
            glm::vec2 globalPosition = m_origin + (relativeCursorPosition * m_zoom);
            
            // calculate new zoom from mouse wheel input and clamp to[0.1,1]
            float zoomIncrement =  glm::abs(m_incrementFromMouse) < 1? - m_incrementFromMouse :  0.1 * glm::max(-1.f, glm::min(1.f,m_incrementFromMouse));
            float newZoom = m_zoom - zoomIncrement;
            newZoom = glm::max(0.1f, glm::min(1.f, newZoom));
            
            // calculate new origin of window for new zoom such that relative position of current global cursore position stays the same
            glm::vec2 newOrigin = m_origin + (m_zoom - newZoom) * relativeCursorPosition; //globalPosition - (0.5f * m_zoom * glm::vec2(1.f, 1.f));

            // new origin cannot leave the map texture  >> must be between 0 and (1- new_zoom ) >> Apparently this is a wrong idea.
            newOrigin.x = glm::max(0.f, newOrigin.x);
            newOrigin.y = glm::max(0.f, newOrigin.y);

            // new (1,1) can also not leave map texture, shift new origin accordingly
            glm::vec2 newUpperRight = newOrigin + glm::vec2(newZoom);
            if (newUpperRight.x > 1) newOrigin.x = newOrigin.x - (newUpperRight.x - 1);
            if (newUpperRight.y > 1) newOrigin.y = newOrigin.y - (newUpperRight.y - 1);
            
            // Write new values to UBO
            m_zoom = newZoom;
            m_origin = newOrigin;
            ubo.zoom = newZoom; // A value between 0 and 1. The whole map has sidelength 1. No zoom means m_zoom = 1. Maximum Zoom means m_zoom = 0.1
            ubo.originX = newOrigin.x;
            ubo.originY = newOrigin.y;
        }
    }
};

MouseControl g_mouseControl;
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_mouseControl.m_incrementFromMouse = float(yoffset);
    g_mouseControl.m_updateZoom = true;
}



// Vulkan App
class GeoLifeVisualistation
{
public:
    explicit GeoLifeVisualistation(std::string dataSetPath) : m_dataSetPath(std::move(dataSetPath)) {}

    void run()
    {
        initWindow();
        initVulkan();
        m_ui = Ui(m_window, m_surface, m_instance, m_physicalDevice, m_device, m_queueFamilyIndices.graphicsAndComputeFamily.value(), m_graphicsQueue, m_renderPass, MAX_FRAMES_IN_FLIGHT);
        m_ubo.numberOfWorkGroups = m_numberOfWorkGroups;
        glfwSetScrollCallback(m_window, scrollCallback);
        mainLoop();
        cleanup();
    }

private:
    ////// MEMBERS: //////
    // GLFW:
    const uint32_t                  WIDTH = 1000;
    const uint32_t                  HEIGHT = 1000;
    GLFWwindow*                     m_window;


    // Vulkan basics
    VkInstance                      m_instance;
    VkDebugUtilsMessengerEXT        m_debugMessenger;
    VkPhysicalDevice                m_physicalDevice = VK_NULL_HANDLE; // pyhsical device
    VkDevice                        m_device; // logical device
    VkQueue                         m_graphicsQueue;
    VkQueue                         m_presentQueue;
    QueueFamilyIndices              m_queueFamilyIndices;
    VkSurfaceKHR                    m_surface;
    unsigned int                    m_maxWorkgroupInvocation = 1;
    
    // Swap chain:
    VkSwapchainKHR                  m_swapChain;
    std::vector<VkImage>            m_swapChainImages;
    VkFormat                        m_swapChainImageFormat;
    VkExtent2D                      m_swapChainExtent;
    std::vector<VkImageView>        m_swapChainImageViews; // Image views make images in the swap chain accesible targets e.g. as textures or as render buffers when framebuffer is set up
    const std::vector<const char*>  m_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // logical device extension required for swapchain
    std::vector<VkFramebuffer>      m_swapChainFramebuffers;
    bool                            m_frameBufferResized = false;
   
    // Rendering & pipeline
    VkRenderPass                    m_renderPass;
    VkDescriptorSetLayout           m_descriptorSetLayoutForGraphics; // RTVIS:texture and KDE Sampler
    VkDescriptorSetLayout           m_descriptorSetLayoutForColorMap; // Layout for Descriptorset holding only color bar for UI
    VkPipelineLayout                m_graphicsPipelineLayout;
    VkPipeline                      m_graphicsPipeline;
    
    //Command Buffers
    VkCommandPool                   m_commandPool;
    std::vector<VkCommandBuffer>    m_commandBuffersGraphics;

    // Semaphores and fences
    std::vector<VkSemaphore>        m_imageAvailableSemaphores;
    std::vector<VkSemaphore>        m_renderFinishedSemaphores;
    std::vector<VkFence>            m_inFlightFences;
    const int                       MAX_FRAMES_IN_FLIGHT = 1; // Ensure that GPU and CPU each have a frame to work on without creating waiting times
    int32_t                         m_currentFrame = 0; 

    // Vertex Buffer
    VkBuffer                        m_vertexBuffer;
    VkDeviceMemory                  m_vertexBufferMemory;
    VkBuffer                        m_indexBufferForQuad;
    VkDeviceMemory                  m_indexBufferMemoryForQuad;

    // Uniform buffer objects: one buffer for each frame in flight.
    std::vector<VkBuffer>           m_uniformBuffers;
    std::vector<VkBuffer>           m_kernelUniformBuffers;
    std::vector<VkDeviceMemory>     m_uniformBuffersMemory;
    std::vector<VkDeviceMemory>     m_kernelUniformBuffersMemory;
    std::vector<void*>              m_uniformBuffersMapped;
    std::vector<void*>              m_kernelUniformBuffersMapped;
    VkDescriptorPool                m_descriptorPoolGraphics;
    std::vector<VkDescriptorSet>    m_descriptorSetsGraphics;
    VkDescriptorSet                 m_descriptorSetColorbar; // Seperate Descriptor set for color bar texture required by UI
    
    // Host data to be written to ubos
    std::string m_dataSetPath;
    GeoLife::UserParameters m_ubo;
    glm::vec4 m_kernelUbo[121];
    unsigned int m_numberOfDatapointsLoaded;

    // Textures & Images
    struct Texture
    {
        std::string filePath;
        int texWidth;
        int texHeight;
        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;        
    };
    Texture m_cityMapTexture, m_colorMapTexture;
    VkSampler m_textureSampler;

    // Validation Layers
    const std::vector<const char*>  validationLayers = { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    // Ui
    Ui m_ui;


    ////// METHODS: //////
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        // fourth parameter:optionally specify a monitor for  window, last parameter only for OpenGL.
        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        // Register this class as user of the window so it can be referenced as "app" in the Resize Callback below
        glfwSetWindowUserPointer(m_window, this);
        
        // Register Callback  for resizing
        glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);

        // Register Callback for Mouse interaction:
        glfwSetMouseButtonCallback(m_window, mouseCallbackFunction);
    }
        // Callback Function for window resize
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height) 
        {
            auto app = reinterpret_cast<GeoLifeVisualistation*>(glfwGetWindowUserPointer(window));
            app->m_frameBufferResized = true;
        }

        // Callback for mouse interaction
        static void mouseCallbackFunction (GLFWwindow* glfw_window, int button, int action, int mods)
            {
                if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
                {
                    global_leftMouseButtonReleased = true;
                }
                else global_leftMouseButtonReleased = false;
            };

        
    void initVulkan() 
    {
        // Vulkan Basics: Drawing a textured quad
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViewsInSwapChain();
        createRenderPass();
        createDescriptorSetLayoutForCompute();
        createDescriptorSetLayoutForGraphics(); // Configures the uniform buffer
        createGraphicsPipeline();
        createFramebuffers(); //creates framebuffers for every image in the swap chain
        createCommandPool();
        m_cityMapTexture.filePath = "textures/mapTexture.png";
        m_colorMapTexture.filePath = "textures/colorMap.png";
        createTextureImage(m_cityMapTexture);
        createTextureImage(m_colorMapTexture);
        createImageViewTexture(m_cityMapTexture);
        createImageViewTexture(m_colorMapTexture);
        createSampler(m_textureSampler);
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPoolForGraphics(); // DescriptorSets Grpahics must be created later after Computer Descriptor sets
        createDescriptorSetsGraphics();
        createCommandBuffers();
        createSyncObjectsGraphics();

        //RTVIS:
        createSSBOs();
        createStorageImagesKDE(m_cityMapTexture.texWidth, m_cityMapTexture.texHeight);
        createImageViewsKDEStorage();
        createImageViewsKDESampler(); // BRauchen wir vielleicht garnicht
        createDescriptorPoolForCompute();
        createDescriptorSetsCompute();
        createComputePipeline();
        createCommandBuffersCompute();
    }
  
    // Create m_instance from ApplicationInfo (incl. ValidationLayerInfo) and Extensions (required by glfw & Validation layers)
    void createInstance()
    {
        //Create VkApplicationInfo
        // This data is technically optional, but it may provide some useful information to the driver
        // We don't need th member pNext and thus let it auto initiliaze to nullPtr;
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "GeoLife Visualisation";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Prepare Vulkan Extensions that connect Vulkan to glfw windows system and extensions for debug with validation layers
        std::vector<const char*>  extensions = getRequiredExtensions();

        // Create VkInstanceCreateInfo
        // tells the Vulkan driver which global extensions and validation layers we want to use
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = 0; //determine the global validation layers to enable, will be changed below

        // Portability: on macOS (MoltenVK) devices are only enumerated if VK_KHR_portability_enumeration is enabled
        for (const char* extension : extensions)
            if (strcmp(extension, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
                createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        // Add Validation layers for debugging to InstanceCreateInfo
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            // Validation layers: Check if available
            if(!checkValidationLayerSupport()) throw std::runtime_error("ERROR: validation layers requested, but not available!");
        
            //Validation Layers: Extend Vulkan Struct VkInstanceCreateInfo
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            
            // Add Validation layer debugging as pnext such that it is already used in the process of instance creation not only after that
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }

        //Create Vulkan Instance
        // Arguments:creation info, custom allocator callbacks (here null), handle to the new instance object
        VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        if (result != VK_SUCCESS) throw std::runtime_error("\nERROR: Failed to create instance!");
    }
        
        //Helper Functions:
        std::vector<const char*> getRequiredExtensions()
        {
            // Extra Info: check & print which extensions are available:
            uint32_t availableExtensionsCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, nullptr); // get number of available extensions
            std::vector<VkExtensionProperties> availableExtensions(availableExtensionsCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, availableExtensions.data()); // get info about available extensions
            std::cout << "\nAvailable extensions:\n";
            for (const auto& extension : availableExtensions) std::cout << '\t' << extension.extensionName << '\n';

            //glfw outputs the extensions it requires
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            // create vector for listing the required extensions and add glfw extensions
            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            // Add extension for debugging with validation layers 
            if (enableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Macro adds "VK_EXT_debug_utils" extensions for debugging
            }

            // Portability: enable VK_KHR_portability_enumeration if available (required for MoltenVK on macOS)
            for (const auto& extension : availableExtensions)
                if (strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
                    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

            // return list of extensions we need
            return extensions;
        }
        bool checkValidationLayerSupport() 
        {
            // Number and names info of available validation layers
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            // compare obtained info to my required layer
            for (const char* layerName : validationLayers) 
            {
                for (const auto& layerProperties : availableLayers)
                    if (strcmp(layerName, layerProperties.layerName) == 0) return true; // layer found
                return false; // layer not found
            }
        }
        void setupDebugMessenger() 
        {
            if (!enableValidationLayers) return;

            // Vulkan create Info for Debug Messenger
            VkDebugUtilsMessengerCreateInfoEXT createInfo{};
            populateDebugMessengerCreateInfo(createInfo);

            // Create DebugMessenger for instance (Using helper function that findes address of necessary Extension
            // 3rd parameter is again the optional allocator callback
            if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
                throw std::runtime_error("\nERROR: Failed to set up debug messenger!");
        }
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) 
        {
            createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            // Specify all the types of severities you would like your callback to be called for:
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            // Which types of messages your callback is notified about:
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            // Specify the pointer to the callback function:
            createInfo.pfnUserCallback = debugCallback;
            // Optional Pointer (e.g. to HelloTriangleApplication class) passed to debugCallback Function
            createInfo.pUserData = nullptr; // Optional
        }
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
        {
            if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) 
            {
                // Message is important enough to show
                std::cerr << "Message is important enough to show \n";
            }
            std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        }
    
    // What is a surface?
    void createSurface() 
    {
        // Create surface connected to GLFW window
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: Failed to create window surface!");  
    }

    // Picks device that has grahics Queue, Presentation Queue, swapchain support (for our surface)
    void pickPhysicalDevice() 
    {
        // Get number of available graphics cards
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0) throw std::runtime_error("\nERROR: Failed to find GPUs with Vulkan support!");

        // List all available devices
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        // Check each device for suitability and pick first suitable device
        for (const auto& device : devices) 
            if (isDeviceSuitable(device))
            {
                m_physicalDevice = device;
                break;
            }
        if (m_physicalDevice == VK_NULL_HANDLE) 
            throw std::runtime_error("\nERROR: Failed to find a suitable GPU!");
    }
        // Helper Functions:
        bool isDeviceSuitable(VkPhysicalDevice device) 
        {
            // Get basic info: name, type and supported Vulkan version etc.
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            // Get advanced info: texture compression, 64 bit floats and multi viewport rendering
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            // Print some info
            std::cout << "\n----------\n GraphicsCard Info:";
            std::cout << "\n    Dedicated Device: " << (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
            std::cout << "\n    Max texture size: " << deviceProperties.limits.maxImageDimension2D;
            std::cout << "\n    Has geometry shader: " << deviceFeatures.geometryShader;
            std::cout << "\n----------";

            // Note: no geometry shader is used, so it is not required (MoltenVK on macOS does not support it)

            // Check for Queue Families that support graphics and for presentation (can be 2 families)
            QueueFamilyIndices indices = findQueueFamilies(device);
            m_queueFamilyIndices = indices;
            if (!indices.isComplete()) return false;

            // Check if required extensions (here: Swapchain) are available
            bool extensionsSupported = checkDeviceExtensionSupport(device);
            if (!extensionsSupported) return false;

            // Check if swapChain supports at least 1 image format and 1 presentation mode given our window surface 
            bool swapChainAdequate = false;
            if (extensionsSupported) 
            {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }
            if (!swapChainAdequate) return false;

            // Check if anistropic filtering is supported for texture sampling
            VkPhysicalDeviceFeatures supportedFeatures;
            vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
            bool anistropySupported = supportedFeatures.samplerAnisotropy;

            // All requirements should be fullfilled when reaching this line, returning true
            return indices.isComplete() && extensionsSupported && swapChainAdequate && anistropySupported;
        }
            
            QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
            {
                // Logic to find graphics queue family
                QueueFamilyIndices indices;

                // Get number of available queue families
                uint32_t queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

                // Get info about available queue families
                std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

                // Find a queue family that supports VK_QUEUE_GRAPHICS_BIT and VK_QUEUE_COMPUTE_BIT
                int i = 0;
                for (const auto& queueFamily : queueFamilies)
                {
                    // check if current queue family supports graphics and compute 
                    if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) indices.graphicsAndComputeFamily = i;

                    //check if current queue family supports presentation
                    VkBool32 presentSupport = false;
                    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
                    if (presentSupport) indices.presentFamily = i;
            
                    //If a queue family supporting  graphics and and one supporting presentation is found : done
                    if (indices.isComplete()) break;
                    i++;
                }
                return indices;
            }

            bool checkDeviceExtensionSupport(VkPhysicalDevice device) 
            // Check if extensions for swap chain support are available
            { 
                // Enumerate available Extensions
                uint32_t extensionCount;
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
                std::vector<VkExtensionProperties> availableExtensions(extensionCount);
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

                // Check if available extensions contain the required extensions (swap chain extension)
                std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());
                for (const auto& extension : availableExtensions)
                    requiredExtensions.erase(extension.extensionName);
                return requiredExtensions.empty();
            }
   
    // Create Logical device: use graphicsWithCompute and present queue and required Extensions (swapchain)
    //>> creates handles for each queue
    void createLogicalDevice()
    {
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsAndComputeFamily.value(), indices.presentFamily.value() };

        // Prepare Create Info for Queues of logical device
        float queuePriority = 1.0f; // priority for scheduling command buffer execution
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }
        
        // Define features of physical device that will be used by the logical device (empty atm) >> see method pickPhysicalDevice()
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE; // Usually every graphics card has anisotropic filtering

        // Prepare Create Info for logical device
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        // Add validation layer info to create info for logical device
        if (enableValidationLayers) 
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
            createInfo.enabledLayerCount = 0;

        // Enable Extensions (VK_KHR_swapchain extension) in Create Info of logical device
        std::vector<const char*> deviceExtensions = m_deviceExtensions;

        // Portability: VK_KHR_portability_subset must be enabled if the device supports it (MoltenVK on macOS)
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, availableExtensions.data());
        for (const auto& extension : availableExtensions)
            if (strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0)
                deviceExtensions.push_back("VK_KHR_portability_subset");

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Create logical device
        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: failed to create logical device!");
        
        // Retrieve queue handles for each queue family. Parameters: logical device, queue family, queue index, pointer  queue handle
        // Here: only 1 queue from this family >> use index 0.
        vkGetDeviceQueue(m_device, indices.graphicsAndComputeFamily.value(), 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    }

    //Create Swapchain: Input: Info about surface (format, present mode), how to handle images between multiple swapchains etc.
    // Output: m_swapChain, m_swapChainImages
    void createSwapChain()
    {
        // Setup properties of the swapchain
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice); // surface formats and present modes
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats); // srgb etc.
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes); // What todo if swap chain is full
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities); // resolution of output window

        // Save info to member variables
        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent; // resolution of output image /output window
        
        // How many pictures in the swap chain? Better take one more than default in case driver is busy
        // Note: capabilities.maxImageCount == 0 means no limit!
        uint32_t imageCount = 1 + swapChainSupport.capabilities.minImageCount;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
            imageCount = swapChainSupport.capabilities.maxImageCount;

        // Create Info for a new swap chain INput: m_surface
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // must be 1 for normal 2d screens
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // How image is used: we use as color attachment, can also be post processing buffer etc.
        
        // Create Info: how to handle swap chain images accros multiple queue families e.g. graphics queue and present queue:
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsAndComputeFamily.value(), indices.presentFamily.value() };
        if (indices.graphicsAndComputeFamily != indices.presentFamily) 
        {
            // Images can be used across multiple queue families without explicit ownership transfers.
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else 
        {
            // Exclusive owner ship of images by 1 queue for best performance  
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // No pretransform is applied ( no rotation etc.) for more options check "supportedTransforms" in "capabilities":
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // No blending with other windows:
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        // Clipping in presentmode = care about pixels  that are overlapped by other windows?
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        //How handle for old swap chain in case of swap chain recreation e.g. when window resized
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        //Create Swapchain using the create Info:
        if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) 
            throw std::runtime_error("\nERROR: Failed to create swap chain!");

        // Set up vector with handles for images in swap chain
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

    }
        //Helper Functions:
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
        {
            SwapChainSupportDetails details;

            // Query basic surface capabilities that the device atogether with the selected surface provide
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

            // Query supported formats:
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
            if (formatCount != 0)
            {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
            }

            // Query supported presentation modes:
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
            if (presentModeCount != 0)
            {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
            }

            return details;
        }
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
        {
            for (const auto& availableFormat : availableFormats)
            {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    return availableFormat;
            }
            return availableFormats[0];
        }
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
        {
            for (const auto& availablePresentMode : availablePresentModes)
            {
                // If queue is full substititute latest image to show
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
            }

            // USe default option: if queue is full wait until space available
            return VK_PRESENT_MODE_FIFO_KHR;
        }
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
        {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
                return capabilities.currentExtent;
            else
            {
                // Vulkan measures in pixel, glfw uses screen coordinates  >> calculate correct size of window in pixel
                int width, height;
                glfwGetFramebufferSize(m_window, &width, &height);
                VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
                actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
                actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
                return actualExtent;
            }
        }
    
    // Configure Images in Swap chain
    void createImageViewsInSwapChain() 
    {
        // Resize vector to number of images in swap chain
        m_swapChainImageViews.resize(m_swapChainImages.size());

        // Iterate over swap chain images and create Imageview for each
        for (size_t i = 0; i < m_swapChainImages.size(); i++) 
            m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat);
    }

    // General Helper: creates Image views e.g. for textures or the swap chain images
    // Defines mimap leves etc
    VkImageView createImageView(VkImage image, VkFormat format)
    {
        // Set up create Info for Image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;

        // Add info about purpose of image view and which part of image is accessed
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        // Create ImageView
        VkImageView imageView;
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: Failed to create texture image view!");
        return imageView;
    }

    // Create Render Pass: Define subpasses,bindingPoint = graphics, wait on swapChain finish reading before writing etc.
    void createRenderPass()
    {
        // Configure attached buffer for rendering (renderpass) We have only one attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_swapChainImageFormat; // must match swapchain format
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // What to do with buffer before rendering starts (here: clear to black)
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // what happens after the rendering to the buffer (here we just store the rendered values)
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // What happens with stencil buffer before rendering (don care since not used)
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // What happens to stencil buffer after rendering: dont care . not used)
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Layout of imagein buffer before rendering (doesnt matter since we clear it)
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Layout of image after rendering: ready for presentation using swapchain
        
        // Configure subpasses e.g. for successive post processing (we only have 1 step anyway)
        VkAttachmentReference colorAttachmentRef{}; // Reference to one of the previously defined attached buffers (we have only 1) in the attachment descriptions array
        colorAttachmentRef.attachment = 0; // index of attachment in attachment array 
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // optimal format

        // Create 1 subpass cotaining only our previously defined attached buffer
        // The index of the attachment in this array is directly referenced from the fragment shader 
        // with the layout(location = 0) out vec4 outColor directive!
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // this is not a compute shader pass but a graphics pass
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // Sub Pass dependencies:
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // refers to implicit subpass at beginning and end of the a render pass
        dependency.dstSubpass = 0; // We have only one subpass with index 0
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // wait on swap chain to finish reading from image before accessing it
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // specifies which part of the color attachment should wait
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Create Info Render Pass
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        // Create render pass
        if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) throw std::runtime_error("\nERROR: Failed to create render pass!");
    }

    // Create Layout of buffers: uniform and Images, for RTVIS: images and SSBOs
    // Layout will be passed on to the pipeline
    void createDescriptorSetLayoutForGraphics() 
    {
        // Layout Binding for the map texture sampler 
        VkDescriptorSetLayoutBinding cityMapTextureLayoutBinding{}; 
        cityMapTextureLayoutBinding.binding = 0;
        cityMapTextureLayoutBinding.descriptorCount = 1;
        cityMapTextureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        cityMapTextureLayoutBinding.pImmutableSamplers = nullptr;
        cityMapTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        // Layout Binding for the color map
        VkDescriptorSetLayoutBinding colorMapTextureLayoutBinding{};
        colorMapTextureLayoutBinding.binding = 1;
        colorMapTextureLayoutBinding.descriptorCount = 1;
        colorMapTextureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        colorMapTextureLayoutBinding.pImmutableSamplers = nullptr;
        colorMapTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        // Create decriptor set layout for the texture sampler and the KDE Sampler
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { cityMapTextureLayoutBinding, colorMapTextureLayoutBinding };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayoutForGraphics) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: Failed to create descriptor set layout!"); 

        // Create decriptor set layout for the color map in UI
        VkDescriptorSetLayoutBinding colorBarTextureLayoutBinding = cityMapTextureLayoutBinding;
        colorBarTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        std::array<VkDescriptorSetLayoutBinding, 1> bindings_colorMap = { colorBarTextureLayoutBinding };
        VkDescriptorSetLayoutCreateInfo layoutInfo_colorMap{};
        layoutInfo_colorMap.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo_colorMap.bindingCount = static_cast<uint32_t>(bindings_colorMap.size());
        layoutInfo_colorMap.pBindings = bindings_colorMap.data();
        if (vkCreateDescriptorSetLayout(m_device, &layoutInfo_colorMap, nullptr, &m_descriptorSetLayoutForColorMap) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: Failed to create descriptor set layout!");


    }

    // Create Graphics pipeline from shaders, define vertex input structure, viewpoert/scissors, Rasterizer Settings, Blending
    void createGraphicsPipeline() 
    {
        // Read precompiled buffers from file
        auto vertShaderCode = readFile("vert.spv");
        auto fragShaderCode = readFile("frag.spv");

        // Create Shader Modules from Shader Code
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        // Setup Create info for Vertex Shader Stage from Vertex Shader Module
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // Define "Entry Point" allowing to combine multiple frag shaders
        vertShaderStageInfo.pSpecializationInfo = nullptr; // allows to set variables inside the shader

        // Setup create info for fragment shader:
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        // Save shader stages in array
        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // Create info dynamic state: about which parts of the pipeline are editable after creating it:
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Create Info Vertix input: about format of vertices passed to vertex shader
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1; //Bindings: spacing between data and whether the data is per - vertex or per - instance(see instancing)
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // Create info for Input Assembly: How to interpret vertices: points, line-list etc.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Create info Viewports and scissors: scissor rectangles define in which regions pixels will actually be stored. 
        // Any pixels outside the scissor rectangles will be discarded
        // Assuming viewport and siccossrs are dynamic state, see above
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_swapChainExtent.width;
        viewport.height = (float)m_swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapChainExtent;
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        
        // Fixed State viewport and sciccors:
        /*
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        */

        // Create Info Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE; // Clamps fragments outside near/far plane rather than discrding, requires GPU feature enabled
        rasterizer.rasterizerDiscardEnable = VK_FALSE; // Lets no geometry through the rasterizer (disables rendering)
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // FILL, LINE or POINT
        rasterizer.lineWidth = 1.0f; // lines wider than 1 require wideLines GPU feature
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; // Face Culling: front back or nothing
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Set up order of vertices to be culled (or VK_FRONT_FACE_CLOCKWISE) because of the Y-flip we did in the projection matrix, the vertices are now being drawn in counter-clockwis
        rasterizer.depthBiasEnable = VK_FALSE; // Manipulate depth value based on fragements slope etc
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // Create Info Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional Is this depth and stencil test?
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        // CreateInfo color blending: 
        // VkPipelineColorBlendAttachmentState per attached frameBuffer, VkPipelineColorBlendStateCreateInfo for global 
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        // Alternative color blending setup:
        /* 
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        */
        // Second option for blending (see comment above)
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // Create Info Pipline Layout
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ m_descriptorSetLayoutForGraphics , m_descriptorSetLayoutForCompute };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size(); // We have 1 descriptor set, created in function "createDescriptorSetLayout" above
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional Pus Constants are faster for small information that is updated frequently
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        // Create Pipeline Layout
        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_graphicsPipelineLayout) != VK_SUCCESS) throw std::runtime_error("\nERROR: Failed to create pipeline layout!");

        // Create Info pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_graphicsPipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional for the case that we inherit from a parent pipeline (requires VK_PIPELINE_CREATE_DERIVATIVE_BIT)
        pipelineInfo.basePipelineIndex = -1; // Optional

        // Create grphics pipeline
        // THis function is meant to create multiple pipelines. 2nd argument is for VkPipelineCache, storing data that is reused for multiple pipline creation
        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) throw std::runtime_error("\nERROR: Failed to create graphics pipeline!");

        // Final Step: Destroy Shader Modules since not needed after pipeline creation
        vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    }
        // Helper Functions
        static std::vector<char> readFile(const std::string& filename) 
        {
            // Open file, ate = start reading at end of file (see next comment), binary = read as binary
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) throw std::runtime_error("\nERROR: Failed to open file " + filename);

            // Use read position at end to determine size of file to allocate a buffer
            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            // Jump to start of file and read into buffer
            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();
            if (file.is_open()) throw std::runtime_error("\nERROR: Failed to open file " + filename);
            return buffer;
        }
        VkShaderModule createShaderModule(const std::vector<char>& code) 
        {
            // Setup Create Info from input
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.size(); 
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // code.data is stored in byte (char) but must be uint32
        
            // Create Shader Module
            VkShaderModule shaderModule;
            if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) throw std::runtime_error("failed to create shader module!");
            return shaderModule;
        }
   
    // General function to create and bind buffers e.g. the vertex buffer below    
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer!");
        
        // Get memory requirements for the vertex buffer. 
        /*
        Attributes are :
        .size: The size of the required amount of memory in bytes, may differ from bufferInfo.size.
        .alignment: The offset in bytes where the buffer begins in the allocated region of memory, depends on bufferInfo.usage and bufferInfo.flags.
        .memoryTypeBits : Bit field of the memory types that are suitable for the buffer.
        */
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

        // Allocate memory to buffer
        /* 
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT mean
        We use host coherent memory to ensure data is instantly visible in the buffer, this appraoch is a bit slower though
        */
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        // Bind vertexbuffer to allocated memory
        // Last argument is offset, must be multiple of memRequirements.alignment
        vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
    }

        //Framebuffers for images in swap chain for each render pass
    void createFramebuffers() 
    {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
        for (size_t i = 0; i < m_swapChainImageViews.size(); i++) 
        {
            // For each imageview: Create frame buffer attachments (here only one)
            VkImageView attachments[] = { m_swapChainImageViews[i] };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_swapChainExtent.width;
            framebufferInfo.height = m_swapChainExtent.height;
            framebufferInfo.layers = 1;
            if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) throw std::runtime_error("failed to create framebuffer!");
        }

    }

    // Creates Vertex buffer used by graphics queue
    void createVertexBuffer(bool useIndices = true)
    {
        // Create a  staging buffer: VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BI means we use host coherent buffer
        // it can be accessed from CPU but is a bit slower
        //VK_BUFFER_USAGE_TRANSFER_SRC_BIT: Buffer can be used as source in a memory transfer operation
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        // Map vertex data into cpu memory of staging buffer
        /*
        // offset and size here are 0 and bufferInfo.size, respectively. 
        // It is also possible to specify the special value VK_WHOLE_SIZE to map all of the memory.
        // 5th parameter must be 0 (flag not used yet by API)
        */
        void* data;
        vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);

        // Copy data to cpu memory of staging buffer
        memcpy(data, vertices.data(), (size_t)bufferSize);
        vkUnmapMemory(m_device, stagingBufferMemory);

        // Create faster vertex buffer (not accessible von cpu but from staging buffer)
        // VK_BUFFER_USAGE_TRANSFER_DST_BIT: Buffer can be used as destination in a memory transfer operation. 
        // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: Buffer not accessible from CPU
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);
        
        // Copy data from staging buffer to vertex buffer
        copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
    }
        
        //Helper: find the right type of memory for our requirements and hardware's requirements
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) 
        {
            VkPhysicalDeviceMemoryProperties memProperties; // Has two attributes: memoryTypes (types in every heap) and memoryHeaps (E.g. VRAM and RAM)
            vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

            // Check 1) if memory matches typeFilter and 2) if memory is visible and mapable from CPU
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) 
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
                    return i;

            throw std::runtime_error("failed to find suitable memory type!");
        }

        // Helper: Copy data from staging buffer to vertex buffer
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
        {
            // Create a comand buffer that contains the memory copy commands, begin recording Buffer
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();

            // Recording to Buffer: copy data from source (staging buffer) to destination (vertex buffer)
            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0; // Optional
            copyRegion.dstOffset = 0; // Optional
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

            // Finsih recording and submit recorded command buffer (that copies from staging to vertex buffer) to graphics Queue
            endSingleTimeCommands(commandBuffer);
        }

            // Helper: Record/ end single time Command buffers , also used in other functions
            VkCommandBuffer beginSingleTimeCommands() 
            {
                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandPool = m_commandPool;
                allocInfo.commandBufferCount = 1;
                VkCommandBuffer newCommandBuffer;
                vkAllocateCommandBuffers(m_device, &allocInfo, &newCommandBuffer);

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

                vkBeginCommandBuffer(newCommandBuffer, &beginInfo);

                return newCommandBuffer;
            }
            void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
                vkEndCommandBuffer(commandBuffer);

                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;

                vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
                
                // Wait until copying is finished , could also be done with a fence
                vkQueueWaitIdle(m_graphicsQueue);

                vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
            }
    
    // Create buffer for indices almost same as createVertexBuffer above!
    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indicesForQuad[0]) * indicesForQuad.size();
        
        //Create (temporary) Staging Buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Write data to staging buffer
        void* data;
        vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indicesForQuad.data(), (size_t)bufferSize);
        vkUnmapMemory(m_device, stagingBufferMemory);

        // Create local index buffer (not accessble from CPU but faster) and copy data
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBufferForQuad, m_indexBufferMemoryForQuad);
        copyBuffer(stagingBuffer, m_indexBufferForQuad, bufferSize);

        // Destroy temporary staging buffer
        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);
    }

    // General Purpose Function for creating images e.g. for the texture-image creation:
    // Configure image, create, allocate memory and bind to handle
    void createImage
    (
        uint32_t width, uint32_t height, 
        VkFormat format, 
        VkImageTiling tiling, 
        VkImageUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkImage& image, 
        VkDeviceMemory& imageMemory, 
        VkSharingMode vkSharingMode = VK_SHARING_MODE_EXCLUSIVE, 
        std::vector<uint32_t> queueFamilyIndices = std::vector<uint32_t>()
    )
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        
        // One dimensional images can be used to store an array of data or gradient, two dimensional images are mainly used for textures, 
        // and three dimensional images can be used to store voxel volumes,
        imageInfo.imageType = VK_IMAGE_TYPE_2D; 
       
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
       
        // use the same format for the texels as the pixels in the buffer, otherwise the copy operation will fail.
        imageInfo.format = format; 
        
        // Cannot be changed later, VK_IMAGE_TILING_LINEAR allows to acces pixels directly, 
        // VK_IMAGE_TILING_OPTIMAL is faster but no acces to pixels >> can be used when staginf buffer exists.
        imageInfo.tiling = tiling;
        
        // VK_IMAGE_LAYOUT_UNDEFINED: Not usable by the GPU and the very first transition will discard the texels.
        // VK_IMAGE_LAYOUT_PREINITIALIZED: Not usable by the GPU, but the first transition will preserve the texels >> only needed when used as staging image
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // >> related to multi sampling
        imageInfo.sharingMode = vkSharingMode;
        imageInfo.flags = 0; // Optional for sparse images that do not need the whole memory

        // If this image is used in more than one pipeline we must configure it accordingly
        /* So far we use one family for presentation and one family for compute and graphics.
         * Thus we do not need vkSharingMode = VK_SHARING_MODE_CONCURRENT but if we and seperates queues for compute and graphics
         * it would be neccessary
         */
        if(vkSharingMode == VK_SHARING_MODE_CONCURRENT && queueFamilyIndices.size() < 2)
            throw std::runtime_error("ERROR: createImage() obtained VK_SHARING_MODE_CONCURRENT but less than 2 queue indices!");
        if (vkSharingMode == VK_SHARING_MODE_CONCURRENT)
        {
            imageInfo.queueFamilyIndexCount = queueFamilyIndices.size();
            imageInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }

        // Create the image
        if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) 
            throw std::runtime_error("failed to create image!");

        // Get memory requirement of the image
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);

        // Allocate Memory for the image
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory!");
        
        // Bind image to allocated memory
        vkBindImageMemory(m_device, image, imageMemory, 0);
    }

    // Command Pool for graphics queue 
    void createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

        // Create Info for command Pool for graphics queue
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // We want to rerecord commands at every frame
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value(); // we use graphics queue, command pools can only allocate command buffers for a single type of queue

        // Create Command Pool
        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) throw std::runtime_error("failed to create command pool!");
    }

    // load an image and upload it into a Vulkan image object
    // Load image from file, copy to staging buffer, call createImage
    void createTextureImage(Texture& tex)
    {
        // Load image with library in pixels object
        /* 
        The STBI_rgb_alpha value forces the image to be loaded with an alpha channel, even if it doesn't have one
        The pointer that is returned is the first element in an array of pixel values.
        The pixels are laid out row by row with 4 bytes per pixel in the case of STBI_rgb_alpha for a total of texWidth* texHeight * 4 values.
        */
        int texChannels;
        stbi_uc* pixels = stbi_load(tex.filePath.c_str(), &tex.texWidth, &tex.texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = tex.texWidth * tex.texHeight * 4;
        if (!pixels) 
            throw std::runtime_error("\nERROR: Failed to load texture image from " + tex.filePath);

        // create a staging buffer in host visible memory so that we can use vkMapMemory and copy the pixels to it.
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        void* data;
        vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(m_device, stagingBufferMemory);

        // Clean up STBI storage
        stbi_image_free(pixels);

        // Allocates and binds memory accessed by m_textureImage
        createImage(tex.texWidth, tex.texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex.textureImage, tex.textureImageMemory);
        
        // Configure Image Layout
        /*
            The image was created with the VK_IMAGE_LAYOUT_UNDEFINED layout, so that one should be specified as old layout when transitioning textureImage.
            Remember that we can do this because we don't care about its contents before performing the copy operation.
        */
        transitionImageLayout(tex.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        
        //copy data from staging buffer to image
        copyBufferToImage(stagingBuffer, tex.textureImage, static_cast<uint32_t>(tex.texWidth), static_cast<uint32_t>(tex.texHeight));

        /*To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access */
        transitionImageLayout(tex.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingBufferMemory, nullptr);
    }
        
    //Helper: Bring image into correct layout using barriers
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) 
        {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();
            // Use image memory barrier:
            /*
            One of the most common ways to perform layout transitions is using an image memory barrier.
            A pipeline barrier like that is generally used to synchronize access to resources, like ensuring that a write to a buffer completes before reading from it, 
            but it can also be used to transition image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used.
            There is an equivalent buffer memory barrier to do this for buffers.
            */
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; //If you are using the barrier to transfer queue family ownership, then these two fields should be the indices of the queue families.
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            //Handle Transitions:
            /*
                Undefined → transfer destination : transfer writes that don't need to wait on anything
                Transfer destination → shader reading : shader reads should wait on transfer writes, specifically the shader reads in the fragment shader, because that's where we're going to use the texture
                These rules are specified using the following access masks and pipeline stages
            */
            VkPipelineStageFlags sourceStage;
            VkPipelineStageFlags destinationStage;
            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
            {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) 
            {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
            {
                barrier.srcAccessMask = 0;
                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }
            else 
            {
                throw std::invalid_argument("\nERROR: Unsupported layout transition!");
            }

            /*
            All types of pipeline barriers are submitted using the same function.
            The first parameter after the command buffer specifies in which pipeline stage the operations occur that should happen before the barrier.
            The second parameter specifies the pipeline stage in which operations will wait on the barrier.
            The pipeline stages that you are allowed to specify before and after the barrier depend on how you use the resource before and after the barrier.
            The third parameter is either 0 or VK_DEPENDENCY_BY_REGION_BIT. The latter turns the barrier into a per-region condition. 
            That means that the implementation is allowed to already begin reading from the parts of a resource that were written so far, for example.
            The last three pairs of parameters reference arrays of pipeline barriers of the three available types: memory barriers, buffer memory barriers, and image memory barriers like the one we're using here.
            */
            vkCmdPipelineBarrier(
                commandBuffer,
                sourceStage, destinationStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
            
            endSingleTimeCommands(commandBuffer);
        }

        // Helper: 
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) 
        {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();

            // Configure image region
            VkBufferImageCopy region{};
            region.bufferOffset = 0; //specifies the byte offset in the buffer at which the pixel values start
            region.bufferRowLength = 0; //specify how the pixels are laid out in memory.  0 for both indicates that the pixels are simply tightly packed 
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Specify which part of image is copied (subresource, Offset, Extent)
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = 
            {
                width,
                height,
                1
            };

            // Copy the image: The fourth parameter indicates which layout the image is currently using, 
            // assuming here that the image has already been transitioned to the layout that is optimal for copying pixels to.
            vkCmdCopyBufferToImage
            (
                commandBuffer,
                buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region
            );

            endSingleTimeCommands(commandBuffer);
        }
    
    // Creates Image view for textures using helper
    void createImageViewTexture(Texture& tex)
    {
        tex.textureImageView = createImageView(tex.textureImage, VK_FORMAT_R8G8B8A8_SRGB);
    }

    void createSampler(VkSampler& IO_sampler)
    {
        // General Sampler Settings
        //addressMode:
        /*
        VK_SAMPLER_ADDRESS_MODE_REPEAT: Repeat the texture when going beyond the image dimensions.
        VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : Take the color of the edge closest to the coordinate beyond the image dimensions.
        VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE : Like clamp to edge, but instead uses the edge opposite to the closest edge.
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER : Return a solid color when sampling beyond the dimensions of the image.
        */
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST; // VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.unnormalizedCoordinates = VK_FALSE; // use [0, texWidth) and [0, texHeight) or [0,1) coordinates on the texture
        samplerInfo.compareEnable = VK_FALSE; // Mainly for percentage-closer-filtering: Should texels be compared to a value before using smaple?
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;// = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        // Anisotropic filtering settings: Get the maximum setting from hardware
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &properties); // Gets a struct with all options and settings of the hardware
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

        // Create Sampler
        if (vkCreateSampler(m_device, &samplerInfo, nullptr, &IO_sampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture sampler!");
    }

    // Create primary or secondary comamnd buffer (from graphics queue command pool)
    void createCommandBuffers()
    {
        m_commandBuffersGraphics.resize(MAX_FRAMES_IN_FLIGHT);
        // Create Info Command Buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Primary: 
        allocInfo.commandBufferCount = (uint32_t) m_commandBuffersGraphics.size();

        // Create commandbuffer as primary buffer (Can be submitted to a queue for execution, cannot be called from other command buffers)
        // Secondary buffers can only be called by primary buffers, e.g. to re use commands
        if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffersGraphics.data()) != VK_SUCCESS) throw std::runtime_error("failed to allocate command buffers!");
    }

    void createSyncObjectsGraphics()
    {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        
        // Create Info Semaphore: Has almost no attributes so in current version
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // Create Info for fences: Signal when rendering is finsihed
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Signals fence right after creation so at first frame CPU does not wait

        // Create info for fences
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("\nERROR: Failed to create semaphores!");
            }
        }
    }

    void drawFrame()
    {
        if (m_ubo.runCompute)
        {
            // Submit compute command buffer if user changed something
            // Create Info for Submitting the command buffer for  compute and graphics
            VkSubmitInfo submitInfoCompute{};
            submitInfoCompute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            // Update UBOs with current user data and with new kernel
            updateUniformBuffers(m_currentFrame);

            // Submit Compute Work
            vkResetCommandBuffer(m_commandBuffersCompute[m_currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
            recordCommandBufferCompute(m_commandBuffersCompute[m_currentFrame], m_StorageImagesKDE[m_currentFrame]); // FRAGE: warum muss ihc jeden frame  neu record machen?
            submitInfoCompute.commandBufferCount = 1;
            submitInfoCompute.pCommandBuffers = &m_commandBuffersCompute[m_currentFrame];
            submitInfoCompute.signalSemaphoreCount = 0;
            if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfoCompute, VK_NULL_HANDLE) != VK_SUCCESS)
                throw std::runtime_error("\nERROR: failed to submit compute command buffer!");
        }

        // Fence. Wait for last rendering to finish
        // VK_TRUE = wait for all fences to signal (just 1 anyway)
        // UINT64_MAX = timeout for waiting, here almost infinite
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        // Acquire image from swap chain
        // Semaphore  is signaled when done, fence not used (VK_NULL_HANDLE). imageIndex is index in the swap chain
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

        // Check if present surface changed and recreate Swapchain in this case
        /*
        VK_ERROR_OUT_OF_DATE_KHR: The swap chain has become incompatible with the surface and can no longer be used for rendering.Usually happens after a window resize.
        VK_SUBOPTIMAL_KHR : The swap chain can still be used to successfully present to the surface, but the surface properties are no longer matched exactly.
        Both VK_SUCCESS and VK_SUBOPTIMAL_KHR are considered "success" return codes. so we only return if a real error (VK_ERROR_OUT_OF_DATE_KHR) occurs
        */
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("\nERROR: failed to acquire swap chain image!");
        }

        // Reset Fence after it was signaled and after we made sure that next image could be aquired!
        vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

        // Record Command Buffer
        vkResetCommandBuffer(m_commandBuffersGraphics[m_currentFrame], 0);
        recordCommandBufferGraphics(m_commandBuffersGraphics[m_currentFrame], imageIndex, vertices, m_StorageImagesKDE[m_currentFrame]);

        // Semaphores for rendering to wait for and to Signal. If we have 2 semaphores, we have to enlist VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT twice!
        VkSubmitInfo submitInfoRendering{};
        submitInfoRendering.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphoresRendering[] = {m_imageAvailableSemaphores[m_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfoRendering.waitSemaphoreCount = 1;
        submitInfoRendering.pWaitSemaphores = waitSemaphoresRendering; // these are semaphores for each stage in pWaitDstStageMask to wait for
        submitInfoRendering.pWaitDstStageMask = waitStages; // these are the stages that have to wait for the seamphores above (must be same size ans semphores array)
        submitInfoRendering.commandBufferCount = 1;
        submitInfoRendering.pCommandBuffers = &m_commandBuffersGraphics[m_currentFrame]; // Buffer with the actual comands to be submitted
        VkSemaphore signalSemaphoresRendering[] = { m_renderFinishedSemaphores[m_currentFrame] };
        submitInfoRendering.signalSemaphoreCount = 1;
        submitInfoRendering.pSignalSemaphores = signalSemaphoresRendering; // What semphore(s) to signal when command buffer execustion is finished

        // Submit Command buffer: state fence to be signaled when command buffer execution finished
        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfoRendering, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) throw std::runtime_error("\nERROR: failed to submit draw command buffer!");

        // put image back to the swap chain for presentation
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphoresRendering; // wait for this signal which means command buffer execution is finished
        VkSwapchainKHR swapChains[] = { m_swapChain }; // which swap chain is used for presentation
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex; // which image is presented?
        presentInfo.pResults = nullptr; // Optional for multiple swap chains: check which ones where successfull presenting
        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

        // Again check if presenting returns error because Swapchain is out of date nad update swapchain if needed.
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_frameBufferResized) {
            m_frameBufferResized = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("\nERROR: failed to present swap chain image!");
        }

        // Advance frame by one each time resulting in drawing to only one frame while the other is "being presented"
        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
        // Input: renderPass, swapChain Image, m_graphicsPipeline
        void recordCommandBufferGraphics(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::vector<Vertex> vertices, VkImage& image) 
        {
            //Create Info for command record, 
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0; // Optional for info about rerecording
            beginInfo.pInheritanceInfo = nullptr; // Optional for secondary buffer: specifies which state to inherit from the calling primary command buffers

            // Start recording (if arecording already started , it is reset)
            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) throw std::runtime_error("\nERROR: failed to begin recording command buffer!");

            // Configure a render pass
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_renderPass;
            renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex]; // which swap chain image is targeted?
            renderPassInfo.renderArea.offset = { 0, 0 }; // render area should match swap chain extent
            renderPassInfo.renderArea.extent = m_swapChainExtent;
            VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            // Begin the render pass using 'VK_SUBPASS_CONTENTS_INLINE' = no secondary buffers used
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Bind the pipeline
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            // Set viewport and siccor state since they were declared dynamic in the fixed function setup of the pipeline
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_swapChainExtent.width);
            viewport.height = static_cast<float>(m_swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = m_swapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Bind vertex buffer
            VkBuffer vertexBuffers[] = { m_vertexBuffer }; // array of vertex buffer to e bound
            VkDeviceSize offsets[] = { 0 }; // offsets to start reading vertex data from for each vertex buffer
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); // 2nd/3rd arg: offset and number of bindings

            // Bind descriptor set
            /*
            * Unlike vertex and index buffers, descriptor sets are not unique to graphics pipelines. 
            Therefore we need to specify if we want to bind descriptor sets to the graphics or compute pipeline. 
            The next parameter is the layout that the descriptors are based on. 
            The next three parameters specify the index of the first descriptor set, 
            the number of sets to bind, and the array of sets to bind. We'll get back to this in a moment.
            The last two parameters specify an array of offsets that are used for dynamic descriptors.
            */
            std::vector<VkDescriptorSet> descriptorSets{ m_descriptorSetsGraphics[m_currentFrame], m_descriptorSetsCompute[m_currentFrame] };
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

            if (useindices)
            {
                // Bind index buffer 3rd argument is offset = 0
                vkCmdBindIndexBuffer(commandBuffer, m_indexBufferForQuad, 0, VK_INDEX_TYPE_UINT16);

                // Draw quad with indices
                /*
                * Arguments:
                * 1 = instance, 0 = offset, 0 = offset to add to each index, 0 = offset for instancing
                */
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indicesForQuad.size()), 1, 0, 0, 0);
            }
            else
            {
                // Issue draw command with vertices
                /*
                Args:
                vertexCount: Even if we don't have a vertex buffer, we technically still have 3 vertices to draw.
                instanceCount: Used for instanced rendering, 1 if you're not doing that.
                firstVertex : Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
                firstInstance : Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
                */
                vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
            }

            // Draw the UI
            m_ui.recordCommandBufferForRendering(m_commandBuffersGraphics[m_currentFrame]);

            // Finish Render pass
            vkCmdEndRenderPass(commandBuffer);

            // Create Memory barrier that ensures that in the next frame the compute buffer can write          
            QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
            uint32_t graphAndCompQueueFamIdx = queueFamilyIndices.graphicsAndComputeFamily.value();
            VkImageMemoryBarrier  imageMemoryBarrierKDE{};
            imageMemoryBarrierKDE.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrierKDE.pNext = nullptr;
            imageMemoryBarrierKDE.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrierKDE.dstAccessMask = VK_ACCESS_NONE;
            imageMemoryBarrierKDE.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageMemoryBarrierKDE.newLayout = VK_IMAGE_LAYOUT_GENERAL;// VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL would be better but needs another transition after smapling
            imageMemoryBarrierKDE.srcQueueFamilyIndex = graphAndCompQueueFamIdx;
            imageMemoryBarrierKDE.dstQueueFamilyIndex = graphAndCompQueueFamIdx;
            imageMemoryBarrierKDE.image = image;
            imageMemoryBarrierKDE.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrierKDE.subresourceRange.baseMipLevel = 0;
            imageMemoryBarrierKDE.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            imageMemoryBarrierKDE.subresourceRange.baseArrayLayer = 0;
            imageMemoryBarrierKDE.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // Idea: This transition should be made before pipline ends
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrierKDE);

            // End recording the command buffer
            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) throw std::runtime_error("\nERROR: failed to record command buffer!");
        }
        void updateUniformBuffers(uint32_t currentImage) 
        {
            /* 
            Using a UBO this way is not the most efficient way to pass frequently changing values to the shader.
            A more efficient way to pass a small buffer of data to shaders are push constants.
            */
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            // Copy new matrices to memory
            memcpy(m_uniformBuffersMapped[currentImage], &m_ubo, sizeof(m_ubo));
            memcpy(m_kernelUniformBuffersMapped[currentImage], &m_kernelUbo, 121*sizeof(glm::vec4));
        }

    void recreateSwapChain() {
        // If window is minimized, framebuffer size = 0, wait until maximised
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        // wait until all jobs finsihed, then recreate SwapChain, Images and Framebuffer
        vkDeviceWaitIdle(m_device);
        cleanupSwapChain();
        createSwapChain();
        createImageViewsInSwapChain();
        createFramebuffers();
    }

    // Destroy Framebuffers, ImageViews & Swapchain
    void cleanupSwapChain() {
        for (size_t i = 0; i < m_swapChainFramebuffers.size(); i++) {
            vkDestroyFramebuffer(m_device, m_swapChainFramebuffers[i], nullptr);
        }

        for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
            vkDestroyImageView(m_device, m_swapChainImageViews[i], nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    }

    // Create uniform buffers handles, memory and memory mapping
    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(GeoLife::UserParameters);
        VkDeviceSize kernelBufferSize = sizeof(glm::vec4) * 121; // we use an 11 x11 matrix for our kernel

        m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_kernelUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_kernelUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        m_kernelUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        /* Create Mapping
        We map the buffer right after creation using vkMapMemory to get a pointer to which we can write the data later on.
        The buffer stays mapped to this pointer for the application's whole lifetime. This technique is called "persistent mapping" 
        and works on all Vulkan implementations. Not having to map the buffer every time we need to update it increases performances, as mapping is not free.
        */
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffers[i], m_uniformBuffersMemory[i]);
            createBuffer(kernelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_kernelUniformBuffers[i], m_kernelUniformBuffersMemory[i]);
            vkMapMemory(m_device, m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
            vkMapMemory(m_device, m_kernelUniformBuffersMemory[i], 0, kernelBufferSize, 0, &m_kernelUniformBuffersMapped[i]);
        }
    }

    // Create Descriptor Pool for graphics
    void createDescriptorPoolForGraphics() 
    {
        // Create Info for the pool: pool size = # frames in flight
        std::array<VkDescriptorPoolSize, 1> poolSizes{};

        // Info for texture sampler
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; 
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 3);  //RTVIS: one kdeImage sampler per frame + color bar for sampling + city map + colorbar in UI

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; 
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size()); 
        poolInfo.pPoolSizes = poolSizes.data(); 
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)+1; 

        // Create the descriptor set pool
        if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPoolGraphics) != VK_SUCCESS) 
            throw std::runtime_error("\nERROR: failed to create descriptor pool!");
    }

    // Create Descriptor sets for Graphics and update them
    void createDescriptorSetsGraphics() 
    {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayoutForGraphics);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPoolGraphics;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = descriptorSetLayouts.data();
        m_descriptorSetsGraphics.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSetsGraphics.data()) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: failed to allocate descriptor sets for Graphics!");
        
        // Image info to be updated on GPU (send to GPU)
        VkDescriptorImageInfo imageInfoCityMap{};
        imageInfoCityMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoCityMap.imageView = m_cityMapTexture.textureImageView;
        imageInfoCityMap.sampler = m_textureSampler;

        // Image info to be updated on GPU
        VkDescriptorImageInfo imageInfoColorMap{};
        imageInfoColorMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoColorMap.imageView = m_colorMapTexture.textureImageView;
        imageInfoColorMap.sampler = m_textureSampler;

        // Create descripter writers
        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        // city map texture sampler
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfoCityMap;
        //descriptorWrites[0].dstSet = m_descriptorSetsGraphics[i];

        // color map texture sampler
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfoColorMap;
        //descriptorWrites[1].dstSet = m_descriptorSetsGraphics[i];


        // Update descriptors in each descriptorset using the image info we just created
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            descriptorWrites[0].dstSet = m_descriptorSetsGraphics[i];
            descriptorWrites[1].dstSet = m_descriptorSetsGraphics[i];
            
            // execute update
            // "VkCopyDescriptorSet" can be used instead to copy descriptors to each other
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }  

        // Repeat whole process to create and update color bar descriptor set
        VkDescriptorSetAllocateInfo allocInfoColorBar = {};
        allocInfoColorBar.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfoColorBar.descriptorPool = m_descriptorPoolGraphics;
        allocInfoColorBar.descriptorSetCount = 1;
        allocInfoColorBar.pSetLayouts = &m_descriptorSetLayoutForColorMap;
        if (vkAllocateDescriptorSets(m_device, &allocInfoColorBar, &m_descriptorSetColorbar) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: failed to allocate descriptor sets for Color Bar  in Ui!");

        // Image info to be updated on GPU
        VkDescriptorImageInfo imageInfoColorBarForUi = {};
        imageInfoColorBarForUi.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoColorBarForUi.imageView = m_colorMapTexture.textureImageView;
        imageInfoColorBarForUi.sampler = m_textureSampler;

        // Create descripter writers
        VkWriteDescriptorSet descriptorWriteColorBar={};
        descriptorWriteColorBar.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWriteColorBar.dstBinding = 0;
        descriptorWriteColorBar.dstArrayElement = 0;
        descriptorWriteColorBar.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWriteColorBar.descriptorCount = 1;
        descriptorWriteColorBar.pImageInfo = &imageInfoColorBarForUi;
        descriptorWriteColorBar.dstSet = m_descriptorSetColorbar;

        // execute update with descriptor writer
        vkUpdateDescriptorSets(m_device, 1, &descriptorWriteColorBar, 0, nullptr);
    }


    //////////////
    //  RTVIS   //
    ////////////// 
    //RTVis: Create Descriptor Set for Compute Shader
    VkDescriptorSetLayout m_descriptorSetLayoutForCompute; //RTVIS: ubo with user data, dataset storage, storage image KDE, ubo kernel, ssbo sync ssbo kernel
    void createDescriptorSetLayoutForCompute()
    {
        // Layout Binding for the UBO: RTVIS: This UBO Contains unser input
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0; // specify the binding used in the shader
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1; // Our MVP transformation is in a single uniform buffer object, so we're using a descriptorCount of 1
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // Could also use VK_SHADER_STAGE_ALL_GRAPHICS for access in every shader
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional , relates to image sampling

        // RTVIS: layout binding for buffer containing the GeoLife Data
        VkDescriptorSetLayoutBinding storageGeoLifeDataLayoutBinding
        {
            /* binding            */ 1,
            /* descriptorType     */ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // Storage where the GeoLIfe Data is stored
            /* descriptorCount    */ 1,
            /* stageFlags         */ VK_SHADER_STAGE_COMPUTE_BIT,
            /* pImmutableSamplers */ nullptr
        };


        // RTVIS: layout binding for image that KDE is written to by Compute Shader
        VkDescriptorSetLayoutBinding storageImgKDELayoutBinding
        {
            /* binding            */ 2,
            /* descriptorType     */ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // JCR: image where KDE is written to
            /* descriptorCount    */ 1,
            /* stageFlags         */ VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            /* pImmutableSamplers */ nullptr
        };

        // RTVIS: layout binding for image where KDE is sampled from in frag shader
        VkDescriptorSetLayoutBinding samplerKDELayoutBinding
        {
            /* binding            */ 3,
            /* descriptorType     */ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //RTVIS: image where KDE are sampled from!
            /* descriptorCount    */ 1,
            /* stageFlags         */ VK_SHADER_STAGE_FRAGMENT_BIT, // bookmark
            /* pImmutableSamplers */ nullptr
        };

        // RTVIS: layout binding for buffer containing the GeoLife Data
        VkDescriptorSetLayoutBinding kernelUboLayoutBinding
        {
            /* binding            */ 4,
            /* descriptorType     */ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // Storage where the GeoLIfe Data is stored
            /* descriptorCount    */ 1,
            /* stageFlags         */ VK_SHADER_STAGE_COMPUTE_BIT,
            /* pImmutableSamplers */ nullptr
        };

        // RTVIS: layout binding for work group sync SSBO
        VkDescriptorSetLayoutBinding ssboSyncLayoutBinding
        {
            /* binding            */ 5,
            /* descriptorType     */ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            /* descriptorCount    */ 1,
            /* stageFlags         */ VK_SHADER_STAGE_COMPUTE_BIT,
            /* pImmutableSamplers */ nullptr
        };

        // RTVIS: layout binding for work group sync SSBO
        VkDescriptorSetLayoutBinding ssboKernelLayoutBinding
        {
            /* binding            */ 6,
            /* descriptorType     */ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            /* descriptorCount    */ 1,
            /* stageFlags         */ VK_SHADER_STAGE_COMPUTE_BIT,
            /* pImmutableSamplers */ nullptr
        };

        // Create decriptor set layout bindings for the descriptors announced above for the compute pipline 
        std::array<VkDescriptorSetLayoutBinding, 7> bindings = { uboLayoutBinding, storageGeoLifeDataLayoutBinding, storageImgKDELayoutBinding, samplerKDELayoutBinding, kernelUboLayoutBinding, ssboSyncLayoutBinding, ssboKernelLayoutBinding };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayoutForCompute) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: failed to create descriptor set layout!");
    }
   
    // Shader Storage buffers holding GeoLife data (read only, maybe better uniform buffers?
    VkBuffer m_ssboGeoLifeData;
    std::vector<VkBuffer> m_ssbosSync;
    std::vector<VkBuffer> m_ssbosKernel;
    VkDeviceMemory m_ssboMemoryGeoLifeData;
    std::vector <VkDeviceMemory> m_ssboMemoriesSync;
    std::vector <VkDeviceMemory> m_ssboMemoriesKernel;
    VkDeviceSize m_bufferSizeGeoLifeDataSet;
    VkDeviceSize m_bufferSizeSync;
    VkDeviceSize m_bufferSizeKernel;
    void createSSBOs()
    {        
        // Load GeoLifeDataSet instance which holds all the data 
        unsigned int numberOfDatapointsLoaded;
        GeoLife::GeoLifeDataSet geoLifeDataSet(m_dataSetPath, &numberOfDatapointsLoaded, 1.0); // last number tells how many percent of dataset is loaded 1.= = 100%
        if (numberOfDatapointsLoaded == 0)
            throw std::runtime_error("\nERROR: No data points loaded from " + m_dataSetPath + ". Pass the path to a GeoLife data set file as first argument.");
        m_ubo.maxIndexDataSet = numberOfDatapointsLoaded - 1;
        
        // Determine size of SSBOs
        m_bufferSizeGeoLifeDataSet = (VkDeviceSize)geoLifeDataSet.getDataSetSize();
        m_bufferSizeKernel = (VkDeviceSize)(121 * sizeof(glm::vec4));
        m_bufferSizeSync = (VkDeviceSize)sizeof(glm::uvec4);

        // Create Staging Buffer with memory for  GeoLife Dataset
        VkDeviceMemory stagingBufferMemoryGeoLifeData;
        VkBuffer stagingBufferGeoLifeData;
        
        // Create staging buffer for GeoLife Data
        createBuffer(m_bufferSizeGeoLifeDataSet, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferGeoLifeData, stagingBufferMemoryGeoLifeData);

        // Copy data from geoLife instance to staging uffer
        void* data; 
        vkMapMemory(m_device, stagingBufferMemoryGeoLifeData, 0, m_bufferSizeGeoLifeDataSet, 0, &data);
        memcpy(data, geoLifeDataSet.getDataSet()->data(), m_bufferSizeGeoLifeDataSet);
        vkUnmapMemory(m_device, stagingBufferMemoryGeoLifeData);

        // Create device local SSBO buffers
        createBuffer(m_bufferSizeGeoLifeDataSet, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssboGeoLifeData, m_ssboMemoryGeoLifeData);
        m_ssbosSync.resize(MAX_FRAMES_IN_FLIGHT);
        m_ssbosKernel.resize(MAX_FRAMES_IN_FLIGHT);
        m_ssboMemoriesSync.resize(MAX_FRAMES_IN_FLIGHT);
        m_ssboMemoriesKernel.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            createBuffer(m_bufferSizeSync, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssbosSync[i], m_ssboMemoriesSync[i]);
            createBuffer(m_bufferSizeKernel, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_ssbosKernel[i], m_ssboMemoriesKernel[i]);
        }
        // Copy data from the staging buffer (host visible) to the shader storage buffer (GPU)
        copyBuffer(stagingBufferGeoLifeData, m_ssboGeoLifeData, m_bufferSizeGeoLifeDataSet);

        // Destroy Staging Buffer and  free its memory
        vkDestroyBuffer(m_device, stagingBufferGeoLifeData, nullptr);
        vkFreeMemory(m_device, stagingBufferMemoryGeoLifeData, nullptr);
    }

    // Create Image for storing and sampling KDE
    std::vector<VkImage> m_StorageImagesKDE;
    std::vector<VkDeviceMemory> m_StorageImageKDEMemories;
    void createStorageImagesKDE(const unsigned int& texWidth, const unsigned int& texHeight)
    {
        // Allocates and binds memory accessed by m_StorageImage
        /*
        Take any single channel formats like VK_FORMAT_R8_UNORM for a 8 bit grayscale, VK_FORMAT_R16_SFLOAT for half precision floating point
        grayscale or VK_FORMAT_R32_SFLOAT for single precision floating point grayscale.
        */
        m_StorageImagesKDE.resize(MAX_FRAMES_IN_FLIGHT);
        m_StorageImageKDEMemories.resize(MAX_FRAMES_IN_FLIGHT);
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            createImage(texWidth, texHeight,
                VK_FORMAT_R32_SINT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_StorageImagesKDE[i], m_StorageImageKDEMemories[i]);

            /*To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access */
            transitionImageLayout(m_StorageImagesKDE[i], VK_FORMAT_R32_SINT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        }
    }

    // Create Image views for KDE Storages (We write to this Image view). This image view has only one channel
    std::vector<VkImageView> m_imageViewsKDEStorage;
    void createImageViewsKDEStorage()
    {
        m_imageViewsKDEStorage.resize(MAX_FRAMES_IN_FLIGHT);
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            m_imageViewsKDEStorage[i] = createImageView(m_StorageImagesKDE[i], VK_FORMAT_R32_SINT);
    }
   
    // RTVIS:Create Image Views for KDE Samplers //FRAGE: Brauch ich den überhaupt? GLaube nicht
    std::vector <VkImageView> m_imageViewsKDESampler; //RTVIS: Image Views for KDE sampler
    void createImageViewsKDESampler()
    {
        m_imageViewsKDESampler.resize(MAX_FRAMES_IN_FLIGHT);
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            m_imageViewsKDESampler[i] = createImageView(m_StorageImagesKDE[i], VK_FORMAT_R32_SINT);
    }
    
    // Create Descriptor Pool (for uniform buffer and texture sampler)
    VkDescriptorPool m_descriptorPoolCompute;
    void createDescriptorPoolForCompute()
    {
        // Create Info for the pool: pool size = # frames in flight
        std::array<VkDescriptorPoolSize, 4> poolSizes{};

        // Info for uniform buffer
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(2* MAX_FRAMES_IN_FLIGHT); // How many descriptors in over all descriptorsets exist. We have 2 ubos per frame in flight

        // Info for SSBOs with GeoLifeDataset Kernel and sync //RTVis
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT *3); // We have three ssbos: Geolife data, kernel, sync

        // Info for KDE Storage image //RTVis
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        // Info for KDE sampler
        poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[3].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);  //RTVIS: 2 samplers per frame: texture and KDE Sampler


        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT); // maximum number of descriptorSets that can use this pool

        // Create the descriptor set pool
        if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPoolCompute) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: failed to create descriptor pool!");
    }

    // Create Descriptor Sets for Compute and update them
    std::vector<VkDescriptorSet> m_descriptorSetsCompute;
    void createDescriptorSetsCompute()
    {
        // Allocate a descriptor set for each frame in flight defined by m_descriptorSetLayoutForCompute
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayoutForCompute);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPoolCompute;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        allocInfo.pSetLayouts = descriptorSetLayouts.data();
        m_descriptorSetsCompute.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSetsCompute.data()) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: failed to allocate descriptor sets!");

        // Update the allocted Descriptor Sets: Links descriptors to buffers
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            // Uniform Buffer with user input:
            VkDescriptorBufferInfo bufferInfoUBO{};
            bufferInfoUBO.buffer = m_uniformBuffers[i];
            bufferInfoUBO.offset = 0;
            bufferInfoUBO.range = sizeof(GeoLife::UserParameters);

            // GeoLifeDataSet Storage uffer
            VkDescriptorBufferInfo bufferInfoSSBOGeoLife{};
            bufferInfoSSBOGeoLife.buffer = m_ssboGeoLifeData;
            bufferInfoSSBOGeoLife.offset = 0;
            bufferInfoSSBOGeoLife.range = m_bufferSizeGeoLifeDataSet;

            // KDE Storage Image
            VkDescriptorImageInfo imageInfoStorageKDE{}; //RTVis
            imageInfoStorageKDE.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfoStorageKDE.imageView = m_imageViewsKDEStorage[i];

            // KDE Storage Sampler
            VkDescriptorImageInfo imageInfoSamplerKDE{}; //RTVis
            imageInfoSamplerKDE.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfoSamplerKDE.imageView = m_imageViewsKDESampler[i];
            imageInfoSamplerKDE.sampler = m_textureSampler;//; m_samplersKDE[i];

            // Uniform Buffer with kernel:
            VkDescriptorBufferInfo bufferInfoKernelUBO{};
            bufferInfoKernelUBO.buffer = m_kernelUniformBuffers[i];
            bufferInfoKernelUBO.offset = 0;
            bufferInfoKernelUBO.range = sizeof(glm::vec4)*121;

            // SSBO for syncing
            VkDescriptorBufferInfo bufferInfoSSBOSync{};
            bufferInfoSSBOSync.buffer = m_ssbosSync[i];
            bufferInfoSSBOSync.offset = 0;
            bufferInfoSSBOSync.range = m_bufferSizeSync;

            // SSBO for KDE Kernel
            VkDescriptorBufferInfo bufferInfoSSBOKernel{};
            bufferInfoSSBOKernel.buffer = m_ssbosKernel[i];
            bufferInfoSSBOKernel.offset = 0;
            bufferInfoSSBOKernel.range = m_bufferSizeKernel;

            // Create descripter writers
            std::array<VkWriteDescriptorSet, 7> descriptorWrites{};

            // Descriptro Write Info For ubo
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = m_descriptorSetsCompute[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfoUBO; // use this if descriptor set is for buffer
            descriptorWrites[0].pImageInfo = nullptr; // use this if descriptor set is for image
            descriptorWrites[0].pTexelBufferView = nullptr; // // use this if descriptor set is for buffer views

            // Descriptor write for For GeoLife SSBO
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = m_descriptorSetsCompute[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &bufferInfoSSBOGeoLife; // use this if descriptor set is for buffer
            descriptorWrites[1].pImageInfo = nullptr; // use this if descriptor set is for image
            descriptorWrites[1].pTexelBufferView = nullptr; // // use this if descriptor set is for buffer views

            // Descripto write for KDE Storage image
            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = m_descriptorSetsCompute[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[2].pImageInfo = &imageInfoStorageKDE;

            // For KDE sampler RTVIS
            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = m_descriptorSetsCompute[i];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pImageInfo = &imageInfoSamplerKDE;

            // Descriptro Write Info For Kernel ubo
            descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[4].dstSet = m_descriptorSetsCompute[i];
            descriptorWrites[4].dstBinding = 4;
            descriptorWrites[4].dstArrayElement = 0;
            descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[4].descriptorCount = 1;
            descriptorWrites[4].pBufferInfo = &bufferInfoKernelUBO; // use this if descriptor set is for buffer
            descriptorWrites[4].pImageInfo = nullptr; // use this if descriptor set is for image
            descriptorWrites[4].pTexelBufferView = nullptr; // // use this if descriptor set is for buffer views

            // Descriptor write for For Work group sync SSBO
            descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[5].dstSet = m_descriptorSetsCompute[i];
            descriptorWrites[5].dstBinding = 5;
            descriptorWrites[5].dstArrayElement = 0;
            descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[5].descriptorCount = 1;
            descriptorWrites[5].pBufferInfo = &bufferInfoSSBOSync; // use this if descriptor set is for buffer
            descriptorWrites[5].pImageInfo = nullptr; // use this if descriptor set is for image
            descriptorWrites[5].pTexelBufferView = nullptr; // // use this if descriptor set is for buffer views

            // Descriptor write for For kernel ssbo
            descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[6].dstSet = m_descriptorSetsCompute[i];
            descriptorWrites[6].dstBinding = 6;
            descriptorWrites[6].dstArrayElement = 0;
            descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[6].descriptorCount = 1;
            descriptorWrites[6].pBufferInfo = &bufferInfoSSBOKernel; // use this if descriptor set is for buffer
            descriptorWrites[6].pImageInfo = nullptr; // use this if descriptor set is for image
            descriptorWrites[6].pTexelBufferView = nullptr; // // use this if descriptor set is for buffer views

            // update Descriptorsets  using infos from above
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    //Create Compute Pipline
    VkPipelineLayout m_computePipelineLayout;
    VkPipeline m_computePipeline;
    void createComputePipeline() 
    {
        // Print info about compute shader capabilities
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
        std::cout << "Compute Capabilities Info:";
        std::cout << "\n    maxComputeWorkGroupSize = ";
        std::cout << deviceProperties.limits.maxComputeWorkGroupSize[0] << " , " << deviceProperties.limits.maxComputeWorkGroupSize[1] << " , " << deviceProperties.limits.maxComputeWorkGroupSize[2];
        std::cout << "\n    maxComputeWorkGroupInvocations: " << deviceProperties.limits.maxComputeWorkGroupInvocations;
        m_maxWorkgroupInvocation = deviceProperties.limits.maxComputeWorkGroupInvocations;
        std::cout << "\n    maxComputeWorkGroupCount = ";
        std::cout << deviceProperties.limits.maxComputeWorkGroupCount[0] << " , " << deviceProperties.limits.maxComputeWorkGroupCount[1] << " , " << deviceProperties.limits.maxComputeWorkGroupCount[2];
        std::cout << "\n----------";
        auto computeShaderCode = readFile("comp.spv"); 

        // Load Compute Shader
        VkShaderModule computeShaderModule = createShaderModule(computeShaderCode); 

        //Create compute pipeline layout 
        VkPipelineShaderStageCreateInfo computeShaderStageInfo{}; 
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; 
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT; 
        computeShaderStageInfo.module = computeShaderModule; 
        computeShaderStageInfo.pName = "main"; 
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{}; 
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
        pipelineLayoutInfo.setLayoutCount = 1; 
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayoutForCompute;
        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_computePipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: Failed to create compute pipeline layout!");
        
        //Create compute pipeline from layout
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO; 
        pipelineInfo.layout = m_computePipelineLayout; 
        pipelineInfo.stage = computeShaderStageInfo; 
        if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_computePipeline) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: Failed to create compute pipeline!");

        // Destroy Shader module, not needed anymore
        vkDestroyShaderModule(m_device, computeShaderModule, nullptr);
    }

    // Create Compute Command Buffers 
    std::vector<VkCommandBuffer> m_commandBuffersCompute;
    void createCommandBuffersCompute() {
        m_commandBuffersCompute.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool; // FRAGE: Warum braucht Compute keinen eigenen Command Pool?
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)m_commandBuffersCompute.size();

        if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffersCompute.data()) != VK_SUCCESS)
            throw std::runtime_error("\nERROR: Failed to allocate compute command buffers!");
    }

    // Helper for recording compute Commands
    // should be m_numberOfDatapointsLoaded / 32 but that does not work. 5622 is the empirical maximum;
    int m_numberOfWorkGroups = 2; // maximum 5622 but this value must be a power of 2 for parrallel reduction;
    void recordCommandBufferCompute(VkCommandBuffer commandBuffer, VkImage& image)
    {
        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{}; 
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) throw std::runtime_error("\nERROR: Failed to begin recording compute command buffer!");
       
        // Bind Pipline and Descriptors for Compute
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_descriptorSetsCompute[m_currentFrame], 0, nullptr);
        
        // Dispatch compute shader
        vkCmdDispatch(commandBuffer, m_numberOfWorkGroups, 1, 1); // 1 , 1, 1 = number of x-y-z workgoups. Idispatch onw work group with as many invocations as possible

        // Create Memory barrier that ensures that compute buffer is done with writing before fragment shader starts reading
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
        uint32_t graphAndCompQueueFamIdx = queueFamilyIndices.graphicsAndComputeFamily.value();
        VkImageMemoryBarrier  imageMemoryBarrierKDE{};    
        imageMemoryBarrierKDE.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrierKDE.pNext                 = NULL;
        imageMemoryBarrierKDE.srcAccessMask         = VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrierKDE.dstAccessMask         = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrierKDE.oldLayout             = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrierKDE.newLayout             = VK_IMAGE_LAYOUT_GENERAL;// VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL would be better but needs another transition after smapling
        imageMemoryBarrierKDE.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrierKDE.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrierKDE.image                 = image;
        imageMemoryBarrierKDE.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrierKDE.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrierKDE.subresourceRange.levelCount = 1;
        imageMemoryBarrierKDE.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrierKDE.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrierKDE);

        // End Command BUffer
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) throw std::runtime_error("\nERROR: Failed to record compute command buffer!");
    }

    // Function to create Gaussian Kernel for KDE
    // We assume a 11x11 matrix for the kernel where 0 are written when smaller kernel soze is used
    int maxRowLength = 11;
    int maxColLength = 11;
    int maxKernelSize = 5;
    void createGaussianKernel(float sigma, int kernelSize, glm::vec4 kernel[121]) // kernel size must be <= 5
    {
        int kernel_size = std::max(0, std::min(5, kernelSize));

        float sum = 0.0; // For accumulating the kernel values
        for (int y = -kernel_size; y <= kernel_size; y++)
        {
            int rowIndex = maxKernelSize + y;
            for (int x = -kernel_size; x <= kernel_size; x++)
            {
                int colIndex = maxKernelSize + x;
                float n = 1 / (2.0f * 3.14156 * pow(sigma, 2));
                float value = exp(-(pow(x, 2) + pow(y, 2)) / (2.0f * pow(sigma, 2))) * n;
                kernel[rowIndex * maxRowLength + colIndex].x = value;
                sum += value;
            }
        }
    }


    ///////////////
    // Main Loop //
    ///////////////
    void mainLoop() 
    {
        float lastSigma = m_ubo.sigma;
        int lastKernelSize = m_ubo.kernelSize;
        GeoLife::UserParameters oldUbo;
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            m_ui.update(m_ubo, m_colorMapTexture.texHeight, m_colorMapTexture.texWidth, m_descriptorSetColorbar);
            g_mouseControl.update(m_window,m_ubo, WIDTH, HEIGHT);
            if (global_leftMouseButtonReleased || g_mouseControl.m_updateZoom)
            {
                g_mouseControl.m_updateZoom = false;
                if (lastSigma != m_ubo.sigma || lastKernelSize != m_ubo.kernelSize) m_ubo.kernelChanged = 1;
                lastSigma = m_ubo.sigma;
                lastKernelSize = m_ubo.kernelSize;
                if (GeoLife::parametersChanged(oldUbo, m_ubo)) m_ui.m_userParamterUpdated = true;
                else m_ui.m_userParamterUpdated = false;
                oldUbo = m_ubo;
                if (m_ui.m_userParamterUpdated)
                {
                    m_ubo.runCompute = 1;
                    if (m_ubo.kernelChanged) createGaussianKernel(m_ubo.sigma, m_ubo.kernelSize, m_kernelUbo);
                }
            }
            if (global_firstFrame)
            {
                createGaussianKernel(m_ubo.sigma, m_ubo.kernelSize, m_kernelUbo);
                m_ubo.runCompute = 1; // on the first frame always run compute
            }

            drawFrame();
            m_ubo.runCompute = 0;
            m_ui.m_userParamterUpdated = false;
            global_leftMouseButtonReleased = false;
            global_firstFrame = false;
            //Wait for logical device to finish operations before exiting main loop
            vkDeviceWaitIdle(m_device);
        }
    }

    void cleanup() 
    {
        // Destroy Ui
        m_ui.cleanup();
        
        // Destroy semaphores
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }
        
        // Destroy command pools (this automatically frees its buffer)
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        // Destory index & vertex Buffers and release their Memory
        vkDestroyBuffer(m_device, m_indexBufferForQuad, nullptr);
        vkFreeMemory(m_device, m_indexBufferMemoryForQuad, nullptr);
        vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
        vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);

        // Destroys m_swapChainFramebuffers, m_swapChainImageViews AND m_swapChain
        cleanupSwapChain();
        

        // Destroy Imageviews
        vkDestroyImageView(m_device, m_cityMapTexture.textureImageView, nullptr);
        vkDestroyImageView(m_device, m_colorMapTexture.textureImageView, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)  //RTVis 
        {
            vkDestroyImageView(m_device, m_imageViewsKDEStorage[i], nullptr);
            vkDestroyImageView(m_device, m_imageViewsKDESampler[i], nullptr);
        }
        
        // Destroy Images and their memory
        vkDestroyImage(m_device, m_cityMapTexture.textureImage, nullptr);
        vkFreeMemory(m_device, m_cityMapTexture.textureImageMemory, nullptr);
        vkDestroyImage(m_device, m_colorMapTexture.textureImage, nullptr);
        vkFreeMemory(m_device, m_colorMapTexture.textureImageMemory, nullptr);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) // RTVis
        {
            vkDestroyImage(m_device, m_StorageImagesKDE[i], nullptr);
            vkFreeMemory(m_device, m_StorageImageKDEMemories[i], nullptr);
        }

        // Destroy buffers and their memory (UBO, SSBOS etc)
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) 
        {
            vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr); 
            vkDestroyBuffer(m_device, m_kernelUniformBuffers[i], nullptr);
            vkDestroyBuffer(m_device, m_ssbosSync[i], nullptr); // RTVis
            vkDestroyBuffer(m_device, m_ssbosKernel[i], nullptr); // RTVis
            vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
            vkFreeMemory(m_device, m_kernelUniformBuffersMemory[i], nullptr);
            vkFreeMemory(m_device, m_ssboMemoriesSync[i], nullptr);
            vkFreeMemory(m_device, m_ssboMemoriesKernel[i], nullptr);
        }
        vkDestroyBuffer(m_device, m_ssboGeoLifeData, nullptr); // RTVis

        vkFreeMemory(m_device, m_ssboMemoryGeoLifeData, nullptr);


        // Destroy Descriptor Pool
        // You don't need to explicitly clean up descriptor sets, 
        // because they will be automatically freed when the descriptor pool is destroyed. 
        vkDestroyDescriptorPool(m_device, m_descriptorPoolGraphics, nullptr);
        vkDestroyDescriptorPool(m_device, m_descriptorPoolCompute, nullptr); //RTVIS

        // Destroy descriptorset layout of graphics and compute pipline
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutForGraphics, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutForCompute, nullptr); // RTVIS

        // Destroy Pipeline
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipeline(m_device, m_computePipeline, nullptr); //RTVIS

        // Destroy pipeline Layout
        vkDestroyPipelineLayout(m_device, m_graphicsPipelineLayout, nullptr);
        vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);

        /// Destroy render pass
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

        // Destroy logical device
        vkDestroyDevice(m_device, nullptr);
        // Device queues are implicitly cleaned up when the device is destroyed/

        // Debugging with validation layers: destroy debugMessenger
        if (enableValidationLayers) DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);

        // Destroy surface
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

        // Destroy vulkan instance
        vkDestroyInstance(m_instance, nullptr); //nullptr = optional allocator callback

        //Destroy glfw stuff
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
};


int main(int argc, char** argv) {
    namespace fs = std::filesystem;

    // Optional first argument: path to the GeoLife data set file (default: geoLifeDataSet.txt next to the executable)
    std::string dataSetPath = argc > 1 ? fs::absolute(argv[1]).string() : "geoLifeDataSet.txt";

    // Shaders, textures and the data set are loaded relative to the working directory.
    // If they are not found there, switch to the directory of the executable.
    if (!fs::exists("vert.spv") && argc > 0)
    {
        fs::path executableDir = fs::absolute(argv[0]).parent_path();
        if (fs::exists(executableDir / "vert.spv")) fs::current_path(executableDir);
    }

    GeoLifeVisualistation app(dataSetPath);

    try
    {
        app.run();
    }
    catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}