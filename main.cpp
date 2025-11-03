#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <array>
#include <cmath>
#include <algorithm>

// IMPORTANT: This is needed for Windows to properly link SDL2
#ifdef _WIN32
#include <SDL2/SDL_main.h>
#endif

// Constants
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Simple Vec3 structure (no GLM dependency)
struct Vec3 {
    float x, y, z;
    
    Vec3(float x_ = 0, float y_ = 0, float z_ = 0) : x(x_), y(y_), z(z_) {}
    
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
    
    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
};

// Color structure
struct Color {
    uint8_t r, g, b, a;
    
    Color(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0, uint8_t alpha = 255) 
        : r(red), g(green), b(blue), a(alpha) {}
    
    uint32_t toUint32() const {
        // Corrected for SDL_PIXELFORMAT_ARGB8888
        // The format is: Alpha-Red-Green-Blue
        return (a << 24) | (r << 16) | (g << 8) | b;
    }
};

// Face structure
struct Face {
    std::vector<std::array<int, 3>> vertexIndices;
};

// Global variables
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
Color currentColor;
std::vector<Color> framebuffer;

// Initialize framebuffer
void initFramebuffer() {
    framebuffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT);
}

// Clear framebuffer with background color
void clear() {
    std::fill(framebuffer.begin(), framebuffer.end(), Color(0, 0, 0));  // Black background
}

// Set pixel in framebuffer
void pixel(int x, int y) {
    // Check bounds
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        framebuffer[y * SCREEN_WIDTH + x] = currentColor;
    }
}

// Bresenham's line algorithm
void line(Vec3 start, Vec3 end) {
    int x1 = static_cast<int>(std::round(start.x));
    int y1 = static_cast<int>(std::round(start.y));
    int x2 = static_cast<int>(std::round(end.x));
    int y2 = static_cast<int>(std::round(end.y));
    
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        pixel(x1, y1);
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// Draw triangle using lines
void triangle(const Vec3& A, const Vec3& B, const Vec3& C) {
    line(A, B);
    line(B, C);
    line(C, A);
}

// Load OBJ file
bool loadOBJ(const std::string& path, std::vector<Vec3>& out_vertices, std::vector<Face>& out_faces) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << path << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        
        if (prefix == "v") {
            // Vertex position
            Vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            out_vertices.push_back(vertex);
        }
        else if (prefix == "f") {
            // Face
            Face face;
            std::string vertexStr;
            
            while (iss >> vertexStr) {
                std::array<int, 3> indices = {0, 0, 0};
                std::replace(vertexStr.begin(), vertexStr.end(), '/', ' ');
                std::istringstream vertexIss(vertexStr);
                
                vertexIss >> indices[0];  // vertex index
                indices[0]--;  // OBJ indices are 1-based, convert to 0-based
                
                // Optional texture and normal indices
                if (vertexIss >> indices[1]) {
                    indices[1]--;
                    if (vertexIss >> indices[2]) {
                        indices[2]--;
                    }
                }
                
                face.vertexIndices.push_back(indices);
            }
            
            out_faces.push_back(face);
        }
    }
    
    file.close();
    std::cout << "Loaded " << out_vertices.size() << " vertices and " << out_faces.size() << " faces" << std::endl;
    return true;
}

// Setup vertex array from faces
std::vector<Vec3> setupVertexArray(const std::vector<Vec3>& vertices, const std::vector<Face>& faces) {
    std::vector<Vec3> vertexArray;
    
    for (const auto& face : faces) {
        for (const auto& vertexIndices : face.vertexIndices) {
            Vec3 vertexPosition = vertices[vertexIndices[0]];
            vertexArray.push_back(vertexPosition);
        }
    }
    
    return vertexArray;
}

// Render buffer to screen
void renderBuffer(SDL_Renderer* renderer) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, 
        SDL_PIXELFORMAT_ARGB8888, 
        SDL_TEXTUREACCESS_STREAMING, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT);
    
    void* texturePixels;
    int texturePitch;
    SDL_LockTexture(texture, nullptr, &texturePixels, &texturePitch);
    
    Uint32* pixels = static_cast<Uint32*>(texturePixels);
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        pixels[i] = framebuffer[i].toUint32();
    }
    
    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    SDL_DestroyTexture(texture);
}

