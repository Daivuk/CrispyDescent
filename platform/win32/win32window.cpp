
#ifndef USE_SDL

#include "platform/mouse.h"
#include "platform/timer.h"
#include "platform/mouse.h"
#include "platform/key.h"
#include "platform/joy.h"
#include "2d/gr.h"
#include "platform/platform.h"
#include "misc/error.h"
#include "misc/types.h"
#include "win32joystick.h"
#include <string>
#include <vector>
#include <cmath>

#include <Windows.h>
#include <windowsx.h>
#include <d3d9.h>

#pragma comment(lib, "d3d9.lib")

#define APART(x) (static_cast<uint32_t>(x) >> 24)
#define RPART(x) ((static_cast<uint32_t>(x) >> 16) & 0xff)
#define GPART(x) ((static_cast<uint32_t>(x) >> 8) & 0xff)
#define BPART(x) (static_cast<uint32_t>(x) & 0xff)
#define MAKEARGB(a,r,g,b) ((static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))

// Display API:

// < [crispy] draw list
static std::vector<crispy_vertex_t> crispy_vertices;
struct crispy_draw_cmd_t
{
	IDirect3DTexture9* texture;
	int vertices_idx;
	int count;
};
static std::vector<crispy_draw_cmd_t> crispy_draw_list;
static D3DMATRIX crispy_view_matrix;
static D3DMATRIX crispy_proj_matrix;
// > [crispy]

namespace
{
	HWND Window;
	uint32_t palette[256];

	std::vector<std::unique_ptr<Win32HidDevice>> joysticks;

	int mousedx = 0;
	int mousedy = 0;
	int mousebuttons = 0;

	int SrcWidth = 0;
	int SrcHeight = 0;
	int ClientWidth = 0;
	int ClientHeight = 0;
	bool CurrentVSync = false;

	IDirect3D9* d3d9 = nullptr;
	IDirect3DDevice9* device = nullptr;
	IDirect3DSurface9* surface = nullptr;

	bool vid_vsync = true;

