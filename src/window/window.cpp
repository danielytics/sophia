
#include "window/window.h"
#include "util/logging.h"
#include "util/telemetry.h"
#include "util/config.h"

#define STB_IMAGE_IMPLEMENTATION
#include "util/stb_image.h"

#include "graphics/shader.h"
#include "graphics/tilemap.h"
#include "graphics/spritepool.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <entt/entt.hpp>

#include <iostream>
#include <cmath>
#include <random>

#include <stdexcept>
#include <chrono>
typedef std::chrono::high_resolution_clock Clock;
typedef float Time_t;
typedef std::chrono::duration<Time_t> Time;

 const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

 Renderable::~Renderable() {}

GLuint loadTexture (const std::string& filename)
{
    GLuint texture = 0;
    int width, height, comp;
    unsigned char* image = stbi_load(filename.c_str(), &width, &height, &comp, STBI_rgb_alpha);
    if (image) {
        info("Loading image '{}', width={} height={} components={}", filename, width, height, comp);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

        stbi_image_free(image);
    } else {
        error("Could not load texture: {}", filename);
    }
    return texture;
}

GLuint loadTextureArray (const std::vector<std::string>& filenames)
{
    GLuint texture = 0;
    int width, height;
    int components;
    // Load the image files
    auto images = std::vector<unsigned char*>{};
    for (auto filename : filenames) {
        unsigned char* image = stbi_load(filename.c_str(), &width, &height, &components, STBI_rgb_alpha);
        info("Loading image '{}', width={} height={} components={}", filename, width, height, components);
        images.push_back(image);
    }

    // Create the texture array
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA,  width, height, images.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    // Load texture data into texture array
    for (unsigned index = 0; index < images.size(); ++index) {
        auto image = images[index];
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, image);
        stbi_image_free(image);
    }
    info("Loaded {} images into texture array", images.size());

    //Always set reasonable texture parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

    return texture;
}

#include "graphics/shader.h"

struct Test {
     Shader_t shader;
     Shader_t lamp;
     Shader_t shadows;
     GLuint light_vao;
     GLuint vbo;
     GLuint backdrop_vao;
     GLuint backdrop_vbo;
     GLuint depthMapFBO;
     GLuint depthMap;
}

Test setupTest() {

    GLuint lightVAO;
    GLuint vbo;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // set the vertex attributes (only position data for our lamp)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Backdrop plane
    float backdrop[] = {
        -0.5f, -0.5f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.0f,  0.0f,  0.0f, 1.0f
    };
    GLuint backdropVAO;
    GLuint backdropVBO;
    glGenVertexArrays(1, &backdropVAO);
    glBindVertexArray(backdropVAO);
    glGenBuffers(1, &backdropVBO);
    glBindBuffer(GL_ARRAY_BUFFER, backdropVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(backdrop), backdrop, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Shadow mapping
    GLuint depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    GLuint depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return {
        Shader::load("data/shaders/lighting.vert", "data/shaders/lighting.frag"),
        Shader::load("data/shaders/lighting.vert", "data/shaders/lamp.frag"),
        Shader::load("data/shaders/shadowmap.vert", "data/shaders/shadowmap.frag"),
        lightVAO,
        vbo,
        backdropVAO,
        backdropVBO,
        depthMapFBO,
        depthMap
    };
}

Window::Window (const std::string& title)
    : ready(false)
    , window(0)
    , title(title)
{
    // Initialize SDL's Video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        error("Failed to init SDL");
        throw std::runtime_error("Failed to initialise SDL");
    }

    ready = true;
}

