//Includes and linkers
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "d3dx10.lib")
#pragma comment (lib, "D3D10_1.lib")
#pragma comment (lib, "DXGI.lib")
#pragma comment (lib, "D2D1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")

#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>
#include <xnamath.h>
#include <D3D10_1.h>
#include <DXGI.h>
#include <D2D1.h>
#include <sstream>
#include <dwrite.h>
#include <dinput.h>
#include <vector>
#include <fstream>
#include <istream>
#include <iostream>
#include "ConfigFile.h"

ConfigFile cf("config.txt");

//Globals
IDXGISwapChain* _pSwapChain;
ID3D11Device* _pd3dDevice;
ID3D11DeviceContext* _pImmediateContext;
ID3D11RenderTargetView* _pRenderTargetView;
ID3D11DepthStencilView* _depthStencilView;
ID3D11Texture2D* _depthStencilBuffer;

ID3D11VertexShader* VS;
ID3D11PixelShader* PS;
ID3D11GeometryShader* GS;
ID3D10Blob* GS_Buffer;
ID3D11PixelShader* D2D_PS;
ID3D10Blob* D2D_PS_Buffer;
ID3D10Blob* VS_Buffer;
ID3D10Blob* PS_Buffer;

ID3D11InputLayout* vertLayout;
ID3D11Buffer* cbPerObjectBuffer;
ID3D11BlendState* d2dTransparency;
ID3D11RasterizerState* CCWcullMode;
ID3D11RasterizerState* CWcullMode;
ID3D11SamplerState* CubesTexSamplerState;
ID3D11Buffer* cbPerFrameBuffer;
ID3D10Device1 *d3d101Device;	

ID2D1RenderTarget *D2DRenderTarget;	
ID2D1SolidColorBrush *Brush;
ID3D11Texture2D *BackBuffer11;
ID3D11Texture2D *sharedTex11;	
ID3D11Buffer *d2dVertBuffer;
ID3D11Buffer *d2dIndexBuffer;
ID3D11ShaderResourceView *d2dTexture;
IDWriteFactory *DWriteFactory;
IDWriteTextFormat *TextFormat;
IDirectInputDevice8* DIKeyboard;
IDirectInputDevice8* DIMouse;
ID3D11Buffer* sphereIndexBuffer;
ID3D11Buffer* sphereVertBuffer;
ID3D11VertexShader* SKYMAP_VS;
ID3D11PixelShader* SKYMAP_PS;
ID3D10Blob* SKYMAP_VS_Buffer;
ID3D10Blob* SKYMAP_PS_Buffer;
ID3D11ShaderResourceView* smrv;
ID3D11DepthStencilState* DSLessEqual;
ID3D11RasterizerState* RSCullNone;
ID3D11BlendState* Transparency;
ID3D11BlendState* leafTransparency;
ID3D11ShaderResourceView* treeBillboardSRV;

//Global Declarations
LPCTSTR WndClassName = L"firstwindow";
HWND hwnd = NULL;
HRESULT hr;

// Integers
int Width  = cf.Value("section_2", "Width");
int Height = cf.Value("section_2", "Height");

int NumSphereVertices;
int NumSphereFaces;

// Floating Point
float rotx = cf.Value("section_2", "rotx");
float rotz = cf.Value("section_2", "rotx");
float scaleX = cf.Value("section_2", "scaleX");
float scaleY = cf.Value("section_2", "scaleX");

float charCamDist = cf.Value("section_2", "charCamDist"); //This is the distance between the camera and the character

float moveLeftRight = cf.Value("section_2", "moveLeftRight");
float moveBackForward = cf.Value("section_2", "moveLeftRight");

float camYaw = cf.Value("section_2", "moveLeftRight");
float camPitch = cf.Value("section_2", "moveLeftRight");

//Matrices
XMMATRIX Rotationx;
XMMATRIX Rotationz;
XMMATRIX Rotationy;

XMMATRIX WVP;
XMMATRIX camView;
XMMATRIX camProjection;
XMMATRIX d2dWorld;

XMMATRIX camRotationMatrix;

XMMATRIX Rotation;
XMMATRIX Scale, Scale2;
XMMATRIX Translation, Translation2;

