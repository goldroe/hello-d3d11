#include <windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <crtdbg.h>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <directxmath.h>

#include <stdint.h>
typedef int32_t int32;
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "user32")
#pragma comment(lib, "winmm.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

using namespace DirectX;

struct Material {
    XMFLOAT4 ambient;
    XMFLOAT4 diffuse;
    XMFLOAT4 specular;
};

struct Directional_Light {
    XMFLOAT4 ambient;
    XMFLOAT4 diffuse;
    XMFLOAT4 specular;
    
    XMFLOAT3 direction;
    float pad;
};

struct Point_Light {
    XMFLOAT4 ambient;
    XMFLOAT4 diffuse;
    XMFLOAT4 specular;
    
    XMFLOAT3 position;
    float range;
    
    XMFLOAT3 att;
    float pad;
};

struct Spot_Light {
    XMFLOAT4 ambient;
    XMFLOAT4 diffuse;
    XMFLOAT4 specular;
    
    XMFLOAT3 position;
    float range;
    
    XMFLOAT3 direction;
    float spot;
    
    XMFLOAT3 att;
    float pad;
};

struct CB_Per_Frame {
    Directional_Light dir_light;
    Point_Light point_light;
    Spot_Light spot_light;
    XMFLOAT3 eye_pos;
    float pad;
};

struct CB_Per_Object {
    XMMATRIX world;
    XMMATRIX world_inv_transpose;
    XMMATRIX wvp;
    XMMATRIX tex_transform;
    Material material;
};

struct Input {
    bool left;
    bool right;
    bool up;
    bool down;
    POINT last_cursor;
};

struct Mesh_Data {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    XMFLOAT3 origin;
};

float camera_pitch;
float camera_yaw;
float camera_radius = 3.0f;

const int TARGET_FPS = 120;

int WINDOW_WIDTH = 1280;
int WINDOW_HEIGHT = 720;
bool window_should_close;

Mesh_Data load_mesh(const char *filename) {
    Mesh_Data result = {};
    FILE *file = fopen(filename, "rb");
    if(file) {
        fseek(file, 0 , SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0 , SEEK_SET);// needed for next read from beginning of file
    }

    char buf[128];
    char *line = buf;
    float max_x = -100.0f, max_y = -100.0f, max_z = -100.0f;
    float min_x = 100.0f, min_y = 100.0f, min_z = 100.0f;
    while (fgets(line, 60, file) != NULL) {
        char c = *line++;
        if (c == 'v') {
            c = *line;
            if (c == 'n') continue;
            else if (c== 't') continue;
            line += 1;

            float x = 0, y = 0, z = 0;
            float gray = 0.6f;
            float r = gray, g = gray, b = gray;
            sscanf(line, "%f %f %f", &x, &y, &z);
            result.vertices.push_back(x);
            result.vertices.push_back(y);
            result.vertices.push_back(z);
            result.vertices.push_back(r);
            result.vertices.push_back(g);
            result.vertices.push_back(b);

            max_x = (x > max_x) ? x : max_x;
            max_y = (y > max_y) ? y : max_y;
            max_z = (z > max_z) ? z : max_z;
            
            min_x = (x < min_x) ? x : min_x;
            min_y = (y < min_y) ? y : min_y;
            min_z = (z < min_z) ? z : min_z;
        } else if (c == 'f') {
            line++;

            char *tok = strtok(line, " ");
            while (tok != NULL) {
                uint32_t val = (uint32_t)strtol(tok, &tok, 10);
                val -= 1;
                //vertex normal indices (no textures)
                if (strcmp(tok, "//") == 0) {
                    line += 2;
                    val = (uint32_t)strtol(tok, &tok, 10);
                }
                // vertex texture coord indices
                else if (strcmp(tok, "/") == 0) {
                    line += 1;
                    val = (uint32_t)strtol(tok, &tok, 10);
                }
                // simple indices (triangles)
                else {
                    result.indices.push_back(val);
                }
                line += 1; // skip space
                tok = strtok(NULL, " ");
            }
        }
        
        // } else if (c == 'f') {
        //     char word[10];
        //     uint32_t index[3];
        //     uint32_t norm[3];
        //     sscanf(line, "%s %d//%d %d//%d %d//%d", word, &index[0], &norm[0], &index[1], &norm[1], &index[2], &norm[2]);
        //     result.indices.push_back(index[0]-1);
        //     result.indices.push_back(index[1]-1);
        //     result.indices.push_back(index[2]-1);

        //     do {
        //         char *tok = strtok(line, " ");
                
        //     } while (tok != NULL);
        // }
    }
    fclose(file);

    float x = min_x + ((max_x - min_x) / 2.0f);
    float y = min_y + ((max_y - min_y) / 2.0f);
    float z = min_z + ((max_z - min_z) / 2.0f);
    result.origin = XMFLOAT3(x, y, z);
    return result;
}