	LRESULT CALLBACK WindowProc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
	{
		if (message == WM_CLOSE)
		{
			PostQuitMessage(0);
			return 0;
		}
		else if (message == WM_ERASEBKGND)
		{
			return 0;
		}
#if 0
		else if (message == WM_PAINT)
		{
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(handle, &ps);
			EndPaint(handle, &ps);
			return 0;
		}
#endif
		else if (message == WM_SETFOCUS)
		{
			ShowCursor(FALSE);
			return 0;
		}
		else if (message == WM_KILLFOCUS)
		{
			ShowCursor(TRUE);
			return 0;
		}
		else if (message == WM_KEYDOWN)
		{
			int scancode = (lparam >> 16) & 0xff;
			if (lparam & 0x1000000) //handle extended flag
				scancode |= 0x80;
			KeyPressed(scancode);
			return 0;
		}
		else if (message == WM_KEYUP)
		{
			int scancode = (lparam >> 16) & 0xff;
			if (lparam & 0x1000000) //handle extended flag
				scancode |= 0x80;
			KeyReleased(scancode);
			return 0;
		}
		// < [crispy] Support ALTED for quick-save and quick-load
		else if (message == WM_SYSKEYDOWN)
		{
			int scancode = (lparam >> 16) & 0xff;
			if (lparam & 0x1000000) //handle extended flag
				scancode |= 0x80;
			if (scancode != KEY_LALT)
			{
				KeyPressed(KEY_LALT);
				KeyPressed(scancode);
			}
			return 0;
		}
		else if (message == WM_SYSKEYUP)
		{
			int scancode = (lparam >> 16) & 0xff;
			if (lparam & 0x1000000) //handle extended flag
				scancode |= 0x80;
			if (scancode != KEY_LALT)
			{
				KeyReleased(scancode);
				KeyReleased(KEY_LALT);
			}
			return 0;
		}
		// > [crispy]
		else if (message == WM_INPUT)
		{
			HRAWINPUT rawinputHandle = (HRAWINPUT)lparam;
			UINT size = 0;
			UINT result = GetRawInputData(rawinputHandle, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
			if (result == 0 && size > 0)
			{
				std::vector<uint32_t> buf((size + 3) / 4);
				result = GetRawInputData(rawinputHandle, RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER));
				if (result >= 0)
				{
					RAWINPUT* rawinput = (RAWINPUT*)buf.data();
					if (rawinput->header.dwType == RIM_TYPEMOUSE)
					{
						uint32_t buttons;
						mousedx += rawinput->data.mouse.lLastX;
						mousedy += rawinput->data.mouse.lLastY;

						//[ISB] Making buttons use rawinput
						buttons = rawinput->data.mouse.ulButtons;

						if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN && !mouse_button_state(MBUTTON_LEFT))
						{
							MousePressed(MBUTTON_LEFT);
							mousebuttons |= 1;
						}
						else if (buttons & RI_MOUSE_LEFT_BUTTON_UP && mouse_button_state(MBUTTON_LEFT))
						{
							MouseReleased(MBUTTON_LEFT);
							mousebuttons &= ~1;
						}
						if (buttons & RI_MOUSE_RIGHT_BUTTON_DOWN && !mouse_button_state(MBUTTON_RIGHT))
						{
							MousePressed(MBUTTON_RIGHT);
							mousebuttons |= 2;
						}
						else if (buttons & RI_MOUSE_RIGHT_BUTTON_UP && mouse_button_state(MBUTTON_RIGHT))
						{
							MouseReleased(MBUTTON_RIGHT);
							mousebuttons &= ~2;
						}
						if (buttons & RI_MOUSE_MIDDLE_BUTTON_DOWN && !mouse_button_state(MBUTTON_MIDDLE))
						{
							MousePressed(MBUTTON_MIDDLE);
							mousebuttons |= 4;
						}
						else if (buttons & RI_MOUSE_MIDDLE_BUTTON_UP && mouse_button_state(MBUTTON_MIDDLE))
						{
							MouseReleased(MBUTTON_MIDDLE);
							mousebuttons &= ~4;
						}
						if (buttons & RI_MOUSE_BUTTON_4_DOWN && !mouse_button_state(MBUTTON_4))
						{
							MousePressed(MBUTTON_4);
							mousebuttons |= 8;
						}
						else if (buttons & RI_MOUSE_BUTTON_4_UP && mouse_button_state(MBUTTON_4))
						{
							MouseReleased(MBUTTON_4);
							mousebuttons &= ~8;
						}
						if (buttons & RI_MOUSE_BUTTON_5_DOWN && !mouse_button_state(MBUTTON_5))
						{
							MousePressed(MBUTTON_5);
							mousebuttons |= 16;
						}
						else if (buttons & RI_MOUSE_BUTTON_5_UP && mouse_button_state(MBUTTON_5))
						{
							MouseReleased(MBUTTON_5);
							mousebuttons &= ~16;
						}

					}
					else if (rawinput->header.dwType == RIM_TYPEHID)
					{
						for (auto &joystick : joysticks)
						{
							joystick->update(rawinput);
						}

						if (joysticks.size() >= 1 && joysticks[0]->get_hat_count() > 0)
						{
							// Simulate CONTROL_THRUSTMASTER_FCS behavior

							int buttons = 0;
							int count = min(joysticks[0]->get_button_count(), 32);
							for (int i = 0; i < count; i++)
								if (joysticks[0]->get_keycode(i)) buttons |= 1 << i;

							int axes[4] = { 0, 0, 0, 0 };
							axes[0] = (int)(joysticks[0]->get_axis(Win32HidDevice::joystick_x) * 127.0f);
							axes[1] = (int)(joysticks[0]->get_axis(Win32HidDevice::joystick_y) * 127.0f);
							axes[2] = (int)(joysticks[0]->get_axis(Win32HidDevice::joystick_rz) * 127.0f);

							int hatdir = joysticks[0]->get_hat(0);
							if (hatdir != -1)
								axes[3] = 111 * hatdir / 360;
							else
								axes[3] = 127;

							JoystickInput(buttons, axes, JOY_ALL_AXIS);
						}
						else if (joysticks.size() > 1)
						{
							// Simulate two joysticks

							int buttons = 0;
							int axes[4] = { 0, 0, 0, 0 };

							int count = min(joysticks[0]->get_button_count(), 32);
							for (int i = 0; i < count; i++)
								if (joysticks[0]->get_keycode(i)) buttons |= 1 << i;

							axes[0] = (int)(joysticks[0]->get_axis(Win32HidDevice::joystick_x) * 127.0f);
							axes[1] = (int)(joysticks[0]->get_axis(Win32HidDevice::joystick_y) * 127.0f);
							axes[2] = (int)(joysticks[1]->get_axis(Win32HidDevice::joystick_x) * 127.0f);
							axes[3] = (int)(joysticks[1]->get_axis(Win32HidDevice::joystick_y) * 127.0f);

							JoystickInput(buttons, axes, JOY_ALL_AXIS);
						}
						else if (joysticks.size() == 1)
						{
							// Simulate one joystick

							int buttons = 0;
							int axes[4] = { 0, 0, 0, 0 };

							int count = min(joysticks[0]->get_button_count(), 32);
							for (int i = 0; i < count; i++)
								if (joysticks[0]->get_keycode(i)) buttons |= 1 << i;

							axes[0] = (int)(joysticks[0]->get_axis(Win32HidDevice::joystick_x) * 127.0f);
							axes[1] = (int)(joysticks[0]->get_axis(Win32HidDevice::joystick_y) * 127.0f);

							JoystickInput(buttons, axes, JOY_1_X_AXIS | JOY_1_Y_AXIS);
						}
					}
				}
			}
			return 0;
		}
		return DefWindowProc(handle, message, wparam, lparam);
	}

	uint8_t* PresentLock(int w, int h, int& pitch)
	{
		HRESULT result;

		RECT rect = {};
		GetClientRect(Window, &rect);
		if (rect.right != ClientWidth || rect.bottom != ClientHeight || CurrentVSync != vid_vsync)
		{
			if (surface)
			{
				surface->Release();
				surface = nullptr;
			}

			CurrentVSync = vid_vsync;
			ClientWidth = rect.right;
			ClientHeight = rect.bottom;

			D3DPRESENT_PARAMETERS pp = {};
			pp.Windowed = true;
			pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
			pp.BackBufferWidth = ClientWidth;
			pp.BackBufferHeight = ClientHeight;
			pp.BackBufferCount = 1;
			pp.hDeviceWindow = Window;
			pp.PresentationInterval = CurrentVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
			device->Reset(&pp);
		}

		if (SrcWidth != w || SrcHeight != h || !surface)
		{
			if (surface)
			{
				surface->Release();
				surface = nullptr;
			}

			SrcWidth = w;
			SrcHeight = h;
			result = device->CreateOffscreenPlainSurface(SrcWidth, SrcHeight, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &surface, 0);
			if (FAILED(result))
			{
				Error("IDirect3DDevice9.CreateOffscreenPlainSurface failed");
				return nullptr;
			}
		}

		D3DLOCKED_RECT lockrect = {};
		result = surface->LockRect(&lockrect, nullptr, D3DLOCK_DISCARD);
		if (FAILED(result))
		{
			pitch = 0;
			return nullptr;
		}

		pitch = lockrect.Pitch;
		return (uint8_t*)lockrect.pBits;
	}

	void PresentUnlock(int x, int y, int width, int height, bool linearfilter)
	{
		surface->UnlockRect();

		IDirect3DSurface9* backbuffer = nullptr;
		HRESULT result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
		if (FAILED(result))
			return;

		result = device->BeginScene();
		if (SUCCEEDED(result))
		{
			int count = 0;
			D3DRECT clearrects[4];
			if (y > 0)
			{
				clearrects[count].x1 = 0;
				clearrects[count].y1 = 0;
				clearrects[count].x2 = ClientWidth;
				clearrects[count].y2 = y;
				count++;
			}
			if (y + height < ClientHeight)
			{
				clearrects[count].x1 = 0;
				clearrects[count].y1 = y + height;
				clearrects[count].x2 = ClientWidth;
				clearrects[count].y2 = ClientHeight;
				count++;
			}
			if (x > 0)
			{
				clearrects[count].x1 = 0;
				clearrects[count].y1 = y;
				clearrects[count].x2 = x;
				clearrects[count].y2 = y + height;
				count++;
			}
			if (x + width < ClientWidth)
			{
				clearrects[count].x1 = x + width;
				clearrects[count].y1 = y;
				clearrects[count].x2 = ClientWidth;
				clearrects[count].y2 = y + height;
				count++;
			}
			if (count > 0)
				device->Clear(count, clearrects, D3DCLEAR_TARGET, 0, 0.0f, 0);

			RECT srcrect = {}, dstrect = {};
			srcrect.right = SrcWidth;
			srcrect.bottom = SrcHeight;
			dstrect.left = x;
			dstrect.top = y;
			dstrect.right = x + width;
			dstrect.bottom = y + height;
			device->StretchRect(surface, &srcrect, backbuffer, &dstrect, linearfilter ? D3DTEXF_LINEAR : D3DTEXF_POINT);

			// < [crispy] Draw draw commands
			// < TEMP CUBE
			crispy_vertex_t cube_verts[36] = {
				// Top
				{{-1, 1, 1}, 0xFFFFFFFF, {0, 0}},
				{{-1, -1, 1}, 0xFFFFFFFF, {0, 1}},
				{{1, -1, 1}, 0xFFFFFFFF, {1, 1}},
				{{-1, 1, 1}, 0xFFFFFFFF, {0, 0}},
				{{1, -1, 1}, 0xFFFFFFFF, {1, 1}},
				{{1, 1, 1}, 0xFFFFFFFF, {1, 0}},

				// Bottom
				{{-1, -1, -1}, 0xFFFFFFFF, {0, 0}},
				{{-1, 1, -1}, 0xFFFFFFFF, {0, 1}},
				{{1, 1, -1}, 0xFFFFFFFF, {1, 1}},
				{{-1, -1, -1}, 0xFFFFFFFF, {0, 0}},
				{{1, 1, -1}, 0xFFFFFFFF, {1, 1}},
				{{1, -1, -1}, 0xFFFFFFFF, {1, 0}},

				// Front
				{{-1, -1, 1}, 0xFFFFFFFF, {0, 0}},
				{{-1, -1, -1}, 0xFFFFFFFF, {0, 1}},
				{{1, -1, -1}, 0xFFFFFFFF, {1, 1}},
				{{-1, -1, 1}, 0xFFFFFFFF, {0, 0}},
				{{1, -1, -1}, 0xFFFFFFFF, {1, 1}},
				{{1, -1, 1}, 0xFFFFFFFF, {1, 0}},

				// Back
				{{1, 1, 1}, 0xFFFFFFFF, {0, 0}},
				{{1, 1, -1}, 0xFFFFFFFF, {0, 1}},
				{{-1, 1, -1}, 0xFFFFFFFF, {1, 1}},
				{{1, 1, 1}, 0xFFFFFFFF, {0, 0}},
				{{-1, 1, -1}, 0xFFFFFFFF, {1, 1}},
				{{-1, 1, 1}, 0xFFFFFFFF, {1, 0}},

				// Right
				{{1, -1, 1}, 0xFFFFFFFF, {0, 0}},
				{{1, -1, -1}, 0xFFFFFFFF, {0, 1}},
				{{1, 1, -1}, 0xFFFFFFFF, {1, 1}},
				{{1, -1, 1}, 0xFFFFFFFF, {0, 0}},
				{{1, 1, -1}, 0xFFFFFFFF, {1, 1}},
				{{1, 1, 1}, 0xFFFFFFFF, {1, 0}},

				// Left
				{{-1, 1, 1}, 0xFFFFFFFF, {0, 0}},
				{{-1, 1, -1}, 0xFFFFFFFF, {0, 1}},
				{{-1, -1, -1}, 0xFFFFFFFF, {1, 1}},
				{{-1, 1, 1}, 0xFFFFFFFF, {0, 0}},
				{{-1, -1, -1}, 0xFFFFFFFF, {1, 1}},
				{{-1, -1, 1}, 0xFFFFFFFF, {1, 0}}
			};
			//plat_set_3d_view({-3.0f, -3.0f, 3.0f}, {1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, 90.0f * 3.1415f / 180.0f);
			plat_set_3d_view({0, 0, 0}, {0, 1, 0}, {0, 0, 1}, 90.0f * 3.1415f / 180.0f);
			plat_draw_3d(0, cube_verts, 36);
			// > TEMP CUBE

			device->SetTransform(D3DTS_VIEW, &crispy_view_matrix);
			device->SetTransform(D3DTS_PROJECTION, &crispy_proj_matrix);
			device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
			device->SetRenderState(D3DRENDERSTATETYPE::D3DRS_ZENABLE, 0);
			device->SetRenderState(D3DRENDERSTATETYPE::D3DRS_CULLMODE, D3DCULL::D3DCULL_NONE);
			for (const auto& draw_cmd : crispy_draw_list)
			{
				device->SetTexture(0, draw_cmd.texture);
				device->DrawPrimitiveUP(D3DPRIMITIVETYPE::D3DPT_TRIANGLELIST, count / 3, crispy_vertices.data() + draw_cmd.vertices_idx, sizeof(crispy_vertex_t));
			}
			crispy_draw_list.clear();
			crispy_vertices.clear();
			// > [crispy]

			result = device->EndScene();
			if (SUCCEEDED(result))
				device->Present(nullptr, nullptr, 0, nullptr);
		}

		backbuffer->Release();
	}
}

