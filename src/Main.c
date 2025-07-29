#include "C:/Wichtig/System/Static/Library/Math.h"
#include "C:/Wichtig/System/Static/Library/Random.h"
#include "C:/Wichtig/System/Static/Container/PVector.h"
#include "C:/Wichtig/System/Static/Library/WindowEngine1.0.h"
#include "C:/Wichtig/System/Static/Library/Cmd.h"

#define VHANDLE_NONE            0
#define VHANDLE_TEXTBUFFER      1
#define VHANDLE_INPUTBUFFER     2
#define VHANDLE_PROCESS         3
#define VHANDLE_THREAD          4

typedef struct VHandleHeader {
    int Type;
    int Mode;
    size_t Size;
} VHandleHeader;

typedef void* VHandle;

VHandle VHandle_New(int Type,int Mode,size_t Size){
    VHandleHeader hh;
    hh.Type = Type;
    hh.Mode = Mode;
    hh.Size = Size;

    VHandle h = malloc(sizeof(VHandleHeader)+Size);
    memcpy(h,&hh,sizeof(VHandleHeader));
    return h;
}

void VHandle_Free(VHandle h){
    free(h);
}

typedef struct VProcess {
    char* Title;
    BOOL Running;
    VHandle hOut;
    VHandle hIn;
    HANDLE hThread;
    unsigned long (*Main)(void*);
} VProcess;

VProcess VProcess_New(char* Name,unsigned long (*Main)(void*)){
    VProcess p;
    p.Title = CStr_Cpy(Name);
    p.Running = FALSE;
    p.hOut = NULL;
    p.hIn = NULL;
    p.hThread = NULL;
    p.Main = Main;
    return p;
}

void VProcess_Free(VProcess* p){
    if(p->Title) free(p->Title);
    if(p->hOut) VHandle_Free(p->hOut);
    if(p->hIn) VHandle_Free(p->hIn);
    TerminateThread(p->hThread,0);
    CloseHandle(p->hThread);
}

#define VWINDOW_NONE        0
#define VWINDOW_GRAPPED     1
#define VWINDOW_TOP         2
#define VWINDOW_BOTTOM      3
#define VWINDOW_LEFT        4
#define VWINDOW_RIGHT       5

typedef struct VWindow {
    char* Title;
    Vec2 p;
    Pixel* SwapBuffer;
    Sprite Context;
    HANDLE hMutex;

    Vec2 Dimension;

    Vec2 pBefore;
    Vec2 dBefore;
    unsigned short LastKey;
    unsigned short LastChar;
    States Strokes[256];
    unsigned char Icon;
    long long TimeLifted;

    struct {
        char Min    : 1;
        char Max    : 1;
        char Focus  : 1;
    };
} VWindow;

VWindow VNew(char* Title,Vec2 p,Vec2 d){
    VWindow w;
    w.Title = CStr_Cpy(Title);
    w.p = p;
    w.Context = Sprite_Init(d.x,d.y);
    w.SwapBuffer = (Pixel*)calloc((int)(d.x * d.y),sizeof(Pixel));
    w.hMutex = CreateMutex(NULL,FALSE,NULL);
    
    w.Dimension = d;
    w.pBefore = (Vec2){ 0.0f,0.0f };
    w.dBefore = (Vec2){ 0.0f,0.0f };
    w.TimeLifted = 0LL;
    w.Min = FALSE;
    w.Max = FALSE;
    w.Focus = TRUE;
    w.Icon = 5;
    return w;
}

void VResize(VWindow* w){
    if(w->Context.w!=w->Dimension.x || w->Context.h!=w->Dimension.y){
        WaitForSingleObject(w->hMutex,INFINITE);
        w->Context.w = w->Dimension.x;
        w->Context.h = w->Dimension.y;
        if(w->Context.img) free(w->Context.img);
        if(w->SwapBuffer) free(w->SwapBuffer);
        w->Context.img = (Pixel*)calloc((int)(w->Context.w * w->Context.h),sizeof(Pixel));
        w->SwapBuffer  = (Pixel*)calloc((int)(w->Context.w * w->Context.h),sizeof(Pixel));
        ReleaseMutex(w->hMutex);
    }
}

void VSetTitle(VWindow* w,char* Title){
    if(w->Title) free(w->Title);
    w->Title = CStr_Cpy(Title);
}

void VSwap(VWindow* w){
    memcpy(w->Context.img,w->SwapBuffer,sizeof(Pixel) * w->Context.w * w->Context.h);
}

void VRender(VWindow* w,Font* f,Font* I){
    if(!w->Min){
        WaitForSingleObject(w->hMutex,INFINITE);
        RenderRect(w->p.x,w->p.y,w->Dimension.x,0.02f * GetHeight(),LIGHT_GRAY);
        RenderCircle(((Vec2){w->p.x + w->Dimension.x - 0.05f * GetHeight(),w->p.y + 0.01f * GetHeight()}),0.01f * GetHeight(),GREEN);
        RenderCircle(((Vec2){w->p.x + w->Dimension.x - 0.03f * GetHeight(),w->p.y + 0.01f * GetHeight()}),0.01f * GetHeight(),YELLOW);
        RenderCircle(((Vec2){w->p.x + w->Dimension.x - 0.01f * GetHeight(),w->p.y + 0.01f * GetHeight()}),0.01f * GetHeight(),RED);
        
        if(w->Context.w!=w->Dimension.x || w->Context.h!=w->Dimension.y){
            RenderRect(w->p.x,w->p.y + 0.02f * GetHeight(),w->Dimension.x,w->Dimension.y,BLACK);
        }else{
            RenderSprite(&w->Context,w->p.x,w->p.y + 0.02f * GetHeight());
        }
        
        ReleaseMutex(w->hMutex);

        RenderCStrFont(f,w->Title,w->p.x+I->CharSizeX,w->p.y,BLACK);
        RenderSubSpriteAlpha(&I->Atlas,w->p.x,w->p.y,(w->Icon % I->Columns) * I->CharSizeX,(w->Icon / I->Columns) * I->CharSizeY,I->CharSizeX,I->CharSizeY);
    }
}