XMMATRIX carWorldM;
XMMATRIX treeWorld; 
XMMATRIX sphereWorld;
XMMATRIX barrierWorld;

//Vectors
XMVECTOR camPosition;
XMVECTOR camTarget;
XMVECTOR camUp;
XMVECTOR DefaultForward = XMVectorSet(0.0f,0.0f,1.0f, 0.0f);
XMVECTOR DefaultRight = XMVectorSet(1.0f,0.0f,0.0f, 0.0f);
XMVECTOR camForward = XMVectorSet(0.0f,0.0f,1.0f, 0.0f);
XMVECTOR camRight = XMVectorSet(1.0f,0.0f,0.0f, 0.0f);

XMVECTOR currCharDirection = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR oldCharDirection = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR charPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

XMVECTOR aiCurrCharDirection = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR aiOldCharDirection = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
XMVECTOR aiPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

//Camera BOOLS
bool firstPerson = false;
bool reversePerson = false;
bool topDown = false;

// Other Variables
DIMOUSESTATE mouseLastState;
LPDIRECTINPUT8 DirectInput;
std::wstring printText;
bool moveAI = false;

// Clock (Timer) variables
double countsPerSecond = 0.0;
__int64 CounterStart = 0;

int frameCount = 0;
int fps = 0;

__int64 frameTimeOld = 0;
double frameTime;

//Ground Mesh variables
ID3D11Buffer* meshVertBuff;
ID3D11Buffer* meshIndexBuff;
int meshSubsets = 0;
std::vector<int> meshSubsetIndexStart;
std::vector<int> meshSubsetTexture;
XMMATRIX meshWorld;

//Car mesh
ID3D11Buffer* carMeshVertBuff;
ID3D11Buffer* carMeshIndexBuff;
int carMeshSubsets = 0;
std::vector<int> carMeshSubsetIndexStart;
std::vector<int> carMeshSubsetTexture;
XMMATRIX carMeshWorld;

//Enemy mesh
ID3D11Buffer* enemyMeshVertBuff;
ID3D11Buffer* enemyMeshIndexBuff;
int enemyMeshSubsets = 0;
std::vector<int> enemyMeshSubsetIndexStart;
std::vector<int> enemyMeshSubsetTexture;
XMMATRIX enemyMeshWorld;

//Textures and material variables, used for all mesh's loaded
std::vector<ID3D11ShaderResourceView*> meshSRV;
std::vector<std::wstring> textureNameArray;

//Const Variables
const int numLeavesPerTree = 1000;
const int numTrees = 4000;
const int numBarriers = 200;

//Leaf data (leaves are drawn as quads)
ID3D11ShaderResourceView* leafTexture;
ID3D11Buffer *quadVertBuffer;
ID3D11Buffer *quadIndexBuffer;

//Tree data
ID3D11Buffer* treeInstanceBuff;
ID3D11Buffer* treeVertBuff;
ID3D11Buffer* treeIndexBuff;
int treeSubsets = 0;
std::vector<int> treeSubsetIndexStart;
std::vector<int> treeSubsetTexture;

//Barrier data
ID3D11Buffer* barrierInstanceBuff;
ID3D11Buffer* barrierVertBuff;
ID3D11Buffer* barrierIndexBuff;
int barrierSubsets = 0;
std::vector<int> barrierSubsetIndexStart;
std::vector<int> barrierSubsetTexture;

std::vector<XMFLOAT3> treeAABB;

int numTreesToDraw;
int numBarriersToDraw = 200;

//Structs
struct cbPerObject
{
	XMMATRIX  WVP;
	XMMATRIX World;
		
	XMFLOAT4 difColor; //Used for pixel shader
	BOOL hasTexture;
	BOOL hasNormMap;

	BOOL isInstance;
	BOOL isLeaf;
};
cbPerObject cbPerObj;

//This constant buffer is updated once per scene
struct cbPerScene
{
	float treeBillWidth;
	float treeBillHeight;
	
	float pad1;
	float pad2;
	