int WindowWidth = 1600;
int WindowHeight = 900;
int BestFit = 0;
int Fullscreen = 0;
int SwapInterval = 0;

int plat_init()
{
	WNDCLASSEX windowClassDesc;
	memset(&windowClassDesc, 0, sizeof(WNDCLASSEX));
	windowClassDesc.cbSize = sizeof(WNDCLASSEX);
	windowClassDesc.lpszClassName = TEXT("DescentWindow");
	windowClassDesc.hInstance = GetModuleHandle(nullptr);
	windowClassDesc.lpfnWndProc = WindowProc;
	windowClassDesc.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClassDesc.style |= CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
	RegisterClassEx(&windowClassDesc);

	d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d9)
	{
		Error("Direct3DCreate9 failed\n");
		return 1;
	}

	plat_read_chocolate_cfg();
	return 0;
}

int plat_create_window()
{
	if (!Window)
	{
		HDC screendc = GetDC(0);
		int screenwidth = GetDeviceCaps(screendc, HORZRES);
		int screenheight = GetDeviceCaps(screendc, VERTRES);
		ReleaseDC(0, screendc);
		
		Window = CreateWindowEx(WS_EX_APPWINDOW, TEXT("DescentWindow"), TEXT("Crispy Descent"), WS_POPUP | WS_VISIBLE, 0, 0, screenwidth, screenheight, 0, 0, GetModuleHandle(nullptr), 0);
		if (Window == 0)
			return 1;

		joysticks = Win32HidDevice::create_devices(Window);

		RECT rect = {};
		GetClientRect(Window, &rect);

		CurrentVSync = vid_vsync;
		ClientWidth = rect.right;
		ClientHeight = rect.bottom;

		D3DPRESENT_PARAMETERS pp = {};
		pp.Windowed = true;
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.BackBufferWidth = ClientWidth;
		pp.BackBufferHeight = ClientHeight;
		pp.BackBufferCount = 1;
		pp.hDeviceWindow = Window;
		pp.PresentationInterval = CurrentVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		HRESULT result = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
		if (FAILED(result))
		{
			joysticks.clear();
			DestroyWindow(Window);
			Window = 0;
			Error("IDirect3D9.CreateDevice failed\n");
			return 1;
		}
	}
	return 0;
}