void VUDKB(VWindow* w){
    char Buffer[256];
    GetKeyboardState(Buffer);
    wchar_t Unicode[2];
    
    if(w->Focus){
        w->LastKey = 0;
        w->LastChar = 0;
        for (int i = 0; i < MAX_STROKES; i++)
	    {
	    	w->Strokes[i].PRESSED = FALSE;
	    	w->Strokes[i].RELEASED = FALSE;
	    	if (GetAsyncKeyState(i) & 0x8000)
	    	{
                w->LastKey = i;
                ToAscii(i,MapVirtualKey(i,MAPVK_VK_TO_VSC),Buffer,&w->LastChar,0);
	    		w->Strokes[i].PRESSED = !w->Strokes[i].DOWN;
	    		w->Strokes[i].DOWN = TRUE;
	    	}
	    	else
	    	{
	    		w->Strokes[i].RELEASED = w->Strokes[i].DOWN;
	    		w->Strokes[i].DOWN = FALSE;
	    	}
	    }
    }
}

void VFree(VWindow* w){
    if(w->Title)    free(w->Title);
    Sprite_Free(&w->Context);
    CloseHandle(w->hMutex);
    free(w->SwapBuffer);
}

Vec2 VMouse(VWindow* w){
    return (Vec2){GetMouse().x - w->p.x,GetMouse().y - w->p.y - GetHeight()*0.02f};
}

States VStroke(VWindow* w,int Stroke){
    if(w->Focus)    return w->Strokes[Stroke];
    else            return (States){ 0 };
}

void VHandleInput(PVector* VProcesses,PVector* VWindows,VWindow** Focused,char* Picked,Vec2* Offset,States Input){
    if(Input.PRESSED){
        Vec2 Mouse = (Vec2){GetMouse().x,GetMouse().y};
        for(int i = VWindows->size-1;i>=0;i--){
            VWindow* w = (VWindow*)PVector_Get(VWindows,i);
            if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){0.0f,-0.01f * GetHeight()}),(Vec2){w->Context.w,w->Context.h + 0.03f * GetHeight()}},Mouse)){
                if(w!=(*Focused)){
                    if((*Focused)) (*Focused)->Focus = FALSE;
                    *Picked = VWINDOW_NONE;
                    *Offset = (Vec2){0.0f,0.0f};
                    (*Focused) = w;
                    (*Focused)->Focus = TRUE;
                }
                (*Focused)->TimeLifted = Time_Nano();

                if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){w->Context.w - 0.06f * GetHeight(),0.0f}),(Vec2){0.02f * GetHeight(),0.02f * GetHeight()}},Mouse)){
                    (*Focused)->Min = TRUE;
                }else if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){w->Context.w - 0.04f * GetHeight(),0.0f}),(Vec2){0.02f * GetHeight(),0.02f * GetHeight()}},Mouse)){
                    (*Focused)->Max = !(*Focused)->Max;
                    if(!(*Focused)->Max){
                        w->p = w->pBefore;
                        w->Dimension = w->dBefore;
                        VResize(w);
                        w->pBefore = (Vec2){0.0f,0.0f};
                        w->dBefore = (Vec2){0.0f,0.0f};
                    }else{
                        w->pBefore = w->p;
                        w->dBefore = (Vec2){w->Context.w,w->Context.h};
                        w->p = (Vec2){0.0f,0.0f};
                        w->Dimension = (Vec2){GetWidth(),GetHeight()-0.02f * GetHeight()};
                        VResize(w);
                    }
                }else if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){w->Context.w - 0.02f * GetHeight(),0.0f}),(Vec2){0.02f * GetHeight(),0.02f * GetHeight()}},Mouse)){
                    int Index = -1;
                    for(int i = 0;i<VProcesses->size;i++){
                        VProcess* try = (VProcess*)PVector_Get(VProcesses,i);
                        if(CStr_Cmp(try->Title,w->Title)){
                            Index = i;
                            break;
                        }
                    }
                    if(Index>=0){
                        VProcess_Free((VProcess*)PVector_Get(VProcesses,Index));
                        PVector_Remove(VProcesses,Index);
                    }
                    VFree(w);
                    PVector_Remove(VWindows,i);
                    (*Focused) = NULL;
                    *Picked = VWINDOW_NONE;
                    *Offset = (Vec2){0.0f,0.0f};
                    return;
                }else if(Overlap_Rect_Point((Rect){w->p,(Vec2){w->Context.w,0.02f * GetHeight()}},Mouse)){
                    *Picked = VWINDOW_GRAPPED;
                    *Offset = Vec2_Sub(w->p,Mouse);
                }else if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){0.0f,-0.01f * GetHeight()}),(Vec2){w->Context.w,0.01f * GetHeight()}},Mouse)){
                    *Picked = VWINDOW_TOP;
                }else if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){0.0f,w->Context.h + 0.01f * GetHeight()}),(Vec2){w->Context.w,0.01f * GetHeight()}},Mouse)){
                    *Picked = VWINDOW_BOTTOM;
                }else if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){0.0f,0.02f * GetHeight()}),(Vec2){0.01f * GetWidth(),w->Context.h}},Mouse)){
                    *Picked = VWINDOW_LEFT;
                }else if(Overlap_Rect_Point((Rect){Vec2_Add(w->p,(Vec2){w->Context.w-0.01f * GetWidth(),0.02f * GetHeight()}),(Vec2){0.01f * GetWidth(),w->Context.h}},Mouse)){
                    *Picked = VWINDOW_RIGHT;
                }
                goto end;
            }
        }
        if((*Focused)) (*Focused)->Focus = FALSE;
        (*Focused) = NULL;
        *Picked = VWINDOW_NONE;
        *Offset = (Vec2){0.0f,0.0f};
        end:{

        }
    }
    if(Input.DOWN){
        if((*Focused) && *Picked>0 && !(*Focused)->Min && !(*Focused)->Max) 
            if(*Picked==VWINDOW_GRAPPED){
                (*Focused)->p = Vec2_Add((Vec2){GetMouse().x,GetMouse().y},*Offset);
            }else if(*Picked==VWINDOW_TOP){
                (*Focused)->Dimension = (Vec2){(*Focused)->Dimension.x,(*Focused)->Dimension.y + ((*Focused)->p.y - GetMouse().y)};
                (*Focused)->p.y = GetMouse().y;
            }else if(*Picked==VWINDOW_BOTTOM){
                (*Focused)->Dimension = (Vec2){(*Focused)->Dimension.x,(*Focused)->Dimension.y + (GetMouse().y - ((*Focused)->p.y + (*Focused)->Dimension.y))};
            }else if(*Picked==VWINDOW_LEFT){
                (*Focused)->Dimension = (Vec2){(*Focused)->Dimension.x + ((*Focused)->p.x - GetMouse().x),(*Focused)->Dimension.y};
                (*Focused)->p.x = GetMouse().x;
            }else if(*Picked==VWINDOW_RIGHT){
                (*Focused)->Dimension = (Vec2){(*Focused)->Dimension.x + (GetMouse().x - ((*Focused)->p.x + (*Focused)->Dimension.x)),(*Focused)->Dimension.y};
            }
    }
    if(Input.RELEASED){
        if(Focused && (*Focused)) VResize((*Focused));
        *Picked = VWINDOW_NONE;
        //(*Focused) = NULL;
        *Offset = (Vec2){0.0f,0.0f};
    }
}