	XMMATRIX leafOnTree[numLeavesPerTree];
};
cbPerScene cbPerInst;
ID3D11Buffer* cbPerInstanceBuffer;
ID3D11InputLayout* leafVertLayout;

//Instance Buffer
struct InstanceData
{
	XMFLOAT3 pos;
};

std::vector<InstanceData> treeInstanceData(numTrees);
std::vector<InstanceData> treeBillboardInstanceData(numTrees);
std::vector<InstanceData> barrierInstanceData(numBarriers);
int numBillboardTreesToDraw;
ID3D11Buffer* treeBillboardInstanceBuff;
ID3D11Buffer* treeBillboardVertBuff;

//Create material structure
struct SurfaceMaterial
{
	std::wstring matName;
	XMFLOAT4 difColor;
	int texArrayIndex;
	int normMapTexArrayIndex;
	bool hasNormMap;
	bool hasTexture;
	bool transparent;
};

std::vector<SurfaceMaterial> material;

//Object loader function
bool LoadObjModel(std::wstring filename,			//.obj filename
	ID3D11Buffer** vertBuff,					//mesh vertex buffer
	ID3D11Buffer** indexBuff,					//mesh index buffer
	std::vector<int>& subsetIndexStart,			//start index of each subset
	std::vector<int>& subsetMaterialArray,		//index value of material for each subset
	std::vector<SurfaceMaterial>& material,		//vector of material structures
	int& subsetCount,							//Number of subsets in mesh
	bool isRHCoordSys,							//true if model was created in right hand coord system
	bool computeNormals,						//true to compute the normals, false to use the files normals
	std::vector<XMFLOAT3> &AABB);				//Objects AABB (Axis-Aligned Bounding Box)

struct Light
{
	Light()
	{
		ZeroMemory(this, sizeof(Light));
	}
	XMFLOAT3 pos;
	float range;
	XMFLOAT3 dir;
	float cone;
	XMFLOAT3 att;
	float pad2;
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
};

Light light;

struct cbPerFrame
{
	Light  light;
	XMVECTOR camPos;
};

cbPerFrame constbuffPerFrame;

struct Vertex //Vertex structure
{
	Vertex(){}
	Vertex(float x, float y, float z,
		float u, float v,
		float nx, float ny, float nz,
		float tx, float ty, float tz)
		: pos(x,y,z), texCoord(u, v), normal(nx, ny, nz),
		tangent(tx, ty, tz){}

	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
	XMFLOAT3 biTangent;

	int StartWeight;
	int WeightCount;
};

D3D11_INPUT_ELEMENT_DESC layout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
	{ "NORMAL",	 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "INSTANCEPOS", 0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1}
};
UINT numElements = ARRAYSIZE(layout);

D3D11_INPUT_ELEMENT_DESC leafLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
	{ "NORMAL",	 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{ "INSTANCEPOS", 0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D11_INPUT_PER_INSTANCE_DATA, numLeavesPerTree}
};
UINT numLeafElements = ARRAYSIZE(leafLayout);

struct BoundingBox
{
	XMFLOAT3 min;
	XMFLOAT3 max;
};

struct FrameData
{
	int frameID;
	std::vector<float> frameData;
};

//Functions
bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, int width, int height, bool windowed);
bool InitializeDirect3d11App(HINSTANCE hInstance);
bool InitDirectInput(HINSTANCE hInstance);
int messageloop();
void CleanUp();
bool InitScene();
void DrawScene();
void InitD2DScreenTexture();
void UpdateScene(double time);
void UpdateCamera();
void UpdateAI(XMMATRIX& worldMatrix);
void StartTimer();
double GetTime();
double GetFrameTime();
void DetectInput(double time);
void CreateSphere(int LatLines, int LongLines);
LRESULT CALLBACK WndProc(HWND hWnd,	UINT msg, WPARAM wParam, LPARAM lParam);

//Culling Functions
std::vector<XMFLOAT3> CreateAABB(std::vector<XMFLOAT3> &vertPosArray);
void cullAABB(std::vector<XMFLOAT4> &frustumPlanes);
std::vector<XMFLOAT4> getFrustumPlanes(XMMATRIX& viewProj);
void getBillboards(std::vector<InstanceData> &instanceDataPos);