void plat_close_window()
{
	joysticks.clear();
	if (surface) surface->Release();
	if (device) device->Release();
	if (Window) DestroyWindow(Window);
	surface = nullptr;
	device = nullptr;
	Window = 0;
}

int plat_check_gr_mode(int mode)
{
	//For now, high color modes are rejected (were those ever well supported? or even used?)
	switch (mode)
	{
	case SM_320x200C:
	case SM_320x200U:
	case SM_320x240U:
	case SM_360x200U:
	case SM_360x240U:
	case SM_376x282U:
	case SM_320x400U:
	case SM_320x480U:
	case SM_360x400U:
	case SM_360x480U:
	case SM_360x360U:
	case SM_376x308U:
	case SM_376x564U:
	case SM_640x400V:
	case SM_640x480V:
	case SM_800x600V:
	case SM_1024x768V:
	case 19: 
	case 21:
	case SM_1280x1024V: return 0;
	}
	return 11;
}

int plat_set_gr_mode(int mode)
{
	int w = 0, h = 0;
	switch (mode)
	{
	case SM_320x200C:
	case SM_320x200U:
		w = 320; h = 200;
		break;
	case SM_320x240U:
		w = 320; h = 240;
		break;
	case SM_360x200U:
		w = 360; h = 200;
		break;
	case SM_360x240U:
		w = 360; h = 240;
		break;
	case SM_376x282U:
		w = 376; h = 282;
		break;
	case SM_320x400U:
		w = 320; h = 400;
		break;
	case SM_320x480U:
		w = 320; h = 480;
		break;
	case SM_360x400U:
		w = 360; h = 400;
		break;
	case SM_360x480U:
		w = 360; h = 480;
		break;
	case SM_376x308U:
		w = 376; h = 308;
		break;
	case SM_376x564U:
		w = 376; h = 564;
		break;
	case SM_640x400V:
		w = 640; h = 400;
		break;
	case SM_640x480V:
		w = 640; h = 480;
		break;
	case SM_800x600V:
		w = 800; h = 600;
		break;
	case SM_1024x768V:
		w = 1024; h = 768;
		break;
	case 19:
		w = 320; h = 100;
		break;
	case 21:
		w = 160; h = 100;
		break;
	case SM_1280x1024V:
		w = 1280; h = 1024;
		break;
	default:
		Error("plat_set_gr_mode: bad mode %d\n", mode);
		return 0;
	}
	WindowWidth = w;
	WindowHeight = h;

	return 0;
}