void VProcess_Execute(VProcess* p,void* arg){
    //p->Main(p,argc,argv);
    p->Running = TRUE;
    p->hThread = CreateThread(NULL, 0, p->Main, arg, 0, NULL);
}

void VProcess_Make(PVector* v,VWindow* w,char* Name,unsigned long (*Main)(void*)){
    PVector_Push(v,(VProcess[]){ VProcess_New(Name,Main) },sizeof(VProcess));
    VProcess* p = (VProcess*)PVector_Get(v,v->size-1);
    VProcess_Execute(p,w);
}

PVector VProcesses;
PVector VWindows;
Session VSession;

VWindow* Focused = NULL;
TextBox SearchBar;
char Picked = FALSE;
char Stretch = 0;
Vec2 Offset = { 0.0f,0.0f };

Font TitleFont;
Font Icons;
Font BigIcons;

DWORD WINAPI GameTest(LPVOID lpParam) {
    VWindow* w = (VWindow*)lpParam;
    
    Vec2 p1 = {   0.0f,  0.0f };
    Vec2 d1 = {   0.05f, 0.2f };
    Vec2 v1 = {   0.0f,  0.0f };

    Vec2 p2 = {   0.95f, 0.0f };
    Vec2 d2 = {   0.05f, 0.2f };
    Vec2 v2 = {   0.0f,  0.0f };

    int Points1 = 0;
    int Points2 = 0;

    Vec2 p3 = {   0.5f,  0.5f };
    float r = 0.02f;
    Vec2 v3 = {   0.0f,  0.0f };

    p3.x = 0.5f;
    p3.y = 0.5f;
    float a = (float)Random_f64_MinMax(0.0f,2 * PI);
    if(a>PI/4 && a<PI/4*3)      a = PI/4;
    if(a>PI/4*5 && a<PI/4*7)    a = PI/4*5;
    v3 = Vec2_Mulf(Vec2_OfAngle(a),0.01f);
    
    while(TRUE){
        VUDKB(w);
        if(VStroke(w,'W').DOWN)             v1.y = -0.03f;
        else if(VStroke(w,'S').DOWN)        v1.y =  0.03f;
        else                                    v1.y =  0.0f;

        if(VStroke(w,VK_UP).DOWN)           v2.y = -0.03f;
        else if(VStroke(w,VK_DOWN).DOWN)    v2.y =  0.03f;
        else                                    v2.y =  0.0f;

        p1.y += v1.y;
        p2.y += v2.y;
        
        p3.x += v3.x;
        p3.y += v3.y;

        if(p1.y<0.0f){
            p1.y = 0.0f;
        }
        if(p1.y>1.0f-d1.y){
            p1.y = 1.0f-d1.y;
        }
        if(p2.y<0.0f){
            p2.y = 0.0f;
        }
        if(p2.y>1.0f-d2.y){
            p2.y = 1.0f-d2.y;
        }

        if(Overlap_Rect_Rect((Rect){p1,d1},(Rect){p3,(Vec2){2.0f*r,2.0f*r}})){
            v3.x *= -1;
            v3.x += 0.005f * (v3.x>0?1:-1);
        }
        if(Overlap_Rect_Rect((Rect){p2,d2},(Rect){p3,(Vec2){2.0f*r,2.0f*r}})){
            v3.x *= -1;
            v3.x += 0.005f * (v3.x>0?1:-1);
        }

        if(p3.x<-r*2.0f){
            Points2++;
            p3.x = 0.5f;
            p3.y = 0.5f;
            float a = (float)Random_f64_MinMax(0.0f,2 * PI);
            if(a>PI/4 && a<PI/4*3)      a = PI/4;
            if(a>PI/4*5 && a<PI/4*7)    a = PI/4*5;
            v3 = Vec2_Mulf(Vec2_OfAngle(a),0.01f);
        }
        if(p3.x>1.0f){
            Points1++;
            p3.x = 0.5f;
            p3.y = 0.5f;
            float a = (float)Random_f64_MinMax(0.0f,2 * PI);
            if(a>PI/4 && a<PI/4*3)      a = PI/4;
            if(a>PI/4*5 && a<PI/4*7)    a = PI/4*5;
            v3 = Vec2_Mulf(Vec2_OfAngle(a),0.01f);
        }
        if(p3.y<0.0f){
            v3.y *= -1;
        }
        if(p3.y>1.0f - (r * 2.0f)){
            v3.y *= -1;
        }

        WaitForSingleObject(w->hMutex,INFINITE);
        Graphics_Clear(w->SwapBuffer,w->Context.w,w->Context.h,BLACK);
        Graphics_RenderRect(w->SwapBuffer,w->Context.w,w->Context.h,p1.x * w->Context.w,p1.y * w->Context.h,d1.x * w->Context.w,d1.y * w->Context.h,BLUE);
        Graphics_RenderRect(w->SwapBuffer,w->Context.w,w->Context.h,p2.x * w->Context.w,p2.y * w->Context.h,d2.x * w->Context.w,d2.y * w->Context.h,RED);
        Graphics_RenderCircle(w->SwapBuffer,w->Context.w,w->Context.h,((Vec2){(p3.x + r) * w->Context.w,(p3.y + r) * w->Context.h}),r * w->Context.w,WHITE);

        String fs = String_Format("%d : %d",Points1,Points2);
        char* cstr = String_CStr(&fs);
        Graphics_RenderCStrFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,cstr,w->Context.w*0.4f,w->Context.h*0.05f,WHITE);
        free(cstr);
        String_Free(&fs);

        VSwap(w);
        ReleaseMutex(w->hMutex);
        
        Sleep(10);
    }
    return 0;
}