// Initialize SDL
void init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    window = SDL_CreateWindow("Software Renderer - OBJ Viewer", 
        SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT, 
        SDL_WINDOW_SHOWN);
    
    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }
    
    initFramebuffer();
    std::cout << "SDL initialized successfully!" << std::endl;
}

// Set current color
void setColor(const Color& color) {
    currentColor = color;
    std::cout << "Color set to: R=" << (int)color.r 
              << " G=" << (int)color.g 
              << " B=" << (int)color.b << std::endl;
}

// Main render function
void render() {
    // Load the OBJ model
    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    
    // Change this path to your OBJ file
    if (!loadOBJ("model.obj", vertices, faces)) {
        std::cerr << "Failed to load OBJ file" << std::endl;
        return;
    }
    
    // Scale and translate the model to fit on screen
    float scale = 100.0f;  // Adjust based on your model size
    Vec3 offset(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 0.0f);
    
    // Transform vertices
    for (auto& vertex : vertices) {
        vertex = vertex * scale + offset;
    }
    
    // Draw all triangles
    int triangleCount = 0;
    for (const auto& face : faces) {
        // Most OBJ faces are quads or triangles
        // We'll triangulate quads if necessary
        if (face.vertexIndices.size() >= 3) {
            // Draw first triangle
            Vec3 v1 = vertices[face.vertexIndices[0][0]];
            Vec3 v2 = vertices[face.vertexIndices[1][0]];
            Vec3 v3 = vertices[face.vertexIndices[2][0]];
            triangle(v1, v2, v3);
            triangleCount++;
            
            // If it's a quad, draw the second triangle
            if (face.vertexIndices.size() == 4) {
                Vec3 v4 = vertices[face.vertexIndices[3][0]];
                triangle(v1, v3, v4);
                triangleCount++;
            }
        }
    }
    
    std::cout << "Drew " << triangleCount << " triangles" << std::endl;
}

// IMPORTANT: SDL requires the main function to have these exact parameters
int main(int argc, char* argv[]) {
    init();
    
    if (window == nullptr || renderer == nullptr) {
        std::cerr << "Failed to initialize SDL properly" << std::endl;
        return -1;
    }
    
    // Print instructions
    std::cout << "\n=== OBJ Renderer Controls ===" << std::endl;
    std::cout << "Press R for Red" << std::endl;
    std::cout << "Press G for Green" << std::endl;
    std::cout << "Press B for Blue" << std::endl;
    std::cout << "Press Y for Yellow" << std::endl;
    std::cout << "Press W for White" << std::endl;
    std::cout << "Press C for Cyan" << std::endl;
    std::cout << "Press M for Magenta" << std::endl;
    std::cout << "Press ESC or close window to quit" << std::endl;
    std::cout << "============================\n" << std::endl;
    
    // Initial render with yellow color
    clear();
    setColor(Color(255, 255, 0));  // Yellow = Red + Green
    render();
    renderBuffer(renderer);
    
    bool running = true;
    SDL_Event event;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            
            // Re-render on any key press
            if (event.type == SDL_KEYDOWN) {
                clear();
                
                // Change color based on key pressed
                switch(event.key.keysym.sym) {
                    case SDLK_r:
                        setColor(Color(255, 0, 0));  // Red
                        break;
                    case SDLK_g:
                        setColor(Color(0, 255, 0));  // Green
                        break;
                    case SDLK_b:
                        setColor(Color(0, 0, 255));  // Blue
                        break;
                    case SDLK_y:
                        setColor(Color(255, 255, 0));  // Yellow
                        break;
                    case SDLK_w:
                        setColor(Color(255, 255, 255));  // White
                        break;
                    case SDLK_c:
                        setColor(Color(0, 255, 255));  // Cyan
                        break;
                    case SDLK_m:
                        setColor(Color(255, 0, 255));  // Magenta
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    default:
                        // Keep current color
                        break;
                }
                
                render();
                renderBuffer(renderer);
            }
        }
        
        SDL_Delay(16);  // ~60 FPS
    }
    
    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::cout << "Program terminated successfully" << std::endl;
    
    return 0;
}