/*
 * Copyright (c) 2010-2016 Stephane Poirier
 *
 * stephane.poirier@oifii.org
 *
 * Stephane Poirier
 * 3532 rue Ste-Famille, #3
 * Montreal, QC, H2X 2L1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// spicapturewebcam.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

/*
int _tmain(int argc, _TCHAR* argv[])
{
	return 0;
}
*/

#include <windows.h>
#include <dshow.h>
//spi, begin
#include <mtype.h>
#include <string>
using namespace std;
#include "direct.h" //for _mkdir()
//spi, end
#pragma comment(lib,"Strmiids.lib")

#define DsHook(a,b,c) if (!c##_) { INT_PTR* p=b+*(INT_PTR**)a;   VirtualProtect(&c##_,4,PAGE_EXECUTE_READWRITE,&no);\
                                          *(INT_PTR*)&c##_=*p;   VirtualProtect(p,    4,PAGE_EXECUTE_READWRITE,&no);   *p=(INT_PTR)c; }

//spi, begin
bool global_bsaveimage=true;
int global_imageframe_id=0;
//int global_imageframe_max=3; //user defined, the smaller the shorter response time
int global_imageframe_max=10; //user defined, the smaller the shorter response time
CHAR global_imageframe_path[] = {"c:\\temp\\spicapturewebcam\\"}; //must terminate by 
CHAR global_TempBuffer[1024];

BITMAPINFOHEADER global_bmpInfo; // current bitmap header info
int global_stride=-1;
//int global_bpp=-1;
//spi, end
//spi 2014, begin
IMediaEventEx *event;
//spi 2014, end
// Here you get image video data in buf / len. Process it before calling Receive_ because renderer dealocates it.
HRESULT ( __stdcall * Receive_ ) ( void* inst, IMediaSample *smp ) ; 
HRESULT   __stdcall   Receive    ( void* inst, IMediaSample *smp ) 
{     
    BYTE*     buf;    smp->GetPointer(&buf); DWORD len = smp->GetActualDataLength();
	//spi, begin
	//need image width and height
    AM_MEDIA_TYPE* type;
    HRESULT hr = smp->GetMediaType(&type);
    if ( hr != S_OK )
    { 
		//TODO: error 
	} 
    else
    {
        if ( type->formattype == FORMAT_VideoInfo )
        {
            const VIDEOINFOHEADER * vi = reinterpret_cast<VIDEOINFOHEADER*>( type->pbFormat );
            const BITMAPINFOHEADER & bmiHeader = vi->bmiHeader;
            //! now the bmiHeader.biWidth contains the data stride
            //global_stride = bmiHeader.biWidth;
			//global_bpp = bmiHeader.biBitCount; //spi
            global_bmpInfo = bmiHeader;
            int width = ( vi->rcTarget.right - vi->rcTarget.left );
            //! replace the data stride by the actual width
            if ( width != 0 )
			{
                global_bmpInfo.biWidth = width;
			}
            global_stride = global_bmpInfo.biWidth;
			//global_bpp = global_bmpInfo.biBitCount;

        }
        else
        { // unsupported format 
		}
    }
    DeleteMediaType( type );

	if(global_stride>0 && global_bsaveimage)
	{
		global_imageframe_id++;
		if(global_imageframe_id==global_imageframe_max) global_imageframe_id=1;
		_mkdir(global_imageframe_path);
		std::string mystring;
		mystring = global_imageframe_path;
		sprintf(global_TempBuffer, "frame%03d.bmp", global_imageframe_id);
		mystring += global_TempBuffer;
		strcpy(global_TempBuffer, mystring.c_str());

		BITMAPFILEHEADER bmfh;
		int nBitsOffset = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); 
		LONG lImageSize = global_bmpInfo.biSizeImage;
		LONG lFileSize = nBitsOffset + lImageSize;
    
		bmfh.bfType = 'B'+('M' << 8);
		bmfh.bfOffBits = nBitsOffset;
		bmfh.bfSize = lFileSize;
		bmfh.bfReserved1 = 0;  
		bmfh.bfReserved2 = 0;  

		FILE *pFile = fopen(global_TempBuffer, "wb");
		fwrite(&bmfh, sizeof(BITMAPFILEHEADER), 1, pFile);
		fwrite(&global_bmpInfo, sizeof(BITMAPINFOHEADER), 1, pFile);  
		fwrite(buf, lImageSize, 1, pFile);

		fclose(pFile);

	}
	//spi, end
    HRESULT   ret  =  Receive_   ( inst, smp );   
    return    ret; 
}