DWORD WINAPI Commander(LPVOID lpParam) {
    VWindow* w = (VWindow*)lpParam;
    Cmd cmd = Cmd_Make(&VSession);
    
    char* Name = NULL;
    if(VSession.LogedIn) Name = VSession.LogedIn->Name;
    Stream_Printf(&cmd.Stdout,"User[%s]: %s $ ",Name,cmd.DirInStr);

    HANDLE hThread;
    
    while(cmd.Running){
        VUDKB(w);
        cmd.Stdin.Enabled = w->Focus;
        hThread = Cmd_Update(&VSession,&cmd,hThread);

        WaitForSingleObject(w->hMutex,INFINITE);
        Graphics_Clear(w->SwapBuffer,w->Context.w,w->Context.h,BLACK);

        int Line = 0;
        int Last = 0;
        float SizeY = (TitleFont.CharSizeY * 1.2f);
        char* cstr = String_CStr(&cmd.Stdout.Buffer);
        float Scroll = w->Context.h - (float)(CStr_CountOf(cstr,'\n')+4) * SizeY;
        if(Scroll>0) Scroll = 0;
        if(Scroll/w->Context.h<-4.0f){
            Cmd_Clear(&VSession,&cmd);
        }
        free(cstr);
        
        for(int i = 0;i<cmd.Stdout.Buffer.str.size;i++){
            if(String_Get(&cmd.Stdout.Buffer,i)=='\n'){
                int Size = i - Last;
                Graphics_RenderCStrSizeFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,(char*)cmd.Stdout.Buffer.str.Memory+Last,Size,0.0f,Scroll + Line * SizeY,WHITE);
                Last = i+1;
                Line++;
            }
            if(i==cmd.Stdout.Buffer.str.size-1){
                int Size = i - Last;
                Graphics_RenderCStrSizeFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,(char*)cmd.Stdout.Buffer.str.Memory+Last,Size,0.0f,Scroll + Line * SizeY,WHITE);
            }
        }

        Graphics_RenderCStrSizeFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,(char*)cmd.Stdin.Buffer.str.Memory,cmd.Stdin.Buffer.str.size,(cmd.Stdout.Buffer.str.size-Last) * TitleFont.CharSizeX,Scroll + Line * SizeY,WHITE);
        Graphics_RenderCharFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,'_',(cmd.Stdin.Curser+(cmd.Stdout.Buffer.str.size-Last)) * TitleFont.CharSizeX,Scroll + Line * SizeY,RED);

        VSwap(w);
        ReleaseMutex(w->hMutex);
        
        Sleep(10);
    }

    Cmd_Free(&cmd);

    return 0;
}

