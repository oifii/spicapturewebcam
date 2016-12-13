#include <windows.h>
#include <dshow.h>
#include <streams.h>

int w,h; HWND hwnd; MSG msg = {0}; BITMAPINFOHEADER bmih={0}; 

struct __declspec(  uuid("{71771540-2017-11cf-ae26-0020afd79767}")  ) CLSID_Sampler;

struct Sampler : public CBaseVideoRenderer 
{
    Sampler( IUnknown* unk, HRESULT *hr ) : CBaseVideoRenderer(__uuidof(CLSID_Sampler), NAME("Frame Sampler"), unk, hr) {};
    HRESULT CheckMediaType(const CMediaType *media ) 
	{    
        VIDEOINFO* vi; if(!IsEqualGUID( *media->Subtype(), MEDIASUBTYPE_RGB24) || !(vi=(VIDEOINFO *)media->Format()) ) return E_FAIL;
        bmih=vi->bmiHeader;	SetWindowPos(hwnd,0,0,0,20+(w=vi->bmiHeader.biWidth),60+(h=vi->bmiHeader.biHeight),SWP_NOZORDER|SWP_NOMOVE);
        return  S_OK;
    }
    HRESULT DoRenderSample(IMediaSample *sample){
        BYTE* data; sample->GetPointer( &data ); 
        // Process RGB Frame data* here. For Example: ZeroMemory(data+w*h,w*h);
        BITMAPINFO bmi={0}; bmi.bmiHeader=bmih; RECT r; GetClientRect( hwnd, &r );
        HDC dc=GetDC(hwnd);
        StretchDIBits(dc,0,16,r.right,r.bottom-16,0,0,w,h,data,&bmi,DIB_RGB_COLORS,SRCCOPY);
        ReleaseDC(dc);
        return  S_OK;
    }
    HRESULT ShouldDrawSampleNow(IMediaSample *sample, REFERENCE_TIME *start, REFERENCE_TIME *stop) {
        return S_OK; // disable droping of frames
    }
};

int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show){
    HRESULT hr = CoInitialize(0);
    hwnd=CreateWindowEx(0,"LISTBOX",0,WS_SIZEBOX,0,0,0,0,0,0,0,0); 
           
    IGraphBuilder*  graph= 0; hr = CoCreateInstance( CLSID_FilterGraph, 0, CLSCTX_INPROC,IID_IGraphBuilder, (void **)&graph );
    IMediaControl*  ctrl = 0; hr = graph->QueryInterface( IID_IMediaControl, (void **)&ctrl );

    Sampler*        sampler      = new Sampler(0,&hr); 
    IPin*           rnd  = 0; hr = sampler->FindPin(L"In", &rnd);
                              hr = graph->AddFilter((IBaseFilter*)sampler, L"Sampler");

    ICreateDevEnum* devs = 0; hr = CoCreateInstance (CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC, IID_ICreateDevEnum, (void **) &devs);
    IEnumMoniker*   cams = 0; hr = devs?devs->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &cams, 0):0;  
    IMoniker*       mon  = 0; hr = cams?cams->Next (1, &mon, 0):0;
    IBaseFilter*    cam  = 0; hr = mon?mon->BindToObject(0,0,IID_IBaseFilter, (void**)&cam):0;
    IEnumPins*      pins = 0; hr = cam?cam->EnumPins(&pins):0; 
    IPin*           cap  = 0; hr = pins?pins->Next(1,&cap, 0):0;
                              hr = graph->AddFilter(cam, L"Capture Source"); 

    IBaseFilter*    vid  = 0; hr = graph->AddSourceFilter (L"c:\\Windows\\clock.avi", L"File Source", &vid);
    IPin*           avi  = 0; hr = vid?vid->FindPin(L"Output", &avi):0;

    hr = graph->Connect(cap?cap:avi,rnd);
    hr = graph->Render( cap?cap:avi );
    hr = ctrl->Run();
    SendMessage(hwnd, LB_ADDSTRING, 0, (long)(cap?"Capture source ...":"File source ..."));
    ShowWindow(hwnd,SW_SHOW);

    while ( msg.message != WM_QUIT) {  
        if( PeekMessage(      &msg, 0, 0, 0, PM_REMOVE ) ) {
            TranslateMessage( &msg );   
            DispatchMessage(  &msg ); 
            if(msg.message == WM_KEYDOWN && msg.wParam==VK_ESCAPE ) break;
        }   Sleep(30);
    }
};