int _fltused;

#include "types.h"
#include "platform.h"

#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <xinput.h>

#include "win32.h"

internal inline window_dimension
Win32GetWindowDimension(HWND Window)
{
  window_dimension Dimension = {0};
  
  RECT ClientRect;
  GetClientRect(Window, &ClientRect);
  Dimension.Width = ClientRect.right - ClientRect.left;
  Dimension.Height = ClientRect.bottom - ClientRect.top;
  return(Dimension);
}

#define AssertHR(HR) Assert(SUCCEEDED(HR))

global win32_state *GlobalState;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
  return(ERROR_DEVICE_NOT_CONNECTED);
}
global x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
  return(ERROR_DEVICE_NOT_CONNECTED);
}
global x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal PLATFORM_OUTPUT_STRING(Win32OutputString)
{
  OutputDebugStringA(Text);
}

internal void
Win32LoadXInput(void)
{
  HMODULE XInputLibrary = LoadLibraryA("Xinput9_1_0.dll");
  if(!XInputLibrary)
  {
    XInputLibrary = LoadLibraryA("Xinput1_4.dll");
    if(!XInputLibrary)
    {
      XInputLibrary = LoadLibraryA("Xinput1_3.dll");
    }
  }
  
  if(XInputLibrary)
  {
    XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

internal inline FILETIME
Win32GetLastWriteTime(s8 *FileName)
{
  FILETIME LastWriteTime = {0};
  
  WIN32_FILE_ATTRIBUTE_DATA Data;
  if(GetFileAttributesExA(FileName, GetFileExInfoStandard, &Data))
  {
    LastWriteTime = Data.ftLastWriteTime;
  }
  
  return(LastWriteTime);
}

internal win32_game_code
Win32LoadGameCode(s8 *SourceDllName, s8 *TempDllName, s8 *LockName)
{
  win32_game_code Result = {0};
  
  Result.LastWriteTime = Win32GetLastWriteTime(SourceDllName);
  CopyFileA(SourceDllName, TempDllName, false);
  
  WIN32_FILE_ATTRIBUTE_DATA Ignored;
  if(!GetFileAttributesExA(LockName, GetFileExInfoStandard, &Ignored))
  {
    Result.GameDll = LoadLibraryA(TempDllName);
    if(Result.GameDll)
    {
      Result.GameUpdateAndRender =
      (game_update_and_render *)GetProcAddress(Result.GameDll, "GameUpdateAndRender");
      
      Result.IsValid = Result.GameUpdateAndRender != 0;
    }
  }
  
  if(!Result.IsValid)
  {
    Result.GameUpdateAndRender = GameUpdateAndRenderStub;
    Result.LastWriteTime = (FILETIME){0};
  }
  
  return(Result);
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
  if(GameCode->GameDll)
  {
    FreeLibrary(GameCode->GameDll);
    GameCode->GameDll = 0;
  }
  
  GameCode->IsValid = false;
  GameCode->GameUpdateAndRender = GameUpdateAndRenderStub;
}

internal PLATFORM_OPEN_FILE(Win32OpenFile)
{
  DWORD AccessFlags = 0;
  DWORD ShareMode = 0;
  if(Flags & FILE_OPEN_READ)
  {
    AccessFlags |= GENERIC_READ;
    ShareMode |= FILE_SHARE_READ;
  }
  if(Flags & FILE_OPEN_WRITE)
  {
    AccessFlags |= GENERIC_WRITE;
    ShareMode |= FILE_SHARE_WRITE;
  }
  
  HANDLE File = CreateFileA(FileName, AccessFlags, ShareMode, 0,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  
  return(File);
}

internal PLATFORM_GET_FILE_SIZE(Win32GetFileSize)
{
  u32 Result = 0;
  
  if(Handle != INVALID_HANDLE_VALUE)
  {
    LARGE_INTEGER FileSize;
    if(GetFileSizeEx(Handle, &FileSize))
    {
      Result = SafeTruncateUInt64(FileSize.QuadPart);
    }
    else
    {
      // TODO(evan): Logging
    }
  }
  else
  {
    // TODO(evan): Logging
  }
  
  return(Result);
}

internal PLATFORM_READ_ENTIRE_FILE(Win32ReadEntireFile)
{
  if(Handle != INVALID_HANDLE_VALUE)
  {
    if(Dest)
    {
      DWORD BytesRead;
      if(ReadFile(Handle, Dest, FileSize, &BytesRead, 0) &&
         BytesRead == FileSize)
      {
      }
      else
      {
        // TODO(evan): Logging
      }
    }
    else
    {
      // TODO(evan): Logging
    }
  }
  else
  {
    // TODO(evan): Logging
  }
}

internal PLATFORM_WRITE_ENTIRE_FILE(Win32WriteEntireFile)
{
  b32 Result = false;
  
  HANDLE File = *((HANDLE *)Handle);
  if(File != INVALID_HANDLE_VALUE)
  {
    DWORD BytesWritten;
    if(WriteFile(File, Data, DataSize, &BytesWritten, 0) &&
       BytesWritten == DataSize)
    {
      Result = true;
    }
    else
    {
      // TODO(evan): Logging
    }
  }
  else
  {
    // TODO(evan): Logging
  }
  
  return(Result);
}

internal PLATFORM_CLOSE_FILE(Win32CloseFile)
{
  CloseHandle(Handle);
}

internal inline LARGE_INTEGER
Win32GetWallClock(void)
{
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);
  return(Result);
}

internal inline f32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
  f32 Result = ((f32)(End.QuadPart - Start.QuadPart) /
                (f32)GlobalState->PerfCountFrequency);
  return(Result);
}

internal PLATFORM_COPY_MEMORY(Win32CopyMemory)
{
  RtlCopyMemory(Dest, Source, Length);
}

internal PLATFORM_ZERO_MEMORY(Win32ZeroMemory)
{
  RtlZeroMemory(Dest, Length);
}

internal LRESULT CALLBACK
Win32WindowProc(HWND Window,
                UINT Message,
                WPARAM WParam,
                LPARAM LParam);

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CmdLine,
        int CmdShow)
{
  // Assert((uint64)(*((int64*)__readgsqword(0x60) + 0x23)) >= 10);
  
  memory EngineMemory = {0};
  {
#if HORIZONS_INTERNAL
    LPVOID BaseAddress = (LPVOID)Tebibytes(2);
#else
    LPVOID BaseAddress = 0;
#endif
    
    EngineMemory.PermanentStorageSize = Mebibytes(64);
    EngineMemory.TempStorageSize = Gibibytes(1);
    
    EngineMemory.OpenFile = Win32OpenFile;
    EngineMemory.GetFileSize = Win32GetFileSize;
    EngineMemory.ReadEntireFile = Win32ReadEntireFile;
    EngineMemory.WriteEntireFile = Win32WriteEntireFile;
    EngineMemory.CloseFile = Win32CloseFile;
    EngineMemory.OutputString = Win32OutputString;
    
    memory_index PlatformMemorySize = Mebibytes(64);
    
    memory_index TotalEngineMemorySize = EngineMemory.PermanentStorageSize + EngineMemory.TempStorageSize + PlatformMemorySize;
    GlobalState = VirtualAlloc(BaseAddress, sizeof(win32_state) + TotalEngineMemorySize,
                               MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    
    InitializeArena(&GlobalState->Arena, (u8 *)GlobalState + sizeof(win32_state), PlatformMemorySize);
    EngineMemory.PermanentStorage = GlobalState->Arena.Memory + PlatformMemorySize;
    EngineMemory.TempStorage = (u8 *)EngineMemory.PermanentStorage + EngineMemory.PermanentStorageSize;
  }
  
  LARGE_INTEGER PerfCountFrequencyResult;
  QueryPerformanceFrequency(&PerfCountFrequencyResult);
  GlobalState->PerfCountFrequency = PerfCountFrequencyResult.QuadPart;
  
  // NOTE(evan): Set the Windows schedular granularity to 1ms
  // so that Sleep() can be more granular
  UINT DesiredSchedularMS = 1;
  b32 SleepIsGranular = (timeBeginPeriod(DesiredSchedularMS) == TIMERR_NOERROR);
  
  Win32LoadXInput();
  
  WNDCLASSW WindowClass = {0};
  WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
  WindowClass.lpfnWndProc = Win32WindowProc;
  WindowClass.hInstance = Instance;
  WindowClass.hCursor = LoadCursorW(0, IDC_ARROW);
  WindowClass.lpszClassName = L"WindowClass";
  
  s8 *GameCodeDllName = "engine.dll";
  s8 *GameCodeTempDllName = "engine_temp.dll";
  s8 *GameCodeLockName = "lock.tmp";
  
  if(RegisterClassW(&WindowClass))
  {
    GlobalState->Window = CreateWindowW(WindowClass.lpszClassName, L"Engine",
                                        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 700,
                                        0, 0, Instance, 0);
    
    GlobalState->ShowCursor = true;
    
    if(GlobalState->Window)
    {
      HDC DeviceContext = GetDC(GlobalState->Window);
      
      s32 MonitorRefreshHz = 60;
      s32 Win32RefreshRate = GetDeviceCaps(DeviceContext, VREFRESH);
      if(Win32RefreshRate > 1)
      {
        MonitorRefreshHz = Win32RefreshRate;
      }
      f32 GameUpdateHz = ((f32)MonitorRefreshHz);
      f32 TargetSecondsPerFrame = 1.0f / (f32)GameUpdateHz;
      
      win32_game_code Game = Win32LoadGameCode(GameCodeDllName, GameCodeTempDllName, GameCodeLockName);
      
      if(EngineMemory.PermanentStorage)
      {
        GlobalState->WindowDimension = Win32GetWindowDimension(GlobalState->Window);
        
        LARGE_INTEGER LastCounter = Win32GetWallClock();
        b32 ShouldClose = false;
        while(!ShouldClose)
        {
          f32 DeltaTime;
          {
            LARGE_INTEGER EndCounter = Win32GetWallClock();
            DeltaTime = Win32GetSecondsElapsed(LastCounter, EndCounter);
            LastCounter = EndCounter;
          }
          
          {
            FILETIME NewDllWriteTime = Win32GetLastWriteTime(GameCodeDllName);
            if(CompareFileTime(&Game.LastWriteTime, &NewDllWriteTime))
            {
              Win32UnloadGameCode(&Game);
              Game = Win32LoadGameCode(GameCodeDllName, GameCodeTempDllName, GameCodeLockName);
            }
          }
          
          GlobalState->GameInput.Keyboard.Backspace = false;
          {
            MSG Message;
            while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
            {
              TranslateMessage(&Message);
              DispatchMessageA(&Message);
            }
          }
          
          {
            controller_input *Input = &GlobalState->GameInput.Controller;
            
            XINPUT_STATE ControllerState = {0};
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
            if(XInputGetState(0, &ControllerState) == ERROR_SUCCESS)
            {
              Input->Connected = true;
              
              Input->Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
              Input->Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
              Input->Left= Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
              Input->Right= Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
              
              Input->Start = Pad->wButtons & XINPUT_GAMEPAD_START;
              
              Input->LeftShoulder= Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
              Input->RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
              
              Input->A = Pad->wButtons & XINPUT_GAMEPAD_A;
              Input->B = Pad->wButtons & XINPUT_GAMEPAD_B;
              Input->X = Pad->wButtons & XINPUT_GAMEPAD_X;
              Input->Y = Pad->wButtons & XINPUT_GAMEPAD_Y;
              
              Input->LeftTrigger = Pad->bLeftTrigger;
              Input->RightTrigger = Pad->bLeftTrigger;
              
              Input->LeftThumbY = (Pad->sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ? Pad->sThumbLX : 0;
              Input->LeftThumbX = (Pad->sThumbLY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ? Pad->sThumbLY : 0;
              
              XINPUT_VIBRATION Vibration;
              Vibration.wLeftMotorSpeed = Input->LeftVibration;
              Vibration.wRightMotorSpeed = Input->RightVibration;
              XInputSetState(0, &Vibration);
            }
            else
            {
              Input->Connected = false;
              // NOTE(evan): Controller not connected, this is NOT an error
            }
          }
          
          {
            window_dimension NewWindowDimension = Win32GetWindowDimension(GlobalState->Window);
            b32 WindowResized = false;
            if(NewWindowDimension.Width != GlobalState->WindowDimension.Width ||
               NewWindowDimension.Height != GlobalState->WindowDimension.Height)
            {
              WindowResized = true;
              GlobalState->WindowDimension = NewWindowDimension;
            }
          }
          
          if(GlobalState->ShowCursor)
          {
            RECT WindowRect;
            GetClientRect(GlobalState->Window, &WindowRect);
            
            POINT Center = { WindowRect.right/2, WindowRect.bottom/2 };
            ClientToScreen(GlobalState->Window, &Center);
            // SetCursorPos(Center.x, Center.y);
          }
          
          ShouldClose = Game.GameUpdateAndRender(GlobalState->WindowDimension,
                                                 &EngineMemory, &GlobalState->GameInput, DeltaTime);
        }
      }
    }
    else
    {
      // TODO(evan): Logging
      return(1);
    }
  }
  else
  {
    // TODO(evan): Logging
    return(1);
  }
  
  return(0);
}

void __stdcall
WinMainCRTStartup(void)
{
  int Result = WinMain(GetModuleHandle(0), 0, 0, 0);
  ExitProcess(Result);
}

internal LRESULT CALLBACK
Win32WindowProc(HWND Window,
                UINT Message,
                WPARAM WParam,
                LPARAM LParam)
{
  LRESULT Result = 0;
  
  switch(Message)
  {
    case WM_SETCURSOR:
    {
      if(GlobalState->ShowCursor)
      {
        Result = DefWindowProc(Window, Message, WParam, LParam);
      }
      else
      {
        ShowCursor(0);
      }
    } break;
    
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
      u32 VKCode = (u32)WParam;
      b32 WasDown = LParam & (1 << 30);
      b32 IsDown = (LParam & (1 << 31)) == 0;
      
      if(WasDown != IsDown)
      {
        switch(VKCode)
        {
          case 'W':
          {
            GlobalState->GameInput.Keyboard.W = IsDown;
          } break;
          
          case 'A':
          {
            GlobalState->GameInput.Keyboard.A = IsDown;
          } break;
          
          case 'S':
          {
            GlobalState->GameInput.Keyboard.S = IsDown;
          } break;
          
          case 'D':
          {
            GlobalState->GameInput.Keyboard.D = IsDown;
          } break;
          
          case 'Q':
          {
            GlobalState->GameInput.Keyboard.Q = IsDown;
          } break;
          
          case 'E':
          {
            GlobalState->GameInput.Keyboard.E = IsDown;
          } break;
          
          case 'M':
          {
            GlobalState->GameInput.Keyboard.M = IsDown;
          } break;
          
          case VK_UP:
          {
            GlobalState->GameInput.Keyboard.Up = IsDown;
          } break;
          
          case VK_LEFT:
          {
            GlobalState->GameInput.Keyboard.Left = IsDown;
          } break;
          
          case VK_DOWN:
          {
            GlobalState->GameInput.Keyboard.Down = IsDown;
          } break;
          
          case VK_RIGHT:
          {
            GlobalState->GameInput.Keyboard.Right = IsDown;
          } break;
          
          case VK_BACK:
          {
            if(IsDown)
            {
              GlobalState->GameInput.Keyboard.Backspace = true;
            }
          } break;
          
          case VK_ESCAPE:
          {
            GlobalState->GameInput.Keyboard.Escape = IsDown;
          } break;
          
          case VK_DELETE:
          {
            GlobalState->GameInput.Keyboard.Delete = IsDown;
          } break;
          
          case VK_SPACE:
          {
            GlobalState->GameInput.Keyboard.Space = IsDown;
          } break;
          
          case VK_RETURN:
          {
            GlobalState->GameInput.Keyboard.Enter = IsDown;
          } break;
          
          case 0x31:
          {
            GlobalState->GameInput.Keyboard.One = IsDown;
          } break;
          
          case 0x32:
          {
            GlobalState->GameInput.Keyboard.Two = IsDown;
          } break;
          
          case 0x33:
          {
            GlobalState->GameInput.Keyboard.Three = IsDown;
          } break;
        }
      }
      
      if(IsDown)
      {
        b32 AltKeyIsDown = (LParam & (1 << 29));
        GlobalState->GameInput.Keyboard.AltF4 = false;
        if(AltKeyIsDown && VKCode == VK_F4)
        {
          GlobalState->GameInput.Keyboard.AltF4 = true;
        }
      }
    } break;
    
    case WM_LBUTTONDOWN:
    {
      GlobalState->GameInput.Mouse.LButton = true;
    } break;
    
    case WM_LBUTTONUP:
    {
      GlobalState->GameInput.Mouse.LButton = false;
    } break;
    
    case WM_RBUTTONDOWN:
    {
      GlobalState->GameInput.Mouse.RButton = true;
    } break;
    
    case WM_RBUTTONUP:
    {
      GlobalState->GameInput.Mouse.RButton = false;
    } break;
    
    case WM_MOUSEMOVE:
    {
      GlobalState->GameInput.Mouse.X = GET_X_LPARAM(LParam);
      GlobalState->GameInput.Mouse.Y = GET_Y_LPARAM(LParam);
    } break;
    
    case WM_CLOSE:
    {
      GlobalState->WindowClosed = true;
    } break;
    
    case WM_DESTROY:
    {
      GlobalState->WindowClosed = true;;
    } break;
    
    default:
    {
      Result = DefWindowProc(Window, Message, WParam, LParam);
    } break;
  }
  
  return(Result);
}