DWORD WINAPI Explorer(LPVOID lpParam) {
    VWindow* w = (VWindow*)lpParam;
    
    TextBox tb = TextBox_New(Input_New(100,1),(Rect){0.0f,0.0f,w->Context.w,30.0f},FONT_PATHS_BLOCKY,w->Context.h / 15,w->Context.h / 15,GRAY);
    
    Input_SetText(&tb.In,"Disk/");
    char* CStr = String_CStr(&tb.In.Buffer);
    Node* DirIn = FSystem_FindByPath(&VSession.fs,CStr);
    free(CStr);

    float Scroll = 0.0f;
    Rect Scroller = { { (w->Context.w * 0.96f),40.0f },{ (w->Context.w * 0.02f),(w->Context.h - 50.0f) } };

    while(TRUE){
        VUDKB(w);
        if(!w->Focus) tb.In.Enabled = FALSE;

        Scroller = (Rect){ { (w->Context.w * 0.96f),40.0f },{ (w->Context.w * 0.02f),(w->Context.h - 50.0f) } };

        Vector StuffIn = FSystem_DirIn(&VSession.fs,DirIn);
        float MaxScroll = -((float)(StuffIn.size+1) * (BigIcons.CharSizeY * 1.2f) - (w->Context.h - 30.0f));
        if(MaxScroll>0) MaxScroll = 0;

        if(VStroke(w,VK_LBUTTON).DOWN){
            if(VMouse(w).y<30.0f){
                tb.In.Enabled = TRUE;
            }else{
                tb.In.Enabled = FALSE;
                if(Overlap_Rect_Point(Scroller,VMouse(w))){
                    float d = (VMouse(w).y - 20.0f - Scroller.p.y) / (Scroller.d.y - 30.0f);
                    d = F32_Clamp(d,0.0f,1.0f);
                    Scroll = d * MaxScroll;
                    if(Scroll>0) Scroll = 0;
                }
            }
        }
        if(VStroke(w,VK_LBUTTON).PRESSED){
            if(VMouse(w).y<30.0f){
                char* str = String_CStr(&tb.In.Buffer);
                Node* Dir = FSystem_Path(&VSession.fs,DirIn,str);
                free(str);
                char* CStr = FSystem_PathByNode(&VSession.fs,Dir);
                Node* n = FSystem_FindByPath(&VSession.fs,CStr);
                free(CStr);
                DirIn = n;
                char* DirInStr = FSystem_PathByNode(&VSession.fs,n);
                String_Clear(&tb.In.Buffer);
                String_Append(&tb.In.Buffer,DirInStr);
                free(DirInStr);
                Scroll = 0.0f;
            }else if(VMouse(w).x<(w->Context.w * 0.95f)){
                float y = (VMouse(w).y - Scroll - 30.0f) / (BigIcons.CharSizeY * 1.2f);
                int Id = (int)y - 1;
                if(Id<0){
                    if(((File*)DirIn->Memory)->Parent) DirIn = ((File*)DirIn->Memory)->Parent;
                    char* DirInStr = FSystem_PathByNode(&VSession.fs,DirIn);
                    String_Clear(&tb.In.Buffer);
                    String_Append(&tb.In.Buffer,DirInStr);
                    free(DirInStr);
                }else{
                    if(Id>=0 && Id<StuffIn.size){
                        DirIn = *(Node**)Vector_Get(&StuffIn,Id);
                        char* DirInStr = FSystem_PathByNode(&VSession.fs,DirIn);
                        String_Clear(&tb.In.Buffer);
                        String_Append(&tb.In.Buffer,DirInStr);
                        free(DirInStr);
                    }
                }
                Scroll = 0.0f;
            }
        }
        
        Input_DefaultReact(&tb.In);
        TextBox_Update(&tb,VMouse(w));

        if(tb.r.d.x!=w->Context.w){
            tb.r.d.x = w->Context.w;
        }

        WaitForSingleObject(w->hMutex,INFINITE);
        Graphics_Clear(w->SwapBuffer,w->Context.w,w->Context.h,WHITE);

        if(DirIn){
            Graphics_RenderSubSpriteAlpha(w->SwapBuffer,w->Context.w,w->Context.h,&BigIcons.Atlas,0.0f,tb.r.d.y + Scroll,(4 % BigIcons.Columns) * BigIcons.CharSizeX,(4 / BigIcons.Columns) * BigIcons.CharSizeY,BigIcons.CharSizeX,BigIcons.CharSizeY);
            Graphics_RenderCStrFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,"..",BigIcons.CharSizeX,tb.r.d.y + Scroll,BLACK);

            for(int i = 0;i<StuffIn.size;i++){
                Node* n = *(Node**)Vector_Get(&StuffIn,i);
                File* file = (File*)n->Memory;
                int Icon = 4;//file->IsDir?4:7
                Graphics_RenderSubSpriteAlpha(w->SwapBuffer,w->Context.w,w->Context.h,&BigIcons.Atlas,0.0f,tb.r.d.y + (i+1) * (BigIcons.CharSizeY * 1.2f) + Scroll,(Icon % BigIcons.Columns) * BigIcons.CharSizeX,(Icon / BigIcons.Columns) * BigIcons.CharSizeY,BigIcons.CharSizeX,BigIcons.CharSizeY);
                Graphics_RenderCStrFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,file->FileName,BigIcons.CharSizeX,tb.r.d.y + (i+1) * (BigIcons.CharSizeY * 1.2f) + Scroll,BLACK);
                Graphics_RenderCStrFont(w->SwapBuffer,w->Context.w,w->Context.h,&TitleFont,(file->IsDir?"Directory":"File"),BigIcons.CharSizeX + TitleFont.CharSizeX * 20,tb.r.d.y + (i+1) * (BigIcons.CharSizeY * 1.2f) + Scroll,BLACK);
            }
            
            Graphics_RenderRect(w->SwapBuffer,w->Context.w,w->Context.h,Scroller.p.x,Scroller.p.y,Scroller.d.x,Scroller.d.y,GRAY);
            Graphics_RenderRect(w->SwapBuffer,w->Context.w,w->Context.h,Scroller.p.x,Scroller.p.y + (Scroll / MaxScroll * (Scroller.d.y - 30.0f)),Scroller.d.x,30.0f,DARK_GRAY);
        }
        Vector_Free(&StuffIn);
        
        Graphics_RenderTextBox(w->SwapBuffer,w->Context.w,w->Context.h,&tb);
        
        VSwap(w);
        ReleaseMutex(w->hMutex);
        
        Sleep(10);
    }

    TextBox_Free(&tb);

    return 0;
}

