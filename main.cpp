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
    
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
    
    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
};

// Simple 4x4 Matrix for transformations
struct Mat4 {
    float m[4][4];
    
    Mat4() {
        // Initialize as identity matrix
        for(int i = 0; i < 4; i++) {
            for(int j = 0; j < 4; j++) {
                m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }
    
    // Multiply matrix by vector
    Vec3 multiply(const Vec3& v) const {
        float w = m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3];
        if(w == 0) w = 1.0f;
        
        return Vec3(
            (m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3]) / w,
            (m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3]) / w,
            (m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3]) / w
        );
    }
    
    // Multiply two matrices
    Mat4 operator*(const Mat4& other) const {
        Mat4 result;
        for(int i = 0; i < 4; i++) {
            for(int j = 0; j < 4; j++) {
                result.m[i][j] = 0;
                for(int k = 0; k < 4; k++) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }
};

// Create rotation matrix around Y axis
Mat4 rotationY(float angle) {
    Mat4 mat;
    float c = cos(angle);
    float s = sin(angle);
    
    mat.m[0][0] = c;
    mat.m[0][2] = s;
    mat.m[2][0] = -s;
    mat.m[2][2] = c;
    
    return mat;
}

// Create rotation matrix around X axis
Mat4 rotationX(float angle) {
    Mat4 mat;
    float c = cos(angle);
    float s = sin(angle);
    
    mat.m[1][1] = c;
    mat.m[1][2] = -s;
    mat.m[2][1] = s;
    mat.m[2][2] = c;
    
    return mat;
}

// Create translation matrix
Mat4 translation(float x, float y, float z) {
    Mat4 mat;
    mat.m[0][3] = x;
    mat.m[1][3] = y;
    mat.m[2][3] = z;
    return mat;
}

// Create scale matrix
Mat4 scale(float sx, float sy, float sz) {
    Mat4 mat;
    mat.m[0][0] = sx;
    mat.m[1][1] = sy;
    mat.m[2][2] = sz;
    return mat;
}

// Create perspective projection matrix
Mat4 perspective(float fov, float aspect, float near, float far) {
    Mat4 mat;
    float tanHalfFov = tan(fov / 2.0f);
    
    mat.m[0][0] = 1.0f / (aspect * tanHalfFov);
    mat.m[1][1] = 1.0f / tanHalfFov;
    mat.m[2][2] = -(far + near) / (far - near);
    mat.m[2][3] = -(2.0f * far * near) / (far - near);
    mat.m[3][2] = -1.0f;
    mat.m[3][3] = 0.0f;
    
    return mat;
}

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

// Camera parameters
float cameraAngleY = 0.0f;
float cameraAngleX = 0.0f;
float cameraDistance = 5.0f;
bool autoRotate = false;

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
    
    window = SDL_CreateWindow("3D OBJ Viewer - Press Arrow Keys to Rotate", 
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
}

// Main render function with 3D transformations
void render(const std::vector<Vec3>& vertices, const std::vector<Face>& faces) {
    // Clear the screen
    clear();
    
    // Create transformation matrices
    Mat4 modelMatrix = scale(1.0f, 1.0f, 1.0f);  // Scale the model if needed
    
    // Rotate the model for better viewing angle
    Mat4 rotY = rotationY(cameraAngleY);
    Mat4 rotX = rotationX(cameraAngleX);
    Mat4 rotation = rotY * rotX;
    
    // Move the model back from the camera
    Mat4 translationMat = translation(0.0f, 0.0f, -cameraDistance);
    
    // Create perspective projection
    float fov = 3.14159f / 4.0f;  // 45 degrees
    float aspect = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
    Mat4 projection = perspective(fov, aspect, 0.1f, 100.0f);
    
    // Combine all transformations
    Mat4 mvp = projection * translationMat * rotation * modelMatrix;
    
    // Transform all vertices
    std::vector<Vec3> transformedVertices;
    for (const auto& vertex : vertices) {
        Vec3 transformed = mvp.multiply(vertex);
        
        // Convert from normalized device coordinates to screen coordinates
        transformed.x = (transformed.x + 1.0f) * 0.5f * SCREEN_WIDTH;
        transformed.y = (1.0f - transformed.y) * 0.5f * SCREEN_HEIGHT;  // Flip Y axis
        
        transformedVertices.push_back(transformed);
    }
    
    // Draw all triangles
    int triangleCount = 0;
    for (const auto& face : faces) {
        if (face.vertexIndices.size() >= 3) {
            // Check if vertices are valid
            bool validFace = true;
            for (const auto& idx : face.vertexIndices) {
                if (idx[0] >= transformedVertices.size()) {
                    validFace = false;
                    break;
                }
            }
            
            if (!validFace) continue;
            
            // Draw first triangle
            Vec3 v1 = transformedVertices[face.vertexIndices[0][0]];
            Vec3 v2 = transformedVertices[face.vertexIndices[1][0]];
            Vec3 v3 = transformedVertices[face.vertexIndices[2][0]];
            
            // Simple back-face culling (optional)
            // Calculate normal using cross product
            Vec3 edge1 = v2 - v1;
            Vec3 edge2 = v3 - v1;
            float normalZ = edge1.x * edge2.y - edge1.y * edge2.x;
            
            // Only draw if facing camera (you can comment this out if you want to see all faces)
            // if (normalZ > 0) {
                triangle(v1, v2, v3);
                triangleCount++;
            // }
            
            // If it's a quad, draw the second triangle
            if (face.vertexIndices.size() == 4) {
                Vec3 v4 = transformedVertices[face.vertexIndices[3][0]];
                // if (normalZ > 0) {
                    triangle(v1, v3, v4);
                    triangleCount++;
                // }
            }
        }
    }
}

// IMPORTANT: SDL requires the main function to have these exact parameters
int main(int argc, char* argv[]) {
    init();
    
    if (window == nullptr || renderer == nullptr) {
        std::cerr << "Failed to initialize SDL properly" << std::endl;
        return -1;
    }
    
    // Print instructions
    std::cout << "\n=== 3D OBJ Viewer Controls ===" << std::endl;
    std::cout << "Arrow Keys: Rotate model" << std::endl;
    std::cout << "W/S: Zoom in/out" << std::endl;
    std::cout << "A: Toggle auto-rotation" << std::endl;
    std::cout << "R: Reset view" << std::endl;
    std::cout << "1-7: Change colors" << std::endl;
    std::cout << "ESC: Quit" << std::endl;
    std::cout << "================================\n" << std::endl;
    
    // Load the OBJ model once
    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    
    if (!loadOBJ("model.obj", vertices, faces)) {
        std::cerr << "Failed to load OBJ file" << std::endl;
        return -1;
    }
    
    // Set initial viewing angle (diagonal view)
    cameraAngleY = 0.785f;  // 45 degrees in radians
    cameraAngleX = 0.35f;   // 20 degrees in radians
    cameraDistance = 3.0f;  // Distance from object
    
    // Initial render with yellow color
    setColor(Color(255, 255, 0));  // Yellow
    render(vertices, faces);
    renderBuffer(renderer);
    
    bool running = true;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();
    
    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // Auto-rotation if enabled
        if (autoRotate) {
            cameraAngleY += deltaTime * 1.0f;  // Rotate 1 radian per second
            render(vertices, faces);
            renderBuffer(renderer);
        }
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_KEYDOWN) {
                bool needsRender = true;
                
                switch(event.key.keysym.sym) {
                    // Rotation controls
                    case SDLK_LEFT:
                        cameraAngleY -= 0.1f;
                        break;
                    case SDLK_RIGHT:
                        cameraAngleY += 0.1f;
                        break;
                    case SDLK_UP:
                        cameraAngleX -= 0.1f;
                        break;
                    case SDLK_DOWN:
                        cameraAngleX += 0.1f;
                        break;
                        
                    // Zoom controls
                    case SDLK_w:
                        cameraDistance -= 0.2f;
                        if (cameraDistance < 1.0f) cameraDistance = 1.0f;
                        break;
                    case SDLK_s:
                        cameraDistance += 0.2f;
                        if (cameraDistance > 10.0f) cameraDistance = 10.0f;
                        break;
                        
                    // Auto-rotation toggle
                    case SDLK_a:
                        autoRotate = !autoRotate;
                        std::cout << "Auto-rotation: " << (autoRotate ? "ON" : "OFF") << std::endl;
                        break;
                        
                    // Reset view
                    case SDLK_r:
                        cameraAngleY = 0.785f;
                        cameraAngleX = 0.35f;
                        cameraDistance = 3.0f;
                        autoRotate = false;
                        break;
                        
                    // Color controls
                    case SDLK_1:
                        setColor(Color(255, 0, 0));  // Red
                        break;
                    case SDLK_2:
                        setColor(Color(0, 255, 0));  // Green
                        break;
                    case SDLK_3:
                        setColor(Color(0, 0, 255));  // Blue
                        break;
                    case SDLK_4:
                        setColor(Color(255, 255, 0));  // Yellow
                        break;
                    case SDLK_5:
                        setColor(Color(255, 255, 255));  // White
                        break;
                    case SDLK_6:
                        setColor(Color(0, 255, 255));  // Cyan
                        break;
                    case SDLK_7:
                        setColor(Color(255, 0, 255));  // Magenta
                        break;
                        
                    case SDLK_ESCAPE:
                        running = false;
                        needsRender = false;
                        break;
                        
                    default:
                        needsRender = false;
                        break;
                }
                
                if (needsRender) {
                    render(vertices, faces);
                    renderBuffer(renderer);
                }
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