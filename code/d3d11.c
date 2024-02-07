#include "d3d11.h"

// Thanks to mmozeiko for the D3D11 code
// https://gist.github.com/mmozeiko/5e727f845db182d468a34d524508ad5f

internal d3d11_state
InitD3D11(HWND Window, platform_log_message_plain *LogMessagePlain)
{
  HRESULT Result;
  
  ID3D11Device *Device;
  ID3D11DeviceContext *Context;
  
  // Create device and context
  {
    UINT Flags = 0;
    
#if HORIZONS_DEBUG
    Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL Levels[] = {D3D_FEATURE_LEVEL_11_0};
    
    Result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0,
                               Flags, Levels, ArrayCount(Levels),
                               D3D11_SDK_VERSION, &Device, 0, &Context);
    
    AssertHR(Result);
  }
  
#ifdef HORIZONS_DEBUG
  // Set up debug break on errors
  {
    ID3D11InfoQueue *Info;
    ID3D11Device_QueryInterface(Device, &IID_ID3D11InfoQueue, &Info);
    ID3D11InfoQueue_SetBreakOnSeverity(Info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    ID3D11InfoQueue_SetBreakOnSeverity(Info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    ID3D11InfoQueue_Release(Info);
    
    IDXGIInfoQueue *DXGIInfo;
    Result = DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, &DXGIInfo);
    AssertHR(Result);
    IDXGIInfoQueue_SetBreakOnSeverity(DXGIInfo, DXGI_DEBUG_ALL,
                                      DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION,
                                      TRUE);
    IDXGIInfoQueue_SetBreakOnSeverity(DXGIInfo, DXGI_DEBUG_ALL,
                                      DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,
                                      TRUE);
    IDXGIInfoQueue_Release(DXGIInfo);
    
    // NOTE: No need to check results after this because it will automatically
    //       break on errors anyways
  }
#endif
  
  // Create swap chain
  IDXGISwapChain1 *SwapChain;
  {
    IDXGIDevice *DXGIDevice;
    ID3D11Device_QueryInterface(Device, &IID_IDXGIDevice, &DXGIDevice);
    
    IDXGIAdapter *DXGIAdapter;
    IDXGIDevice_GetAdapter(DXGIDevice, &DXGIAdapter);
    
    IDXGIFactory2 *Factory;
    IDXGIAdapter_GetParent(DXGIAdapter, &IID_IDXGIFactory2, &Factory);
    
    DXGI_SWAP_CHAIN_DESC1 Desc =
    {
      .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .SampleDesc = {1, 0},
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2,
      .Scaling = DXGI_SCALING_NONE,
      .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD
    };
    
    IDXGIFactory2_CreateSwapChainForHwnd(Factory, (IUnknown *)Device,
                                         Window, &Desc, 0, 0, &SwapChain);
    
    // Disable weird Alt+Enter behavior
    IDXGIFactory_MakeWindowAssociation(Factory, Window, DXGI_MWA_NO_ALT_ENTER);
    
    IDXGIFactory2_Release(Factory);
    IDXGIAdapter_Release(DXGIAdapter);
    IDXGIDevice_Release(DXGIDevice);
  }
  
  // TODO(evan): Create my own structures for this
  typedef struct vertex
  {
    float Position[2];
    float UV[2];
    float Color[3];
  } vertex;
  
  ID3D11Buffer *VBuffer;
  {
    vertex Data[] =
    {
      { { -0.00f, +0.75f }, { 25.0f, 50.0f }, { 1, 0, 0 } },
      { { +0.75f, -0.50f }, {  0.0f,  0.0f }, { 0, 1, 0 } },
      { { -0.75f, -0.50f }, { 50.0f,  0.0f }, { 0, 0, 1 } }
    };
    
    D3D11_BUFFER_DESC Desc =
    {
      .ByteWidth = sizeof(Data),
      .Usage = D3D11_USAGE_IMMUTABLE,
      .BindFlags = D3D11_BIND_VERTEX_BUFFER
    };
    
    D3D11_SUBRESOURCE_DATA Initial = {.pSysMem = Data};
    ID3D11Device_CreateBuffer(Device, &Desc, &Initial, &VBuffer);
  }
  
  ID3D11InputLayout *Layout;
  ID3D11VertexShader *VShader;
  ID3D11PixelShader *PShader;
  {
    D3D11_INPUT_ELEMENT_DESC Desc[] =
    {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex, UV),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(vertex, Color),    D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    
    const char HLSL[] =
      "#line " STR(__LINE__) "                                  \n\n" // actual line number in this file for nicer error messages
      "                                                           \n"
      "struct VS_INPUT                                            \n"
      "{                                                          \n"
      "     float2 pos   : POSITION;                              \n" // these names must match D3D11_INPUT_ELEMENT_DESC array
      "     float2 uv    : TEXCOORD;                              \n"
      "     float3 color : COLOR;                                 \n"
      "};                                                         \n"
      "                                                           \n"
      "struct PS_INPUT                                            \n"
      "{                                                          \n"
      "  float4 pos   : SV_POSITION;                              \n" // these names do not matter, except SV_... ones
      "  float2 uv    : TEXCOORD;                                 \n"
      "  float4 color : COLOR;                                    \n"
      "};                                                         \n"
      "                                                           \n"
      "cbuffer cbuffer0 : register(b0)                            \n" // b0 = constant buffer bound to slot 0
      "{                                                          \n"
      "    float4x4 uTransform;                                   \n"
      "}                                                          \n"
      "                                                           \n"
      "sampler sampler0 : register(s0);                           \n" // s0 = sampler bound to slot 0
      "                                                           \n"
      "Texture2D<float4> texture0 : register(t0);                 \n" // t0 = shader resource bound to slot 0
      "                                                           \n"
      "PS_INPUT vs(VS_INPUT input)                                \n"
      "{                                                          \n"
      "    PS_INPUT output;                                       \n"
      "    output.pos = mul(uTransform, float4(input.pos, 0, 1)); \n"
      "    output.uv = input.uv;                                  \n"
      "    output.color = float4(input.color, 1);                 \n"
      "    return output;                                         \n"
      "}                                                          \n"
      "                                                           \n"
      "float4 ps(PS_INPUT input) : SV_TARGET                      \n"
      "{                                                          \n"
      "    float4 tex = texture0.Sample(sampler0, input.uv);      \n"
      "    return input.color * tex;                              \n"
      "}                                                          \n";
    
    UINT Flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR|D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS;
    
#if HORIZONS_DEBUG
    Flags |= D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    Flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    
    ID3DBlob *Error;
    
    ID3DBlob *VBlob;
    Result = D3DCompile(HLSL, sizeof(HLSL), 0, 0, 0, "vs", "vs_5_0",
                        Flags, 0, &VBlob, &Error);
    if(FAILED(Result))
    {
      s8 *Message = ID3D10Blob_GetBufferPointer(Error);
      LogMessagePlain(Message, false, MESSAGE_SEVERITY_ERROR);
      LogMessagePlain("Failed to compile vertex shader", false,
                      MESSAGE_SEVERITY_ERROR);
      Assert(0);
    }
    
    ID3DBlob *PBlob;
    Result = D3DCompile(HLSL, sizeof(HLSL), 0, 0, 0, "ps", "ps_5_0",
                        Flags, 0, &PBlob, &Error);
    if(FAILED(Result))
    {
      s8 *Message = ID3D10Blob_GetBufferPointer(Error);
      LogMessagePlain(Message, false, MESSAGE_SEVERITY_ERROR);
      LogMessagePlain("Failed to compile pixel shader", false,
                      MESSAGE_SEVERITY_ERROR);
      Assert(0);
    }
    
    ID3D11Device_CreateVertexShader(Device, ID3D10Blob_GetBufferPointer(VBlob),
                                    ID3D10Blob_GetBufferSize(VBlob), 0, &VShader);
    ID3D11Device_CreatePixelShader(Device, ID3D10Blob_GetBufferPointer(PBlob),
                                   ID3D10Blob_GetBufferSize(PBlob), 0, &PShader);
    ID3D11Device_CreateInputLayout(Device, Desc, ArrayCount(Desc),
                                   ID3D10Blob_GetBufferPointer(VBlob),
                                   ID3D10Blob_GetBufferSize(VBlob), &Layout);
    
    ID3D10Blob_Release(PBlob);
    ID3D10Blob_Release(VBlob);
  }
  
  ID3D11Buffer *UBuffer;
  {
    D3D11_BUFFER_DESC Desc =
    {
      // Space for 4x4 matrix (cbuffer0 from pixel shader)
      .ByteWidth = 4*4*sizeof(f32),
      .Usage = D3D11_USAGE_DYNAMIC,
      .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
      .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
    };
    
    ID3D11Device_CreateBuffer(Device, &Desc, 0, &UBuffer);
  }
  
  ID3D11ShaderResourceView *TextureView;
  {
    u32 Pixels[] =
    {
      0x80000000, 0xffffffff,
      0xffffffff, 0x80000000,
    };
    u32 Width = 2;
    u32 Height = 2;
    
    D3D11_TEXTURE2D_DESC Desc =
    {
      .Width = Width,
      .Height = Height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_IMMUTABLE,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE
    };
    
    D3D11_SUBRESOURCE_DATA Data =
    {
      .pSysMem = Pixels,
      .SysMemPitch = Width*sizeof(u32)
    };
    
    ID3D11Texture2D *Texture;
    ID3D11Device_CreateTexture2D(Device, &Desc, &Data, &Texture);
    ID3D11Device_CreateShaderResourceView(Device, (ID3D11Resource *)Texture,
                                          0, &TextureView);
    ID3D11Texture2D_Release(Texture);
  }
  
  ID3D11SamplerState* Sampler;
  {
    D3D11_SAMPLER_DESC Desc =
    {
      .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
      .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
      .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
      .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
      .MipLODBias = 0,
      .MaxAnisotropy = 1,
      .MinLOD = 0,
      .MaxLOD = D3D11_FLOAT32_MAX,
    };
    
    ID3D11Device_CreateSamplerState(Device, &Desc, &Sampler);
  }
  
  ID3D11BlendState* BlendState;
  {
    // enable alpha blending
    D3D11_BLEND_DESC Desc =
    {
      .RenderTarget[0] =
      {
        .BlendEnable = TRUE,
        .SrcBlend = D3D11_BLEND_SRC_ALPHA,
        .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
        .BlendOp = D3D11_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA,
        .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
        .BlendOpAlpha = D3D11_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
      },
    };
    
    ID3D11Device_CreateBlendState(Device, &Desc, &BlendState);
  }
  
  ID3D11RasterizerState* RasterizerState;
  {
    // disable culling
    D3D11_RASTERIZER_DESC Desc =
    {
      .FillMode = D3D11_FILL_SOLID,
      .CullMode = D3D11_CULL_NONE,
      .DepthClipEnable = TRUE,
    };
    
    ID3D11Device_CreateRasterizerState(Device, &Desc, &RasterizerState);
  }
  
  ID3D11DepthStencilState* DepthState;
  {
    D3D11_DEPTH_STENCIL_DESC Desc =
    {
      .DepthEnable = FALSE,
      .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
      .DepthFunc = D3D11_COMPARISON_LESS,
      .StencilEnable = FALSE,
      .StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK,
      .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
      // .FrontFace = ... 
      // .BackFace = ...
    };
    
    ID3D11Device_CreateDepthStencilState(Device, &Desc, &DepthState);
  }
  
  d3d11_state State =
  {
    Device, Context,
    SwapChain,
    VBuffer,
    Layout, VShader, PShader,
    UBuffer, TextureView,
    Sampler, BlendState, RasterizerState, DepthState,
    0, 0
  };
  return(State);
}