//spi 2014, begin
LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_APP + 1) {
        long evt;
        LONG_PTR param1, param2;

        while (SUCCEEDED(event->GetEvent(&evt, &param1, &param2, 0))) {
            event->FreeEventParams(evt, param1, param2);

            if (evt == EC_USERABORT) {
                PostQuitMessage(0);
            }
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
//spi 2014, end

int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show)
{
    HRESULT hr = CoInitialize(0); MSG msg={0}; DWORD no;

    IGraphBuilder*  graph= 0;  hr = CoCreateInstance( CLSID_FilterGraph, 0, CLSCTX_INPROC,IID_IGraphBuilder, (void **)&graph );
    IMediaControl*  ctrl = 0;  hr = graph->QueryInterface( IID_IMediaControl, (void **)&ctrl );

    ICreateDevEnum* devs = 0;  hr = CoCreateInstance (CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC, IID_ICreateDevEnum, (void **) &devs);
    IEnumMoniker*   cams = 0;  hr = devs?devs->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &cams, 0):0;  
    IMoniker*       mon  = 0;  hr = cams->Next (1,&mon,0);  // get first found capture device (webcam?)    
    IBaseFilter*    cam  = 0;  hr = mon->BindToObject(0,0,IID_IBaseFilter, (void**)&cam);
                               hr = graph->AddFilter(cam, L"Capture Source"); // add web cam to graph as source
    IEnumPins*      pins = 0;  hr = cam?cam->EnumPins(&pins):0;   // we need output pin to autogenerate rest of the graph
    IPin*           pin  = 0;  hr = pins?pins->Next(1,&pin, 0):0; // via graph->Render
                               hr = graph->Render(pin); // graph builder now builds whole filter chain including MJPG decompression on some webcams
    IEnumFilters*   fil  = 0;  hr = graph->EnumFilters(&fil); // from all newly added filters
    IBaseFilter*    rnd  = 0;  hr = fil->Next(1,&rnd,0); // we find last one (renderer)
                               hr = rnd->EnumPins(&pins);  // because data we are intersted in are pumped to renderers input pin 
                               hr = pins->Next(1,&pin, 0); // via Receive member of IMemInputPin interface
    IMemInputPin*   mem  = 0;  hr = pin->QueryInterface(IID_IMemInputPin,(void**)&mem);

	//spi 2014, begin
	WNDCLASSEX wx = {};
	wx.cbSize = sizeof(WNDCLASSEX);
	wx.lpfnWndProc = proc;        // function which will handle messages
	wx.hInstance = GetModuleHandle(0);
	wx.lpszClassName = L"spicapturewebcamclass";
	RegisterClassEx(&wx);
	HWND hwnd = CreateWindowEx(0, L"spicapturewebcamclass", L"spicapturewebcam", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

	graph->QueryInterface(IID_IMediaEventEx, (void **) &event);
	event->SetNotifyWindow((OAHWND) hwnd, WM_APP + 1, 0);
	//spi 2014, end
    DsHook(mem,6,Receive); // so we redirect it to our own proc to grab image data

    hr = ctrl->Run();   

    while ( GetMessage(   &msg, 0, 0, 0 ) ) {  
        TranslateMessage( &msg );   
        DispatchMessage(  &msg ); 
    }
};