float clamp(float val, float minval, float maxval) {
    float result = val;
    if (val < minval) result = minval;
    if (val > maxval) result = maxval;
    return result;
}

LARGE_INTEGER performance_frequency;
inline float win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
    float Result = (float)(end.QuadPart - start.QuadPart) / (float)performance_frequency.QuadPart;
    return Result;
}
inline LARGE_INTEGER win32_get_wall_clock() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}


LRESULT CALLBACK main_window_callback(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;
    
    switch (message) {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProc(window, message, wparam, lparam);
    }
    
    return result;
    
}
void win32_process_pending_messages(Input *input) {
    MSG message;
    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            window_should_close = true;
            break;
        } else {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        switch (message.message) {
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(message.lParam);
            int y = GET_Y_LPARAM(message.lParam);

            if (message.wParam & MK_LBUTTON) {
                float pitch = (float)(y - input->last_cursor.y);
                float yaw = (float)(x - input->last_cursor.x);
                pitch *= XM_PI / 200;
                yaw *= XM_PI / 200;
                camera_pitch += pitch;
                camera_yaw += yaw;

                camera_pitch = clamp(camera_pitch, -XM_PIDIV2, XM_PIDIV2);
                _RPT2(0, "pitch: %f yaw: %f\n", camera_pitch, camera_yaw);
            }
            if (message.wParam & MK_RBUTTON) {
                camera_radius -= (x - input->last_cursor.x) / 6.0f;
            }
            
            input->last_cursor.x = x;
            input->last_cursor.y = y;
        } break;
        }
    }

    
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show) {
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&performance_frequency);
    
    WNDCLASS window_class = {};
    window_class.style = CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = main_window_callback;
    window_class.lpszClassName = "MainWindowClass";
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    RegisterClassA(&window_class);

    const DWORD target_ms_per_frame = (DWORD)(1000.0f * (1.0f / TARGET_FPS));
    HWND window;
    {
        RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        window = CreateWindow("MainWindowClass", "Graphics", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, NULL, NULL);
    }

    HRESULT hr = S_OK;
    UINT flags = 0;


    IDXGISwapChain *swap_chain;
    ID3D11Device *device;
    ID3D11DeviceContext *device_context;

    ////// create device and swap chain ///////////////////////////////////
    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
    swap_chain_desc.BufferDesc.Width = 0;
    swap_chain_desc.BufferDesc.Height = 0;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.OutputWindow = window;
    swap_chain_desc.Windowed = true;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, nullptr, 0, D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, nullptr, &device_context);
    //////////////////////////////////////////////////////////////////////

    ////// create render target /////////////////////////////////////////
    ID3D11RenderTargetView *render_target;
    ID3D11Texture2D *backbuffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);
    device->CreateRenderTargetView(backbuffer, nullptr, &render_target);
    //////////////////////////////////////////////////////////////////////

    ////// create viewport ///////////////////////////////////////////////
    swap_chain->GetDesc(&swap_chain_desc);
    
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (float)swap_chain_desc.BufferDesc.Width;
    viewport.Height = (float)swap_chain_desc.BufferDesc.Height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    //////////////////////////////////////////////////////////////////////

    /////// compile shaders //////////////////////////////////////////////
    flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob *vs_blob = nullptr, *ps_blob = nullptr, *err_blob = nullptr;
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main", "vs_5_0", flags, NULL, &vs_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) {
            OutputDebugStringA((char *)err_blob->GetBufferPointer());
            err_blob->Release();
        }
        if (vs_blob) {
            vs_blob->Release();
        }
        assert(false);
    }

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", "ps_5_0", flags, NULL, &ps_blob, &err_blob);
    if (FAILED(hr)) {
        if (err_blob) {
            OutputDebugStringA((char *)err_blob->GetBufferPointer());
            err_blob->Release();
        }
        if (ps_blob) {
            ps_blob->Release();
        }
        assert(false);
    }
    //////////////////////////////////////////////////////////////////////

    ////////// create shaders ////////////////////////////////////////////
    ID3D11VertexShader *vertex_shader = nullptr;
    ID3D11PixelShader *pixel_shader = nullptr;
    hr = device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vertex_shader);
    assert(SUCCEEDED(hr));
    
    hr = device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &pixel_shader);
    assert(SUCCEEDED(hr));
    //////////////////////////////////////////////////////////////////////
    
    ////////// create input layout ///////////////////////////////////////
    D3D11_INPUT_ELEMENT_DESC input_desc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    ID3D11InputLayout *input_layout = nullptr;
    hr = device->CreateInputLayout(input_desc, ARRAYSIZE(input_desc), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &input_layout);
    assert(SUCCEEDED(hr));
    //////////////////////////////////////////////////////////////////////
    
    // Mesh_Data mesh = {};
    // mesh = load_mesh("meshes/octahedron.obj");

    ////////// create vertex buffer //////////////////////////////////////
    /*
    float vertices[] = {
        // positions        
        -1.0f, -1.0f, -1.0f,   
        -1.0f,  1.0f, -1.0f,  
         1.0f,  1.0f, -1.0f,  
         1.0f, -1.0f, -1.0f,  
        -1.0f, -1.0f, 1.0f,   
        -1.0f,  1.0f, 1.0f,   
         1.0f,  1.0f, 1.0f,   
         1.0f, -1.0f, 1.0f,   
    };
    */
    UINT vertex_offset = 0;
    UINT vertex_stride = 8 * sizeof(float);

    float vertices[] = {
        // front
        -1.0f, -1.0f, -1.0f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,  0.0f, 0.0f, -1.0f,  0.0f, 1.0f,
        1.0f,  1.0f, -1.0f,   0.0f, 0.0f, -1.0f,  1.0f, 1.0f,
         
        -1.0f, -1.0f, -1.0f,  0.0f, 0.0f, -1.0f,  0.0f, 0.0f,
        1.0f,  1.0f, -1.0f,   0.0f, 0.0f, -1.0f,  1.0f, 1.0f,
        1.0f, -1.0f, -1.0f,   0.0f, 0.0f, -1.0f,  1.0f, 0.0f,
        // back
        -1.0f, -1.0f, 1.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
        1.0f,  1.0f, 1.0f,    0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f, 1.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,
        
        -1.0f, -1.0f, 1.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
        1.0f,  -1.0f, 1.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
        1.0f, 1.0f, 1.0f,     0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        // top
        -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
        -1.0f,  1.0f, 1.0f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        1.0f,  1.0f, 1.0f,    0.0f, 1.0f, 0.0f,   1.0f, 0.0f,

        -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
        1.0f,  1.0f, 1.0f,    0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        1.0f,  1.0f, -1.0f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        // bottom
        1.0f, -1.0f, -1.0f,   0.0f, -1.0f, 0.0f,  0.0f, 1.0f,
        1.0f, -1.0f, 1.0f,    0.0f, -1.0f, 0.0f,  0.0f, 0.0f,
        -1.0f, -1.0f, 1.0f,   0.0f, -1.0f, 0.0f,  1.0f, 0.0f,

        1.0f, -1.0f, -1.0f,   0.0f, -1.0f, 0.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,   0.0f, -1.0f, 0.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, -1.0f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f,
        // left
        -1.0f, -1.0f, 1.0f,   -1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
        -1.0f,  1.0f, 1.0f,   -1.0f, 0.0f, 0.0f,  0.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f, 0.0f, 0.0f,  1.0f, 1.0f,

        -1.0f, -1.0f, 1.0f,   -1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f, 0.0f, 0.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
        // right
        1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
        1.0f,  1.0f, -1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f,
        1.0f,  1.0f, 1.0f,    1.0f, 0.0f, 0.0f,   1.0f, 1.0f,

        1.0f, -1.0f, -1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
        1.0f,  1.0f, 1.0f,    1.0f, 0.0f, 0.0f,   1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,    1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
    };


    ID3D11Buffer *vertex_buffer = NULL;
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = (UINT)sizeof(vertices);
        buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sr_data = {};
        sr_data.pSysMem = &vertices;
        sr_data.SysMemPitch = 0;
        sr_data.SysMemSlicePitch = 0;

        hr = device->CreateBuffer(&buffer_desc, &sr_data, &vertex_buffer);
        assert(SUCCEEDED(hr));
    }
    //////////////////////////////////////////////////////////////////////

    /////// create index buffer     //////////////////////////////////////
    uint32_t indices[] = {
        // front
        0, 1, 2,
        0, 2, 3,
        // back
        4, 6, 5,
        4, 7, 6,
        // top
        1, 5, 6,
        1, 6, 2,
        // bottom
        3, 7, 4,
        3, 4, 0,
        // left
        4, 5, 1,
        4, 1, 0,
        // right
        3, 2, 6,
        3, 6, 7,
    };

    ID3D11Buffer *index_buffer = nullptr;
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = (UINT)sizeof(indices) * sizeof(uint32_t);
        buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
        buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sr_data = {};
        sr_data.pSysMem = &indices[0];

        hr = device->CreateBuffer(&buffer_desc, &sr_data, &index_buffer);
        assert(SUCCEEDED(hr));
    }
    //////////////////////////////////////////////////////////////////////
    
    ID3D11Buffer *frame_cbuffer = nullptr;
    ID3D11Buffer *object_cbuffer = nullptr;
    
    /////// create constant buffer ///////////////////////////////////////
    CB_Per_Frame cb_frame = {};
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = sizeof(cb_frame);
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA sr_data = {};
        sr_data.pSysMem = &cb_frame;
        hr = device->CreateBuffer(&buffer_desc, &sr_data, &frame_cbuffer);
        assert(SUCCEEDED(hr));
    }
    
    CB_Per_Object cb_object = {};
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = sizeof(cb_object);
        buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
        buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA sr_data = {};
        sr_data.pSysMem = &cb_frame;
        hr = device->CreateBuffer(&buffer_desc, &sr_data, &object_cbuffer);
        assert(SUCCEEDED(hr));
    }
    //////////////////////////////////////////////////////////////////////
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_context->IASetInputLayout(input_layout);
    device_context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0);
    device_context->IASetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
    device_context->VSSetShader(vertex_shader, nullptr, 0);
    device_context->VSSetConstantBuffers(0, 1, &frame_cbuffer);
    device_context->VSSetConstantBuffers(0, 1, &object_cbuffer);
    device_context->PSSetShader(pixel_shader, nullptr, 0);
    device_context->PSSetConstantBuffers(0, 1, &frame_cbuffer);
    device_context->PSSetConstantBuffers(0, 1, &object_cbuffer);
    device_context->RSSetViewports(1, &viewport);
    device_context->OMSetRenderTargets(1, &render_target, nullptr);

    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX view = XMMatrixIdentity();
    XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 1.0f, 1000.0f);

    ID3D11Texture2D *texture = nullptr;
    ID3D11ShaderResourceView *resource_view = nullptr;
    ID3D11SamplerState *sampler_state = nullptr;
    {
        uint8_t *image_data = nullptr;
        int x, y, n;
        stbi_set_flip_vertically_on_load(true);
        image_data = stbi_load("textures/FireAnim/Fire001.bmp", &x, &y, &n, 4);
        assert(image_data);
        
        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = x;
        tex_desc.Height = y;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
        tex_desc.CPUAccessFlags = 0;
        tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA sr_data = {};
        sr_data.pSysMem = image_data;
        sr_data.SysMemPitch = x * 4;

        hr = device->CreateTexture2D(&tex_desc, &sr_data, &texture);
        assert(SUCCEEDED(hr));

        hr = device->CreateShaderResourceView(texture, nullptr, &resource_view);
        assert(SUCCEEDED(hr));

        D3D11_SAMPLER_DESC sampler_desc = {};
        sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
        sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampler_desc.MaxAnisotropy = 4;
        sampler_desc.MinLOD = -FLT_MAX;
        sampler_desc.MaxLOD = FLT_MAX;
        sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        // FLOAT                      MipLODBias;
        // FLOAT                      BorderColor[4];
        hr =  device->CreateSamplerState(&sampler_desc, &sampler_state);
        assert(SUCCEEDED(hr));
    }

    // load fire textures
    ID3D11ShaderResourceView **fire_views = nullptr;
    char file_name[64] = "textures/FireAnim/Fire001.bmp";
    for (int32 i = 1; i <= 120; i++) {
        sprintf(file_name, "textures/FireAnim/Fire%03d.bmp", i);
        
        uint8_t *image_data = nullptr;
        int x, y, n;
        stbi_set_flip_vertically_on_load(true);
        image_data = stbi_load(file_name, &x, &y, &n, 4);
        assert(image_data);
        
        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = x;
        tex_desc.Height = y;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
        tex_desc.CPUAccessFlags = 0;
        tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA sr_data = {};
        sr_data.pSysMem = image_data;
        sr_data.SysMemPitch = x * 4;

        hr = device->CreateTexture2D(&tex_desc, &sr_data, &texture);
        assert(SUCCEEDED(hr));
        
        hr = device->CreateShaderResourceView(texture, nullptr, &resource_view);
        assert(SUCCEEDED(hr));

        arrpush(fire_views, resource_view);
    }

    
    Input input = {};
    LARGE_INTEGER last_counter = win32_get_wall_clock();
    int fire_index = 0;
    
    LARGE_INTEGER start_counter;
    QueryPerformanceCounter(&start_counter);

    while (!window_should_close) {
        win32_process_pending_messages(&input);

        fire_index %= 120; // 120 fire animation files
        _RPT1(0, "INDEX: %d\n", fire_index);
        resource_view = fire_views[fire_index];
        
        // float back_color[4] = {0.42f, 0.51f, 0.54f, 1.0f};
        float back_color[4] = {0.002f, 0.002f, 0.002f, 1.0f};
        device_context->ClearRenderTargetView(render_target, back_color);
        
        device_context->PSSetShaderResources(0, 1, &resource_view);
        device_context->PSSetSamplers(0, 1, &sampler_state);
        
        XMMATRIX camera_rot = XMMatrixRotationQuaternion (XMQuaternionRotationRollPitchYaw(camera_pitch, camera_yaw, 0.0f));
        XMVECTOR camera_origin = XMVectorSet(0.0f, 1.0f, -1.0f * camera_radius, 1.0f);
        XMVECTOR camera_pos = XMVector3Transform(camera_origin, camera_rot);

        // XMVECTOR target = XMLoadFloat3(&mesh.origin);
        XMVECTOR target = XMVectorZero();
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        view = XMMatrixLookAtLH(camera_pos, target, up);

        Directional_Light dir_light;
        // dir_light.ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        // dir_light.diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        // dir_light.specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
        dir_light.ambient = XMFLOAT4(0.4f, 0.4f, 0.0f, 1.0f);
        dir_light.diffuse = XMFLOAT4(0.8f, 0.8f, 0.0f, 1.0f);
        dir_light.specular = XMFLOAT4(0.73f, 0.73f, 0.73f, 1.0f);
        dir_light.direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
        // XMFLOAT4 vec;
        // XMStoreFloat4(&vec, camera_pos);
        // cb_frame.eye_pos = XMFLOAT3(vec.x, vec.y, vec.z);
        // XMStoreFloat3(&cb_frame.dir_light.direction, XMVector3Normalize(target - camera_pos));

        Point_Light point_light;
        point_light.ambient = XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f);
        point_light.diffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
        point_light.specular = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
        point_light.att = XMFLOAT3(0.0f, 0.1f, 0.0f);
        point_light.range = 25.0f;

        // point_light.position.x = 10.0f * cosf(0.7f * win32_get_seconds_elapsed(start_counter, win32_get_wall_clock()));
        // point_light.position.z = 10.0f * sinf(0.7f * win32_get_seconds_elapsed(start_counter, win32_get_wall_clock()));
        point_light.position.y = 0.0f;

        Spot_Light spot_light;
        spot_light.ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        spot_light.diffuse = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
        spot_light.specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        XMFLOAT4 v;
        XMStoreFloat4(&v, camera_pos);
        spot_light.position = XMFLOAT3(v.x, v.y, v.z);
        XMStoreFloat3(&spot_light.direction, XMVector3Normalize(target - camera_pos));
        spot_light.range = 10000.0f;
        spot_light.spot = 192.0f;
        spot_light.att = XMFLOAT3(1.0f, 0.0f, 0.0f);

        Material material = {};
        material.ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.specular = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

        cb_frame.point_light = point_light;
        cb_frame.dir_light = dir_light;
        cb_frame.spot_light = spot_light;
        
        cb_object.world = world;
        cb_object.wvp = world * view * proj;
        cb_object.world_inv_transpose = XMMatrixTranspose(XMMatrixInverse(nullptr, cb_object.world));
        cb_object.tex_transform = XMMatrixIdentity();
        cb_object.material = material;

        ///////////// update constant buffer ////////////////////////////////////
        D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
        device_context->Map(frame_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &mapped_resource);
        CopyMemory(mapped_resource.pData, &cb_frame, sizeof(cb_frame));
        device_context->Unmap(frame_cbuffer, 0);

        mapped_resource = {};
        device_context->Map(object_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &mapped_resource);
        CopyMemory(mapped_resource.pData, &cb_object, sizeof(cb_object));
        device_context->Unmap(object_cbuffer, 0);

        device_context->VSSetConstantBuffers(0, 1, &frame_cbuffer);
        device_context->VSSetConstantBuffers(1, 1, &object_cbuffer);
        device_context->PSSetConstantBuffers(0, 1, &frame_cbuffer);
        device_context->PSSetConstantBuffers(1, 1, &object_cbuffer);
        /////////////////////////////////////////////////////////////////////////

        device_context->Draw(36, 0);
        swap_chain->Present(1, 0);

        
        float work_seconds_elapsed = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
        DWORD work_ms = (DWORD)(1000.0f * work_seconds_elapsed);
        if (work_ms < target_ms_per_frame) {
            DWORD sleep_ms = target_ms_per_frame - work_ms;
            Sleep(sleep_ms);
        }

        LARGE_INTEGER end_counter = win32_get_wall_clock();
#if 1
        float seconds_elapsed = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
        _RPT1(0, "seconds: %f\n", seconds_elapsed);
#endif
        
        last_counter = end_counter;
        
        fire_index += 1;
    }
    
    return 0;
}