void plat_do_events()
{
	while (true)
	{
		MSG msg;
		BOOL result = PeekMessage(&msg, 0, 0, 0, PM_REMOVE);
		if (result == 0)
			break;
		else if (msg.message == WM_QUIT)
			exit(0); // Is there a nicer way?

		//TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void plat_set_mouse_relative_mode(int state)
{
}

void plat_write_palette(int start, int end, uint8_t* data)
{
	for (int i = start; i <= end; i++)
	{
		palette[i] = MAKEARGB(255, data[i * 3 + 0] << 2, data[i * 3 + 1] << 2, data[i * 3 + 2] << 2);
	}
}

void plat_blank_palette()
{
	uint8_t pal[768];
	memset(&pal[0], 0, 768 * sizeof(uint8_t));
	plat_write_palette(0, 255, &pal[0]);
}

void plat_read_palette(uint8_t* dest)
{
	for (int i = 0; i < 256; i++)
	{
		dest[i * 3 + 0] = RPART(palette[i]);
		dest[i * 3 + 1] = GPART(palette[i]);
		dest[i * 3 + 2] = BPART(palette[i]);
	}
}

void plat_wait_for_vbl()
{
	I_MarkEnd(US_60FPS);
	I_MarkStart();
}

struct LetterboxRect { int left, top, width, height; };

static LetterboxRect FindLetterbox()
{
	RECT box = { 0 };
	GetClientRect(Window, &box);

	// Back buffer letterbox for the final output
	int clientWidth = box.right - box.left;
	int clientHeight = box.bottom - box.top;
	if (clientWidth == 0 || clientHeight == 0)
	{
		// When window is minimized there may not be any client area.
		// Pretend to the rest of the render code that we just have a very small window.
		clientWidth = 320;
		clientHeight = 200;
	}
	int screenWidth = grd_curscreen->sc_canvas.cv_bitmap.bm_w;
	int screenHeight = grd_curscreen->sc_canvas.cv_bitmap.bm_h;

	if (screenHeight == 400) // Minimap support
		screenHeight = 200;

	float scaleX, scaleY;
	if (screenHeight == 200) // Pixel strech 320x200
	{
		scaleX = min(clientWidth / (float)screenWidth, clientHeight / (screenHeight * 1.2f));
		scaleY = scaleX * 1.2f;
	}
	else
	{
		scaleX = min(clientWidth / (float)screenWidth, clientHeight / (float)screenHeight);
		scaleY = scaleX;
	}

	LetterboxRect result;
	result.width = (int)round(screenWidth * scaleX);
	result.height = (int)round(screenHeight * scaleY);
	result.left = (clientWidth - result.width) / 2;
	result.top = (clientHeight - result.height) / 2;
	return result;
}

void plat_present_canvas(int sync)
{
	vid_vsync = sync;

	int width = grd_curscreen->sc_canvas.cv_bitmap.bm_w;
	int height = grd_curscreen->sc_canvas.cv_bitmap.bm_h;
	int pitch = 0;

	uint8_t* pixels = PresentLock(width, height, pitch);
	if (pixels)
	{
		for (int y = 0; y < height; y++)
		{
			uint32_t* dest = (uint32_t*)(pixels + pitch * (ptrdiff_t)y);
			const uint8_t* src = grd_curscreen->sc_canvas.cv_bitmap.bm_data + width * (ptrdiff_t)y;
			for (int x = 0; x < width; x++)
			{
				dest[x] = palette[src[x]];
			}
		}

		auto box = FindLetterbox();
		PresentUnlock(box.left, box.top, box.width, box.height, false);
	}
}

extern unsigned char* gr_video_memory;
void plat_blit_canvas(grs_canvas* canv)
{
	if (canv->cv_bitmap.bm_type == BM_SVGA)
		memcpy(gr_video_memory, canv->cv_bitmap.bm_data, canv->cv_bitmap.bm_w * canv->cv_bitmap.bm_h);
}

void plat_close()
{
	if (d3d9) d3d9->Release();
	d3d9 = nullptr;
}

void plat_display_error(const char* msg)
{
	MessageBoxA(Window, msg, "Game Error", 0);
}

// Mouse API:

void mouse_get_pos(int* x, int* y)
{
	POINT pos = { 0 };
	GetCursorPos(&pos);
	*x = pos.x;
	*y = pos.y;
}

void mouse_get_delta(int* dx, int* dy)
{
	*dx = mousedx;
	*dy = mousedy;
	mousedx = 0;
	mousedy = 0;
}

int mouse_get_btns()
{
	return mousebuttons;
}

void mouse_set_pos(int x, int y)
{
	SetCursorPos(x, y);
}

// < [crispy] 3D rendering

static uintptr_t temp_texture = 0;

uintptr_t plat_create_texture(uint8_t* data, int width, int height)
{
	if (!device) return 0;
	if (!data || width <= 0 || height <= 0) return 0;

	IDirect3DTexture9* texture = nullptr;
	device->CreateTexture((UINT)width, (UINT)height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, 0);
	if (!texture)
	{
		printf("Failed to create texture of size %ix%i\n", width, height);
		return 0;
	}

	D3DLOCKED_RECT rect;
	texture->LockRect( 0, &rect, 0, D3DLOCK_DISCARD );
	uint8_t* dest = (uint8_t*)rect.pBits;
	memcpy(dest, data, sizeof(uint8_t) * width * height * 4);
	texture->UnlockRect(0);

	return reinterpret_cast<uintptr_t>(texture);
}

static void normalize_vec3(vec3f_t& v)
{
    auto len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0f)
    {
        len = 1.0f / len;
    }
    v.x *= len;
    v.y *= len;
    v.z *= len;
}

static vec3f_t cross(const vec3f_t& a, const vec3f_t& b)
{
	vec3f_t result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
	return result;
}

static float dot(const vec3f_t& a, const vec3f_t& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

void plat_set_3d_view(vec3f_t eye, vec3f_t forward, vec3f_t up, float fov_rad, float near_plane, float far_plane)
{
	// View
	{
		auto R2 = forward;
		normalize_vec3(R2);

		auto R0 = cross(up, R2);
		normalize_vec3(R0);

		auto R1 = cross(R2, R0);

		vec3f_t NegEyePosition = {-eye.x, -eye.y, -eye.z};

		auto D0 = dot(R0, NegEyePosition);
		auto D1 = dot(R1, NegEyePosition);
		auto D2 = dot(R2, NegEyePosition);

		crispy_view_matrix = {
			R0.x, R1.x, R2.x, 0,
			R0.y, R1.y, R2.y, 0,
			R0.z, R1.z, R2.z, 0,
			D0, D1, D2, 1};
	}

	// Proj
	{
        float CosFov = std::cos(0.5f * fov_rad);
        float SinFov = std::sin(0.5f * fov_rad);

		float ratio = (float)ClientWidth / (float)ClientHeight;
        float Height = CosFov / SinFov;
        float Width = Height / ratio;
        float fRange = far_plane / (near_plane - far_plane);

        crispy_proj_matrix.m[0][0] = Width;
        crispy_proj_matrix.m[0][1] = 0.0f;
        crispy_proj_matrix.m[0][2] = 0.0f;
        crispy_proj_matrix.m[0][3] = 0.0f;

        crispy_proj_matrix.m[1][0] = 0.0f;
        crispy_proj_matrix.m[1][1] = Height;
        crispy_proj_matrix.m[1][2] = 0.0f;
        crispy_proj_matrix.m[1][3] = 0.0f;

        crispy_proj_matrix.m[2][0] = 0.0f;
        crispy_proj_matrix.m[2][1] = 0.0f;
        crispy_proj_matrix.m[2][2] = fRange;
        crispy_proj_matrix.m[2][3] = -1.0f;

        crispy_proj_matrix.m[3][0] = 0.0f;
        crispy_proj_matrix.m[3][1] = 0.0f;
        crispy_proj_matrix.m[3][2] = fRange * near_plane;
        crispy_proj_matrix.m[3][3] = 0.0f;
	}
}

void plat_draw_3d(uintptr_t texture_id, crispy_vertex_t* vertices, int count)
{
	if (!temp_texture)
	{
		uint32_t checker[4] = {
			0xFFFFFFFF, 0xFF888888,
			0xFF888888, 0xFFFFFFFF
		};
		temp_texture = plat_create_texture((uint8_t*)&checker, 2, 2);
	}
	texture_id = temp_texture;

	if (!device) return;
	if (!texture_id) return;
	if (count <= 0) return;
	if (!vertices) return;

	IDirect3DTexture9* texture = reinterpret_cast<IDirect3DTexture9*>(texture_id);
	
	crispy_draw_list.push_back({});
	auto& draw_cmd = crispy_draw_list.back();
	draw_cmd.texture = texture;
	draw_cmd.count = count;
	draw_cmd.vertices_idx = (int)crispy_vertices.size();
	crispy_vertices.insert(crispy_vertices.end(), vertices, vertices + count);
}

// > [crispy] 

#endif