internal void
D3D11Resize(d3d11_state *State, window_dimension New,
            platform_log_message_plain *LogMessagePlain)
{
  if(State->RTView)
  {
    ID3D11DeviceContext_ClearState(State->Context);
    ID3D11RenderTargetView_Release(State->RTView);
    ID3D11DepthStencilView_Release(State->DSView);
    
    State->RTView = 0;
  }
  
  HRESULT Result;
  if(New.Width != 0 && New.Height != 0)
  {
    Result = IDXGISwapChain1_ResizeBuffers(State->SwapChain, 0,
                                           New.Width, New.Height,
                                           DXGI_FORMAT_UNKNOWN, 0);
    if(FAILED(Result))
    {
      LogMessagePlain("Failed to resize swap chain\n", false, MESSAGE_SEVERITY_ERROR);
      Assert(0);
    }
    
    ID3D11Texture2D *BackBuffer;
    IDXGISwapChain1_GetBuffer(State->SwapChain, 0, &IID_ID3D11Texture2D, &BackBuffer);
    ID3D11Device_CreateRenderTargetView(State->Device, (ID3D11Resource *)BackBuffer,
                                        0, &State->RTView);
    ID3D11Texture2D_Release(BackBuffer);
    
    D3D11_TEXTURE2D_DESC Desc =
    {
      .Width = New.Width,
      .Height = New.Height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_D32_FLOAT,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_DEPTH_STENCIL
    };
    
    ID3D11Texture2D *Depth;
    ID3D11Device_CreateTexture2D(State->Device, &Desc, 0, &Depth);
    ID3D11Device_CreateDepthStencilView(State->Device, (ID3D11Resource *)Depth,
                                        0, &State->DSView);
    ID3D11Texture2D_Release(Depth);
  }
}