Window::~Window ()
{
    if (ready) {
        if (window) {
            SDL_GL_DeleteContext(context);
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
}

void Window::open (const YAML::Node& config_node)
{
    // Load ystem configuration
    struct Resolution {
        int width;
        int height;
        inline void operator= (const std::vector<int>& vec) {
            width = vec[0];
            height = vec[1];
        }
    };
    struct {
        Resolution resolution;
        int fsaa;
        bool vsync;
        bool fullscreen;
    } config;
    {
        std::vector<int> res;
         using namespace Config;
         auto parser = make_parser(
            map("graphics",
                scalar("fullscreen", config.fullscreen),
                scalar("vsync", config.vsync),
                one_of(
                    option("resolution",
                        std::map<std::string, Resolution>{
                            {"720p", Resolution{1280, 720}},
                            {"1080p", Resolution{1920, 1080}},
                        }, config.resolution),
                    sequence("resolution", res)
                    ),
                option("fsaa",
                    std::map<std::string, int>{
                        {"2x", 2},
                        {"4x", 4},
                        {"8x", 8},
                        {"16x", 16},
                        {"32x", 32},
                        {"Off", 0},
                    }, config.fsaa)
            )
        );
        parser(config_node);
        if (res.size()) {
            config.resolution = res;
        }
    }
    info("Loaded graphics configuration: fsaa={} vsync={} fullscreen={} width={} height={}", config.fsaa, config.vsync, config.fullscreen, config.resolution.width, config.resolution.height);

    // Set the OpenGL attributes for our context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, config.fsaa > 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, config.fsaa);

    // Create a centered window, using system configuration
    window = SDL_CreateWindow(
                title.c_str(),
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                config.resolution.width,
                config.resolution.height,
                SDL_WINDOW_OPENGL | config.fullscreen);

    // Create OpenGL rendering context
    context = SDL_GL_CreateContext(window);

    width = static_cast<float>(config.resolution.width);
    height = static_cast<float>(config.resolution.height);
    projection = glm::perspective(glm::radians(60.0f), width / height, 0.1f, 20.0f);
//    float w = (width / height) * 5.0f;
//    projection = glm::ortho(-w, w, -5.0f, 5.0f, 0.0f, 10.0f);
    viewport = glm::vec4(0, 0, width, height);
    glViewport(0, 0, int(width), int(height));

    // Set vertical sync if configured
    SDL_GL_SetSwapInterval(config.vsync);

    // Load OpenGL 3+ functions
    glewExperimental = GL_TRUE;
    glewInit();

    // Enable FSAA if configured
    if (config.fsaa > 0) {
        glEnable(GL_MULTISAMPLE);
    }
}

void Window::run ()
{
    SDL_Event event;
    bool running = true;

    // Initialise timekeeping
    float frame_time = 0;
    auto start_time = Clock::now();
    auto previous_time = start_time;
    auto current_time = start_time;
    auto frames = Telemetry::Counter{"frames"};

    TileMap tileMap;
    tileMap.init(std::vector<std::vector<float>>{
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,  },
        { 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1,  },
        { 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1,  },
        { 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1,  },
        { 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
     });

    SpritePool sprites;
    sprites.init(2);
    std::vector<Sprite> spriteData;
    {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_real_distribution<float> dist(0.0, 400.0);
        for (unsigned i=0; i<1000000; ++i) {
            spriteData.push_back(Sprite{{dist(mt), dist(mt)}, 1});
        }
    }

    glm::vec3 camera = glm::vec3(0.0f, 0.0f, 10.0f);
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);

    glActiveTexture(GL_TEXTURE0 + 0);
    GLuint texture = loadTextureArray(std::vector<std::string>{
        "data/TEXTURES/G000M801.BMP",
        "data/TEXTURES/S5G0I800.BMP",
    });
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(0); // Disable writing to depth buffer

    std::vector<Renderable*> renderables{
        &tileMap,
        &sprites,
    };

    Test test = setupTest();
    test.lamp.bindUnfiromBlock("Matrices", 0);
    test.shader.bindUnfiromBlock("Matrices", 0);
    test.shadows.bindUnfiromBlock("Matrices", 0);

    GLuint matrices_ubo;
    glGenBuffers(1, &matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, matrices_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, matrices_ubo, 0, 2 * sizeof(glm::mat4));
    glBindBuffer(GL_UNIFORM_BUFFER, matrices_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    Uniform_t u_lamp_model = test.lamp.uniform("model");
    Uniform_t u_lamp_color = test.lamp.uniform("lightColor");

    Uniform_t u_model = test.shader.uniform("model");
    Uniform_t u_normal = test.shader.uniform("normal");
    Uniform_t u_material_ambient = test.shader.uniform("material.ambient");
    Uniform_t u_material_diffuse = test.shader.uniform("material.diffuse");
    Uniform_t u_material_specular = test.shader.uniform("material.specular");
    Uniform_t u_material_shininess = test.shader.uniform("material.shininess");

    Uniform_t u_light0_ambient = test.shader.uniform("lights[0].ambient");
    Uniform_t u_light0_diffuse = test.shader.uniform("lights[0].diffuse");
    Uniform_t u_light0_specular = test.shader.uniform("lights[0].specular");
    Uniform_t u_light0_position = test.shader.uniform("lights[0].position");
    Uniform_t u_light0_lightspace = test.shader.uniform("lights[0].lightSpaceMatrix");
    Uniform_t u_light1_ambient = test.shader.uniform("lights[1].ambient");
    Uniform_t u_light1_diffuse = test.shader.uniform("lights[1].diffuse");
    Uniform_t u_light1_specular = test.shader.uniform("lights[1].specular");
    Uniform_t u_light1_position = test.shader.uniform("lights[1].position");
    Uniform_t u_light1_lightspace = test.shader.uniform("lights[1].lightSpaceMatrix");

    Uniform_t u_shadow_model = test.shadows.uniform("model");
    Uniform_t u_lightspace = test.shadows.uniform("lightSpaceMatrix");

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glClearDepth(1.0);
//    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Run the main processing loop
    do {
        // Gather and dispatch input
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_SPACE:
                    tileMap.reset(std::vector<std::vector<float>>{
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  },
                     });
                    break;
                default:
                    break;
                }
            }
        }
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        auto speed = state[SDL_SCANCODE_LSHIFT] ? 3.0f : 2.0f;
        if (state[SDL_SCANCODE_UP]){
            camera.y += (frame_time * 5.0f) * speed;
        }
        if (state[SDL_SCANCODE_DOWN]) {
            camera.y -= (frame_time * 5.0f) * speed;
        }
        if (state[SDL_SCANCODE_LEFT]) {
            camera.x -= (frame_time * 5.0f) * speed;
        }
        if (state[SDL_SCANCODE_RIGHT]) {
            camera.x += (frame_time * 5.0f) * speed;
        }

        // Calculate current time
        float time_since_start = std::chrono::duration_cast<Time>(current_time - start_time).count();

        // Generate the view matrix using the camera position
        glm::mat4 view = glm::lookAt(camera, glm::vec3{camera.x, camera.y, camera.z - 1.0f}, Up);
        glBindBuffer(GL_UNIFORM_BUFFER, matrices_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // Get the mouse position (in world coordinates)
        int tmpMouseX, tmpMouseY;
        SDL_GetMouseState(&tmpMouseX, &tmpMouseY);
        glm::vec3 mouse = glm::unProject(glm::vec3(tmpMouseX, viewport.w - tmpMouseY, 1.0f), view, projection, viewport);

        // Get the screen bounding recatingle
        Rect screenBounds{
            glm::unProject(glm::vec3(viewport.x, viewport.y, 1.0f), view, projection, viewport),
            glm::unProject(glm::vec3(viewport.z, viewport.w, 1.0f), view, projection, viewport),
        };

        info("Mouse:  {}, {}, {}", mouse.x, mouse.y, mouse.z);
//        sprites.update(spriteData);

        // Clear screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//        // Render all renderables
//        for (auto renderable : renderables) {
//            renderable->shader().use();
//            glUniformMatrix4fv(renderable->projection(), 1, GL_FALSE, glm::value_ptr(projection));
//            glUniformMatrix4fv(renderable->view(), 1, GL_FALSE, glm::value_ptr(view));
//            renderable->render(screenBounds);
//        }
        info();

        glm::vec3 lightPos0(20.0f, 0.0f, 3.0f);
        glm::vec3 lightPos1(-20.0f, 5.0f, 5.0f);
        glm::mat4 light0_lightSpaceMatrix;
        glm::mat4 light1_lightSpaceMatrix;

        // Render shadowmap
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, test.depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        {
            float near_plane = 1.0f, far_plane = 7.5f;
            glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
            glm::mat4 lightView = glm::lookAt(glm::vec3(-2.0f, 4.0f, -1.0f),
                                              glm::vec3( 0.0f, 0.0f,  0.0f),
                                              glm::vec3( 0.0f, 1.0f,  0.0f));
            light0_lightSpaceMatrix = lightProjection * lightView;

            test.shadows.use();
            Shader::setUniform(u_lightspace, light0_lightSpaceMatrix);

            // Render scene to shadowmap
            glm::mat4 model = glm::mat4( 1.0 );
            model = glm::scale(model, glm::vec3(1.2f));
            model = glm::translate(model, glm::vec3(mouse.x, mouse.y, 0));
            Shader::setUniform(u_shadow_model, model);
            glBindVertexArray(test.light_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Render scene
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, int(width), int(height));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, test.depthMap);

        // test
        {
            test.lamp.use();
            glm::mat4 model = glm::mat4( 1.0 );
            model = glm::scale(model, glm::vec3(0.2f));
            model = glm::translate(model, lightPos0);
            Shader::setUniform(u_lamp_model, model);
            Shader::setUniform(u_lamp_color, glm::vec3(1.0f, 0.0f, 0.0f));
            glBindVertexArray(test.light_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            model = glm::mat4( 1.0 );
            model = glm::scale(model, glm::vec3(0.2f));
            model = glm::translate(model, lightPos1);
            Shader::setUniform(u_lamp_model, model);
            Shader::setUniform(u_lamp_color, glm::vec3(0.0f, 0.0f, 1.0f));
            glBindVertexArray(test.light_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glm::vec4 light0_pos = view * glm::vec4(lightPos0, 1.0);
        glm::vec4 light1_pos = view * glm::vec4(lightPos1, 1.0);
        {
            test.shader.use();
            Shader::setUniform(u_light0_position, light0_pos);
            Shader::setUniform(u_light0_ambient,  glm::vec3(0.2f, 0.05f, 0.05f));
            Shader::setUniform(u_light0_diffuse,  glm::vec3(1.0f, 0.1f, 0.1f));
            Shader::setUniform(u_light0_specular, glm::vec3(1.0f, 0.25f, 0.25f));
            Shader::setUniform(u_light0_lightspace, light0_lightSpaceMatrix);
            Shader::setUniform(u_light1_position, light1_pos);
            Shader::setUniform(u_light1_ambient,  glm::vec3(0.05f, 0.05f, 0.2f));
            Shader::setUniform(u_light1_diffuse,  glm::vec3(0.1f, 0.1f, 1.0f));
            Shader::setUniform(u_light1_specular, glm::vec3(0.25f, 0.25f, 1.0f));
            Shader::setUniform(u_light1_lightspace, light1_lightSpaceMatrix);

            glm::mat4 model = glm::mat4( 1.0 );
            model = glm::scale(model, glm::vec3(25.0f, 25.0f, 1.0f));
            model = glm::translate(model, glm::vec3(0, 0, -10.0f));
            Shader::setUniform(u_model, model);
            glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model * view));
            Shader::setUniform(u_normal, normalMatrix);
            Shader::setUniform(u_material_ambient, glm::vec3(0.2f, 0.2f, 0.2));
            Shader::setUniform(u_material_diffuse, glm::vec3(0.6f, 0.6f, 0.6f));
            Shader::setUniform(u_material_specular, glm::vec3(0.8f, 0.8f, 0.8f));
            Shader::setUniform(u_material_shininess, 8.0f);
            glBindVertexArray(test.backdrop_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            model = glm::mat4( 1.0 );
            model = glm::scale(model, glm::vec3(1.2f));
            model = glm::translate(model, glm::vec3(mouse.x, mouse.y, 0));
            normalMatrix = glm::inverseTranspose(glm::mat3(model * view));

            Shader::setUniform(u_model, model);
            Shader::setUniform(u_normal, normalMatrix);
            Shader::setUniform(u_material_ambient, glm::vec3(0.2f, 0.2f, 0.2f));
            Shader::setUniform(u_material_diffuse, glm::vec3(0.8f, 0.8f, 0.8f));
            Shader::setUniform(u_material_specular, glm::vec3(1.0f, 1.0f, 1.0f));
            Shader::setUniform(u_material_shininess, 32.0f);
            glBindVertexArray(test.light_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }


        // Refresh display
        SDL_GL_SwapWindow(window);

        // Update timekeeping
        previous_time = current_time;
        current_time = Clock::now();
        frame_time = std::chrono::duration_cast<Time>(current_time - previous_time).count();
        frames.inc();
        trace("Frame {} ended after {:1.6f} seconds", frames.get(), frame_time);
    } while (running);

    test.shader.unload();
    test.lamp.unload();
    glDeleteBuffers(1, &matrices_ubo);
    glDeleteBuffers(1, &test.vbo);
    glDeleteBuffers(1, &test.backdrop_vbo);
    glDeleteVertexArrays(1, &test.light_vao);
    glDeleteVertexArrays(1, &test.backdrop_vao);

    glDeleteTextures(1, &texture);

    info("Average framerate: {} fps", (frames.get() / std::chrono::duration_cast<Time>(current_time - start_time).count()));
}

typedef unsigned Handle_t;

struct Background
{
    Handle_t image;
    glm::vec3 position;
    glm::vec3 transformation; // x: rotation, y: scale
};
struct Foreground
{

};

void render () {

}