void ChooseFile(TextBox* SavingPath){
    OPENFILENAME ofn;
    char FileBuff[260] = { 0 };
    HWND hwnd;
    HANDLE hf;

    ZeroMemory(&ofn,sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = FileBuff;
    ofn.nMaxFile = sizeof(FileBuff);
    ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if(GetOpenFileName(&ofn)){
        //printf("File: %s\n",ofn.lpstrFile);
        Input_SetText(&SavingPath->In,ofn.lpstrFile);
    }else{
        //printf("None Selected.\n");
    }
}
void ChooseFileHigh(String* HighLighter){
    OPENFILENAME ofn;
    char FileBuff[260] = { 0 };
    HWND hwnd;
    HANDLE hf;

    ZeroMemory(&ofn,sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = FileBuff;
    ofn.nMaxFile = sizeof(FileBuff);
    ofn.lpstrFilter = "All Files\0*.*\0ALXON Files\0*.alxon\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = "C:\\Wichtig\\System\\SyntaxFiles\\";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if(GetOpenFileName(&ofn)){
        //printf("File: %s\n",ofn.lpstrFile);
        String_Clear(HighLighter);
        String_Append(HighLighter,ofn.lpstrFile);
    }else{
        //printf("None Selected.\n");
    }
}

void Button_Save(ButtonEvent* be,Button* b){
    /*if(be->e.EventType==EVENT_PRESSED){
        char* Path = String_CStr(&SavingPath.In.Buffer);
        char* Data = String_CStr(&Field.In.Buffer);

        FILE* f = fopen(Path,"w");
        if(f){
            fprintf(f,"%s",Data);
            fclose(f);
        }

        free(Path);
        free(Data);
    }*/
}
void Button_Load(ButtonEvent* be,Button* b){
    /*if(be->e.EventType==EVENT_PRESSED){
        char* Path = String_CStr(&SavingPath.In.Buffer);
        
        FILE* f = fopen(Path,"r");
        if(f){
            fseek(f,0,SEEK_END);
            int Length = ftell(f);
            fseek(f,0,SEEK_SET);

            char* BufferIn = (char*)malloc(Length+1);
            fread(BufferIn,1,Length,f);
            BufferIn[Length] = '\0';

            String_Clear(&Field.In.Buffer);
            String_Append(&Field.In.Buffer,BufferIn);
            free(BufferIn);

            Field.In.Enabled = FALSE;
            Field.In.LastKey = 0;
            Field.In.LastChar = 0;
            Field.In.CharBefore = 0;
            Field.In.KeyBefore = 0;
            Field.In.Curser = 0;
            Field.In.CurserEnd = -1;
            Field.In.Again = FALSE;
            Field.In.FirstPress = 0;
            Field.In.LastTime = 0;

            Field.ScrollX = 0;
            Field.ScrollY = 0;

            fclose(f);
        }
        free(Path);
    }*/
}
void Button_Search(ButtonEvent* be,Button* b){
    /*if(be->e.EventType==EVENT_PRESSED){
        ChooseFile();
    }*/
}
void Button_SearchHigh(ButtonEvent* be,Button* b){
    /*if(be->e.EventType==EVENT_PRESSED){
        ChooseFileHigh();

        char* cstr = String_CStr(&HighLighter);
        TextBox_SetSyntax(&Field,cstr);
        free(cstr);
    }*/
}

DWORD WINAPI Editor(LPVOID lpParam) {
    VWindow* w = (VWindow*)lpParam;
    
    int OffsetX = 100;
    int OffsetY = 100;
    int Size = 16;
    int PathSize = 25;

    char* FilePath = NULL;
    if(!FilePath) FilePath = CStr_Cpy("C:/Wichtig/Hecke/C/Win_IDE/Saved.c");

    String HighLighter = String_New();

    TextBox SavingPath = TextBox_New(Input_New(INPUT_MAXLENGTH,1),(Rect){ { OffsetX * 4.1f,OffsetY*0.5f-Size*0.5f },{ w->Context.w-OffsetX * 3.1f,OffsetY*0.5f-Size*0.5f } },FONT_PATHS_YANIS,PathSize,PathSize,WHITE);
    Input_SetText(&SavingPath.In,FilePath);
    TextBox_SetSyntax(&SavingPath,"C:/Wichtig/System/SyntaxFiles/Path_Syntax.alxon");

    TextBox Field = TextBox_New(Input_New(INPUT_MAXLENGTH,INPUT_MAXLENGTH),(Rect){ { OffsetX,OffsetY },{ w->Context.w-OffsetX,w->Context.h-OffsetY } },FONT_PATHS_YANIS,Size,Size,WHITE);
    TextBox_SetSyntax(&Field,"C:/Wichtig/System/SyntaxFiles/C_Syntax.alxon");

    Scene scene = Scene_New();
    Scene_Add(&scene,(Button[]){ Button_NewStd("Save",Button_Save,(Vec2){19.0f,19.0f},(Rect){ {0.0f,0.0f},{OffsetX,OffsetY} },RED,BLACK) },sizeof(Button));
    Scene_Add(&scene,(Button[]){ Button_NewStd("Load",Button_Load,(Vec2){19.0f,19.0f},(Rect){ {OffsetX,0.0f},{OffsetX,OffsetY} },GREEN,BLACK) },sizeof(Button));
    Scene_Add(&scene,(Button[]){ Button_NewStd("Find",Button_Search,(Vec2){19.0f,19.0f},(Rect){ {OffsetX*2,0.0f},{OffsetX,OffsetY} },BLUE,BLACK) },sizeof(Button));
    Scene_Add(&scene,(Button[]){ Button_NewStd("Color",Button_SearchHigh,(Vec2){19.0f,19.0f},(Rect){ {OffsetX*3,0.0f},{OffsetX,OffsetY} },YELLOW,BLACK) },sizeof(Button));

    while(TRUE){
        VUDKB(w);
        
        if(Field.In.Again && VStroke(w,VK_OEM_PLUS).DOWN && VStroke(w,VK_CONTROL).DOWN){
            Size += 3;
            TextBox_FontSesize(&Field,Size,Size);
        }else if(Field.In.Again && VStroke(w,VK_OEM_MINUS).DOWN && VStroke(w,VK_CONTROL).DOWN){
            Size -= 3;
            TextBox_FontSesize(&Field,Size,Size);
        }else if(Field.In.Again && VStroke(w,VK_UP).DOWN && VStroke(w,VK_CONTROL).DOWN){
            Field.ScrollY += 1;
            if(Field.ScrollY>0) Field.ScrollY = 0;
        }else if(Field.In.Again && VStroke(w,VK_DOWN).DOWN && VStroke(w,VK_CONTROL).DOWN){
            Field.ScrollY -= 1;
            char* cstr = String_CStr(&Field.In.Buffer);
            int Max = (CStr_CountOf(cstr,'\n')+4) - ((w->Context.h-OffsetY) / (Field.font.CharSizeY * INPUT_GAP_FAKTOR));
            free(cstr);
            if(Field.ScrollY<-Max)  Field.ScrollY = -Max;
            if(Field.ScrollY>0)     Field.ScrollY = 0;
        }else{
            Input_DefaultReact(&Field.In);
        }

        Scene_Update(&scene,window.Strokes,(Vec2){VMouse(w).x,VMouse(w).y});

        Input_DefaultReact(&SavingPath.In);
        TextBox_Update(&SavingPath,VMouse(w));
        TextBox_Update(&Field,VMouse(w));

        Graphics_Clear(w->SwapBuffer,w->Context.w,w->Context.h,BLACK);

        Rect_RenderXX(w->SwapBuffer,w->Context.w,w->Context.h,0.0f,0.0f,OffsetX,w->Context.h,GRAY);

        TextBox_Render(w->SwapBuffer,w->Context.w,w->Context.h,&Field);

        int Chop = 0;
        int Lines = 0;
        int Chars = 0;
        int CharsCurser = 0;
        int LinesCurser = 0;

        if(Field.ScrollY!=0){
            for(int i = Chop;i<Field.In.Buffer.str.size;i++){
                char c = String_Get(&Field.In.Buffer,i);
                if(c=='\n' || i==Field.In.Buffer.str.size-1){
                    Chars = i - Chop;
                    if(Field.In.Curser>=Chop && Field.In.Curser<=i+1){
                        CharsCurser = Field.In.Curser-Chop;
                        LinesCurser = Lines;
                    }
                    Chop = i+1;
                    Lines++;
                    if(Lines>=-Field.ScrollY) break;
                }
                if(c=='\n' && i==Field.In.Buffer.str.size-1){
                    CharsCurser = 0;
                    LinesCurser = Lines;
                    Lines++;
                    Chars = -1;
                }
            }
        }
        for(int i = Chop;i<Field.In.Buffer.str.size;i++){
            char c = String_Get(&Field.In.Buffer,i);
            if(c=='\n' || i==Field.In.Buffer.str.size-1){
                char Buff[20];
                sprintf(Buff,"%d",Lines);
                RenderCStrFont(&Field.font,Buff,0,OffsetY + (Field.ScrollY + Lines) * (Field.font.CharSizeY * INPUT_GAP_FAKTOR),BLACK);

                Chars = i - Chop;
                if(Field.In.Curser>=Chop && Field.In.Curser<=i+1){
                    CharsCurser = Field.In.Curser-Chop;
                    LinesCurser = Lines;
                }
                Chop = i+1;
                Lines++;
            }
            if(c=='\n' && i==Field.In.Buffer.str.size-1){
                CharsCurser = 0;
                LinesCurser = Lines;
                Lines++;
                Chars = -1;
            }
        }

        Rect_RenderXX(w->SwapBuffer,w->Context.w,w->Context.h,0.0f,0.0f,w->Context.w,OffsetY,DARK_GRAY);

        Scene_Render(w->SwapBuffer,w->Context.w,w->Context.h,&scene);

        TextBox_Render(w->SwapBuffer,w->Context.w,w->Context.h,&SavingPath);

        VSwap(w);
        ReleaseMutex(w->hMutex);
        
        Sleep(10);
    }

    String_Free(&HighLighter);
    TextBox_Free(&SavingPath);
    TextBox_Free(&Field);
    free(FilePath);
    Scene_Free(&scene);

    return 0;
}

int QCompare(const void* p1,const void* p2){
	VWindow* t1 = *(VWindow**)p1;
	VWindow* t2 = *(VWindow**)p2;
	return t1->TimeLifted > t2->TimeLifted;
}

void Setup(AlxWindow* w){
    TitleFont = Font_Make(FONT_BLOCKY,GetHeight() / 55,GetHeight() / 55);

    Icons = Font_Make(Sprite_Load("C:/Wichtig/System/Icons/VOS.png"),16,16,16,16,GetHeight() / 42,GetHeight() / 42);

    BigIcons = Font_Make(Sprite_Load("C:/Wichtig/System/Icons/VOS.png"),16,16,16,16,GetHeight() / 12,GetHeight() / 12);

    VProcesses = PVector_New();
    VWindows = PVector_New();
    VSession = Session_Load("C:/Wichtig/Hecke/C/Win_VirtualOS/FileSystem.alxon");
    
    SearchBar = TextBox_New(Input_New(INPUT_MAXLENGTH,1),(Rect){ { 0.1f * GetWidth(),0.9f * GetHeight() },{ 0.3f * GetWidth(),0.1f * GetHeight() } },FONT_PATHS_YANIS,0.05f * GetHeight(),0.05f * GetHeight(),BLACK);
}
void Update(AlxWindow* w){
    VHandleInput(&VProcesses,&VWindows,&Focused,&Picked,&Offset,Stroke(VK_LBUTTON));

    Clear(BLUE);
    RenderRect(0.0f * GetWidth(),0.9f * GetHeight(),0.1f * GetWidth(),0.1f * GetHeight(),GRAY);
    RenderRect(0.3f * GetWidth(),0.9f * GetHeight(),0.9f * GetWidth(),0.1f * GetHeight(),GRAY);

    Input_DefaultReact(&SearchBar.In);
    TextBox_Update(&SearchBar,GetMouse());

    char* str = String_CStr(&SearchBar.In.Buffer);
    if(SearchBar.In.Strokes[VK_RETURN].PRESSED){
        if(CStr_Cmp(str,"Cmd")){
            char* Title = str;
            PVector_Push(&VWindows,(VWindow[]){ VNew(Title,(Vec2){GetHeight() / 10,GetHeight() / 10},(Vec2){(float)GetWidth() * 0.7,(float)GetHeight() * 0.5}) },sizeof(VWindow));
            VWindow* VWindowFirst = (VWindow*)PVector_Get(&VWindows,VWindows.size-1);
            VProcess_Make(&VProcesses,VWindowFirst,Title,Commander);
        }else if(CStr_Cmp(str,"Explorer")){
            char* Title = str;
            PVector_Push(&VWindows,(VWindow[]){ VNew(Title,(Vec2){GetHeight() / 10,GetHeight() / 10},(Vec2){(float)GetWidth() * 0.7,(float)GetHeight() * 0.5}) },sizeof(VWindow));
            VWindow* VWindowFirst = (VWindow*)PVector_Get(&VWindows,VWindows.size-1);
            VProcess_Make(&VProcesses,VWindowFirst,Title,Explorer);
        }else if(CStr_Cmp(str,"Pong")){
            char* Title = str;
            PVector_Push(&VWindows,(VWindow[]){ VNew(Title,(Vec2){GetHeight() / 10,GetHeight() / 10},(Vec2){(float)GetWidth() * 0.7,(float)GetHeight() * 0.5}) },sizeof(VWindow));
            VWindow* VWindowFirst = (VWindow*)PVector_Get(&VWindows,VWindows.size-1);
            VProcess_Make(&VProcesses,VWindowFirst,Title,GameTest);
        }else if(CStr_Cmp(str,"Editor")){
            char* Title = str;
            PVector_Push(&VWindows,(VWindow[]){ VNew(Title,(Vec2){GetHeight() / 10,GetHeight() / 10},(Vec2){(float)GetWidth() * 0.7,(float)GetHeight() * 0.5}) },sizeof(VWindow));
            VWindow* VWindowFirst = (VWindow*)PVector_Get(&VWindows,VWindows.size-1);
            VProcess_Make(&VProcesses,VWindowFirst,Title,Editor);
        }
        String_Clear(&SearchBar.In.Buffer);
    }
    free(str);
    
    TextBox_Render(WINDOW_STD_ARGS,&SearchBar);
    
    qsort(VWindows.Memory,VWindows.size,sizeof(void*),QCompare);

    for(int i = 0;i<VWindows.size;i++){
        VWindow* w = (VWindow*)PVector_Get(&VWindows,i);
        
        if(Overlap_Rect_Point((Rect){0.425f * GetWidth() + 0.05f * GetWidth() * i,0.905f * GetHeight(),0.05f * GetHeight(),0.05f * GetHeight()},(Vec2){GetMouse().x,GetMouse().y})){
            if(Stroke(VK_LBUTTON).PRESSED) w->Min = !w->Min;
        }
        
        //RenderRect(0.125f * GetWidth() + 0.05f * GetWidth() * i,0.905f * GetHeight(),0.05f * GetHeight(),0.05f * GetHeight(),BLACK);
        RenderSubSpriteAlpha(&BigIcons.Atlas,0.414f * GetWidth() + 0.05f * GetWidth() * i,0.893f * GetHeight(),(w->Icon % BigIcons.Columns) * BigIcons.CharSizeX,(w->Icon / BigIcons.Columns) * BigIcons.CharSizeY,BigIcons.CharSizeX,BigIcons.CharSizeY);
        VRender(w,&TitleFont,&Icons);
    }
}
void Delete(AlxWindow* w){
    for(int i = 0;i<VProcesses.size;i++){
        VProcess* w = (VProcess*)PVector_Get(&VProcesses,i);
        VProcess_Free(w);
    }
    PVector_Free(&VProcesses);
    
    for(int i = 0;i<VWindows.size;i++){
        VWindow* w = (VWindow*)PVector_Get(&VWindows,i);
        VFree(w);
    }
    PVector_Free(&VWindows);
    Font_Free(&TitleFont);
    Font_Free(&Icons);
    Font_Free(&BigIcons);

    TextBox_Free(&SearchBar);
    
    Session_Free(&VSession);
}

int main(int argc,const char *argv[]){
    if(Create("Virtual Box",1800,1000,1,1,Setup,Update,Delete)){
        Start();
    }
    return 0;
}