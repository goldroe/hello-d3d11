#include <windows.h>
#include <stdint.h>
#include <assert.h>
#include <crtdbg.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <directxmath.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "user32")
#pragma comment(lib, "winmm.lib")

using namespace DirectX;

struct Constant_Buffer {
    XMMATRIX wvp;
};

int WINDOW_WIDTH = 1280;
int WINDOW_HEIGHT = 720;
bool window_should_close;

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
void win32_process_pending_messages() {
    MSG message;
    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            window_should_close = true;
            break;
        }
        TranslateMessage(&message);
        DispatchMessage(&message);
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

    const int target_fps = 60;
    const DWORD target_ms_per_frame = (DWORD)(1000.0f * (1.0f / target_fps));
    HWND window;
    {
        RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        window = CreateWindow("MainWindowClass", "Graphics", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, NULL, NULL);
    }

    HRESULT hr = S_OK;
    UINT flags = 0;
    
    ////// create device and swap chain ///////////////////////////////////
    IDXGISwapChain *swap_chain;
    ID3D11Device *device;
    ID3D11DeviceContext *device_context;

    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
    swap_chain_desc.BufferDesc.Width = 0;
    swap_chain_desc.BufferDesc.Height = 0;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_0", flags, NULL, &vs_blob, &err_blob);
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

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_0", flags, NULL, &ps_blob, &err_blob);
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
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    ID3D11InputLayout *input_layout = nullptr;
    hr = device->CreateInputLayout(input_desc, ARRAYSIZE(input_desc), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &input_layout);
    assert(SUCCEEDED(hr));
    //////////////////////////////////////////////////////////////////////

    ////////// create vertex buffer //////////////////////////////////////
    float vertices[] = {
        -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
    };
    UINT vertex_offset = 0;
    UINT vertex_stride = 6 * sizeof(float);
    
    ID3D11Buffer *vertex_buffer = nullptr;
    {
        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = sizeof(vertices);
        buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sr_data = {};
        sr_data.pSysMem = vertices;
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
        D3D11_BUFFER_DESC buffer_desc = {.ByteWidth = sizeof(indices), .Usage = D3D11_USAGE_IMMUTABLE, .BindFlags = D3D11_BIND_INDEX_BUFFER};
        D3D11_SUBRESOURCE_DATA sr_data = {.pSysMem = indices};

        hr = device->CreateBuffer(&buffer_desc, &sr_data, &index_buffer);
        assert(SUCCEEDED(hr));
    }
    //////////////////////////////////////////////////////////////////////
    
    /////// create constant buffer ///////////////////////////////////////
    Constant_Buffer cb;
    cb.wvp = XMMatrixIdentity();
    ID3D11Buffer *constant_buffer;
    {
        D3D11_BUFFER_DESC buffer_desc = {.ByteWidth = sizeof(Constant_Buffer), .Usage = D3D11_USAGE_DYNAMIC, .BindFlags = D3D11_BIND_CONSTANT_BUFFER, .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE};
        D3D11_SUBRESOURCE_DATA sr_data = {.pSysMem = &cb};

        hr = device->CreateBuffer(&buffer_desc, &sr_data, &constant_buffer);
        assert(SUCCEEDED(hr));
    }
    //////////////////////////////////////////////////////////////////////
    
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    device_context->IASetInputLayout(input_layout);
    device_context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R32_UINT, 0);
    device_context->IASetVertexBuffers(0, 1, &vertex_buffer, &vertex_stride, &vertex_offset);
    device_context->VSSetShader(vertex_shader, nullptr, 0);
    device_context->VSSetConstantBuffers(0, 1, &constant_buffer);
    device_context->PSSetShader(pixel_shader, nullptr, 0);
    device_context->RSSetViewports(1, &viewport);
    device_context->OMSetRenderTargets(1, &render_target, nullptr);

    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX view = XMMatrixIdentity();
    XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 1.0f, 1000.0f);
    
    LARGE_INTEGER last_counter = win32_get_wall_clock();
    
    while (!window_should_close) {
        win32_process_pending_messages();

        XMVECTOR eye = XMVectorSet(2.0f, 4.0f, 5.0f, 1.0f);
        XMVECTOR target = XMVectorZero();
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        view = XMMatrixLookAtLH(eye, target, up);
        cb.wvp = world * view * proj;
        
        ///////////// update constant buffer ////////////////////////////////////
        D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
        device_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, NULL, &mapped_resource);
        CopyMemory(mapped_resource.pData, &cb, sizeof(cb));
        device_context->Unmap(constant_buffer, 0);
        device_context->VSSetConstantBuffers(0, 1, &constant_buffer);
        /////////////////////////////////////////////////////////////////////////
        
        float back_color[4] = {0.1f, 0.1f, 0.1f, 1.0f};
        device_context->ClearRenderTargetView(render_target, back_color);

        device_context->DrawIndexed(36, 0, 0);
        
        swap_chain->Present(1, 0);

        float work_seconds_elapsed = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
        DWORD work_ms = (DWORD)(1000.0f * work_seconds_elapsed);
        if (work_ms < target_ms_per_frame) {
            DWORD sleep_ms = target_ms_per_frame - work_ms;
            Sleep(sleep_ms);
        }

        LARGE_INTEGER end_counter = win32_get_wall_clock();
        float seconds_elapsed = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
        _RPT1(0, "seconds: %f\n", seconds_elapsed);
        
        last_counter = end_counter;
    }
    
    return 0;
}