internal void
D3D11Draw(d3d11_state *State, window_dimension Dimension,
          platform_log_message_plain *LogMessagePlain)
{
  if(State->RTView)
  {
    f32 Color[] = {0.392f, 0.584f, 0.929f, 1.f};
    ID3D11DeviceContext_ClearRenderTargetView(State->Context, State->RTView, Color);
    ID3D11DeviceContext_ClearDepthStencilView(State->Context, State->DSView,
                                              D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,
                                              1.f, 0);
    
    D3D11_VIEWPORT Viewport =
    {
      .Width = (f32)Dimension.Width,
      .Height = (f32)Dimension.Height,
      .MaxDepth = 1
    };
    
    ID3D11DeviceContext_RSSetViewports(State->Context, 1, &Viewport);
    ID3D11DeviceContext_RSSetState(State->Context, State->RasterizerState);
    
    ID3D11DeviceContext_OMSetBlendState(State->Context, State->BlendState, 0, ~0U);
    ID3D11DeviceContext_OMSetDepthStencilState(State->Context, State->DepthState, 0);
    ID3D11DeviceContext_OMSetRenderTargets(State->Context, 1,
                                           &State->RTView, State->DSView);
  }
  
  b32 VSync = true;
  HRESULT Result = IDXGISwapChain1_Present(State->SwapChain, VSync, 0);
  if(Result == DXGI_STATUS_OCCLUDED)
  {
    if(VSync)
    {
      Sleep(10);
    }
  }
  else if(FAILED(Result))
  {
    LogMessagePlain("Failed to present swap chain\n", false, MESSAGE_SEVERITY_ERROR);
    Assert(0);
  }
}