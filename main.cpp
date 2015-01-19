#include "main.h"

int WINAPI WinMain(HINSTANCE hInstance,	//Main windows function
	HINSTANCE hPrevInstance, 
	LPSTR lpCmdLine,
	int nShowCmd)
{

	if(!InitializeWindow(hInstance, nShowCmd, Width, Height, true)) //Initalise window
	{
		MessageBox(0, L"Window Initialisation - Failed",
			L"Error", MB_OK);
		return 0;
	}

	if(!InitializeDirect3d11App(hInstance))	//Initialise Direct3D
	{
		MessageBox(0, L"Direct3D Initialisation - Failed",
			L"Error", MB_OK);
		return 0;
	}

	if(!InitScene()) //Initialise our scene
	{
		MessageBox(0, L"Scene Initialisation - Failed",
			L"Error", MB_OK);
		return 0;
	}

	if(!InitDirectInput(hInstance)) //Initalise our input function
	{
		MessageBox(0, L"Direct Input Initialisation - Failed",
			L"Error", MB_OK);
		return 0;
	}

	messageloop();

	CleanUp();    

	return 0;
}


int messageloop(){
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));
	while(true)
	{
		BOOL PeekMessageL( 
			LPMSG lpMsg,
			HWND hWnd,
			UINT wMsgFilterMin,
			UINT wMsgFilterMax,
			UINT wRemoveMsg
			);

		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);	
			DispatchMessage(&msg);
		}
		else
		{
			//Run game loop  
			frameCount++;
			if(GetTime() > 1.0f)
			{
				fps = frameCount;
				frameCount = 0;
				StartTimer();
			}	

			frameTime = GetFrameTime();

			DetectInput(frameTime);
			UpdateScene(frameTime);
			DrawScene();
		}
	}
	return msg.wParam;
}

std::vector<XMFLOAT4> getFrustumPlanes(XMMATRIX& viewProj)
{
	//x, y, z, and w represent A, B, C and D in the plane equation
	//where ABC are the xyz of the planes normal, and D is the plane constant
	std::vector<XMFLOAT4> tempFrustumPlane(6);

	//Left Frustum Plane
	//Add first column of the matrix to the fourth column
	tempFrustumPlane[0].x = viewProj._14 + viewProj._11; 
	tempFrustumPlane[0].y = viewProj._24 + viewProj._21;
	tempFrustumPlane[0].z = viewProj._34 + viewProj._31;
	tempFrustumPlane[0].w = viewProj._44 + viewProj._41;

	//Right Frustum Plane
	//Subtract first column of matrix from the fourth column
	tempFrustumPlane[1].x = viewProj._14 - viewProj._11; 
	tempFrustumPlane[1].y = viewProj._24 - viewProj._21;
	tempFrustumPlane[1].z = viewProj._34 - viewProj._31;
	tempFrustumPlane[1].w = viewProj._44 - viewProj._41;

	//Top Frustum Plane
	//Subtract second column of matrix from the fourth column
	tempFrustumPlane[2].x = viewProj._14 - viewProj._12; 
	tempFrustumPlane[2].y = viewProj._24 - viewProj._22;
	tempFrustumPlane[2].z = viewProj._34 - viewProj._32;
	tempFrustumPlane[2].w = viewProj._44 - viewProj._42;

	//Bottom Frustum Plane
	//Add second column of the matrix to the fourth column
	tempFrustumPlane[3].x = viewProj._14 + viewProj._12;
	tempFrustumPlane[3].y = viewProj._24 + viewProj._22;
	tempFrustumPlane[3].z = viewProj._34 + viewProj._32;
	tempFrustumPlane[3].w = viewProj._44 + viewProj._42;

	//Near Frustum Plane
	//Third column is near the plane already
	tempFrustumPlane[4].x = viewProj._13;
	tempFrustumPlane[4].y = viewProj._23;
	tempFrustumPlane[4].z = viewProj._33;
	tempFrustumPlane[4].w = viewProj._43;

	//Far Frustum Plane
	//Subtract third column of matrix from the fourth column
	tempFrustumPlane[5].x = viewProj._14 - viewProj._13; 
	tempFrustumPlane[5].y = viewProj._24 - viewProj._23;
	tempFrustumPlane[5].z = viewProj._34 - viewProj._33;
	tempFrustumPlane[5].w = viewProj._44 - viewProj._43;

	//Normalize plane normals (A, B and C (xyz))
	for(int i = 0; i < 6; ++i)
	{
		float length = sqrt((tempFrustumPlane[i].x * tempFrustumPlane[i].x) + (tempFrustumPlane[i].y * tempFrustumPlane[i].y) + (tempFrustumPlane[i].z * tempFrustumPlane[i].z));
		tempFrustumPlane[i].x /= length;
		tempFrustumPlane[i].y /= length;
		tempFrustumPlane[i].z /= length;
		tempFrustumPlane[i].w /= length;
	}

	return tempFrustumPlane;
}

void getBillboards(std::vector<InstanceData> &instanceDataPos)
{
	int numFullyDrawnTrees = 0;
	numBillboardTreesToDraw = 0;

	for(int i = 0; i < numTreesToDraw; ++i)
	{
		//Find the distance on the XZ plane between the tree and the camera
		XMVECTOR treeToCam = camPosition - XMLoadFloat3(&instanceDataPos[i].pos);
		treeToCam = XMVectorSetY(treeToCam, 0.0f);

		float treeToCamDist = XMVectorGetX(XMVector3Length(treeToCam));

		//If tree is further than 200 units from camera, make it a billboard
		if(treeToCamDist < 200.0f)
		{
			instanceDataPos[numFullyDrawnTrees].pos = instanceDataPos[i].pos;
			numFullyDrawnTrees++;
		}
		else
		{
			treeBillboardInstanceData[numBillboardTreesToDraw].pos = instanceDataPos[i].pos;
			numBillboardTreesToDraw++;
		}
	}

	numTreesToDraw = numFullyDrawnTrees;
	_pImmediateContext->UpdateSubresource( treeBillboardInstanceBuff, 0, NULL, &treeBillboardInstanceData[0], 0, 0 );
}

void cullAABB(std::vector<XMFLOAT4> &frustumPlanes)
{
	//Function to check tree objects for culling (if they are outside of the view frustum)

	//Initialize numTreesToDraw
	numTreesToDraw = 0;

	//Create a temporary array to get the tree instance data out
	std::vector<InstanceData> tempTreeInstDat(numTrees);

	bool cull = false;

	//Loop through all the trees
	for(int i = 0; i < numTrees; ++i)
	{
		cull = false;
		//Loop through each frustum plane
		for(int planeID = 0; planeID < 6; ++planeID)
		{
			XMVECTOR planeNormal = XMVectorSet(frustumPlanes[planeID].x, frustumPlanes[planeID].y, frustumPlanes[planeID].z, 0.0f);
			float planeConstant = frustumPlanes[planeID].w;

			//Check each axis (x,y,z) to get the AABB vertex furthest away from the direction the plane is facing (plane normal)
			XMFLOAT3 axisVert;

			//x-axis
			if(frustumPlanes[planeID].x < 0.0f)	// Which AABB vertex is furthest down (plane normals direction) the x axis
				axisVert.x = treeAABB[0].x + treeInstanceData[i].pos.x; // min x plus tree positions x
			else
				axisVert.x = treeAABB[1].x + treeInstanceData[i].pos.x; // max x plus tree positions x

			//y-axis
			if(frustumPlanes[planeID].y < 0.0f)	// Which AABB vertex is furthest down (plane normals direction) the y axis
				axisVert.y = treeAABB[0].y + treeInstanceData[i].pos.y; // min y plus tree positions y
			else
				axisVert.y = treeAABB[1].y + treeInstanceData[i].pos.y; // max y plus tree positions y

			//z-axis
			if(frustumPlanes[planeID].z < 0.0f)	// Which AABB vertex is furthest down (plane normals direction) the z axis
				axisVert.z = treeAABB[0].z + treeInstanceData[i].pos.z; // min z plus tree positions z
			else
				axisVert.z = treeAABB[1].z + treeInstanceData[i].pos.z; // max z plus tree positions z

			//Now we get the signed distance from the AABB vertex that's furthest down the frustum planes normal,
			//and if the signed distance is negative, then the entire bounding box is behind the frustum plane, which means
			//that it should be culled
			if(XMVectorGetX(XMVector3Dot(planeNormal, XMLoadFloat3(&axisVert))) + planeConstant < 0.0f)
			{
				cull = true;
				//Skip remaining planes to check and move on to next tree
				break;
			}
		}

		if(!cull)	//If the object wasn't culled
		{
			//Set the treesToDrawIndex in the constant buffer
			tempTreeInstDat[numTreesToDraw].pos = treeInstanceData[i].pos;

			//Add one to the number of trees to draw
			numTreesToDraw++;
		}
	}

	getBillboards(tempTreeInstDat);
	
	//Update our instance buffer with our new (newly ordered) array of tree instance data (positions)
	_pImmediateContext->UpdateSubresource( treeInstanceBuff, 0, NULL, &tempTreeInstDat[0], 0, 0 );
}

std::vector<XMFLOAT3> CreateAABB(std::vector<XMFLOAT3> &vertPosArray)
{
	//Function to create bounding box out of the vertex position array
	XMFLOAT3 minVertex = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	XMFLOAT3 maxVertex = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for(UINT i = 0; i < vertPosArray.size(); i++)
	{		
		//Get the smallest vertex 
		minVertex.x = min(minVertex.x, vertPosArray[i].x);	//Find smallest x value in model
		minVertex.y = min(minVertex.y, vertPosArray[i].y);	//Find smallest y value in model
		minVertex.z = min(minVertex.z, vertPosArray[i].z);	//Find smallest z value in model

		//Get the largest vertex 
		maxVertex.x = max(maxVertex.x, vertPosArray[i].x);	//Find largest x value in model
		maxVertex.y = max(maxVertex.y, vertPosArray[i].y);	//Find largest y value in model
		maxVertex.z = max(maxVertex.z, vertPosArray[i].z);	//Find largest z value in model
	}	

	std::vector<XMFLOAT3> AABB;

	//AABB [0] is the min vertex, [1] is the max, and [2] is the objects actual center
	AABB.push_back(minVertex);
	AABB.push_back(maxVertex);

	//Find distance between maxVertex and minVertex
	float distX = (maxVertex.x - minVertex.x) / 2.0f;
	float distY = (maxVertex.y - minVertex.y) / 2.0f;
	float distZ = (maxVertex.z - minVertex.z) / 2.0f;	

	//Store the distance between (0, 0, 0) in model space to the models real center
	AABB.push_back(XMFLOAT3(maxVertex.x - distX, maxVertex.y - distY, maxVertex.z - distZ));

	return AABB;
}

bool InitScene()
{
	InitD2DScreenTexture();

	CreateSphere(10, 10);

	std::vector<XMFLOAT3> tempAABB;

	//Load in our models
	if(!LoadObjModel(L"ground.obj", &meshVertBuff, &meshIndexBuff, meshSubsetIndexStart, meshSubsetTexture, material, meshSubsets, true, true, tempAABB))
		return false;

	if(!LoadObjModel(L"car.obj", &carMeshVertBuff, &carMeshIndexBuff, carMeshSubsetIndexStart, carMeshSubsetTexture, material, carMeshSubsets, true, true, tempAABB))
		return false;

	if(!LoadObjModel(L"natla.obj", &enemyMeshVertBuff, &enemyMeshIndexBuff, enemyMeshSubsetIndexStart, enemyMeshSubsetTexture, material, enemyMeshSubsets, true, true, tempAABB))
		return false;

	if(!LoadObjModel(L"tree.obj", &treeVertBuff, &treeIndexBuff, treeSubsetIndexStart, treeSubsetTexture, material, treeSubsets, true, true, treeAABB))
		return false;

	if(!LoadObjModel(L"concrete_roadblock_02.obj", &barrierVertBuff, &barrierIndexBuff, barrierSubsetIndexStart, barrierSubsetTexture, material, barrierSubsets, true, true, tempAABB))
		return false;
	
	//Set up the tree positions then it's instance buffer
	XMVECTOR tempPos;
	srand(100);
	
	for(int i = 0; i < numTrees; i++)
	{
		bool outsideTrack = false;
		float randX = ((float)(rand() % 20000) / 10) - 1000;
		float randZ = ((float)(rand() % 20000) / 10) - 1000;
		while (!outsideTrack)
		{
			if (((randX > -35.0f) && (randZ < 30.0f)) && ((randX < 35.0f) && (randZ < 30.0f)))
			{
				randZ = ((float)(rand() % 20000) / 10) - 1000;
				randX = ((float)(rand() % 20000) / 10) - 1000;
			}
			else
			{
				tempPos = XMVectorSet(randX, 0.0f, randZ, 0.0f);
				outsideTrack = true;
			}
		}
		XMStoreFloat3(&treeInstanceData[i].pos, tempPos);
	}

	//Create our trees instance buffer
	D3D11_BUFFER_DESC instBuffDesc;	
	ZeroMemory( &instBuffDesc, sizeof(instBuffDesc) );

	instBuffDesc.Usage = D3D11_USAGE_DEFAULT;
	instBuffDesc.ByteWidth = sizeof( InstanceData ) * numTrees;
	instBuffDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instBuffDesc.CPUAccessFlags = 0;
	instBuffDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA instData;
	ZeroMemory( &instData, sizeof(instData) );

	instData.pSysMem = &treeInstanceData[0];
	hr = _pd3dDevice->CreateBuffer( &instBuffDesc, &instData, &treeInstanceBuff);

	//Create the tree's world matrix
	treeWorld = XMMatrixIdentity();
		
	//Set up the barrier positions then it's instance buffer
	XMVECTOR tempPos2;
	XMMATRIX rotationMatrix1;
	XMMATRIX tempMatrix1;
	srand(100);
	for(int i = 0; i < 100; i++)
	{
		float randX = 25.0f;
		float randZ = -(float)8 * i;
		tempPos2 = XMVectorSet(randX, 0.0f, randZ, 0.0f);
		XMStoreFloat3(&barrierInstanceData[i].pos, tempPos2);
		Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f );
		Translation = XMMatrixTranslation(barrierInstanceData[i].pos.x, barrierInstanceData[i].pos.y + 8.0f, barrierInstanceData[i].pos.z );
		tempMatrix1 = Scale * Translation;
	}

	for (int i = 0; i < 100; i++)
	{
		float randX = -25.0f;
		float randZ = -(float)8 * i;
		tempPos2 = XMVectorSet(randX, 0.0f, randZ, 0.0f);
		XMStoreFloat3(&barrierInstanceData[i+100].pos, tempPos2);
		Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f );
		Translation = XMMatrixTranslation(barrierInstanceData[i+100].pos.x, barrierInstanceData[i+100].pos.y + 8.0f, barrierInstanceData[i+100].pos.z );
		tempMatrix1 = Scale * Translation;
	}

	//Create our barriers instance buffer
	D3D11_BUFFER_DESC instBuffDesc2;	
	ZeroMemory( &instBuffDesc2, sizeof(instBuffDesc2) );

	instBuffDesc2.Usage = D3D11_USAGE_DEFAULT;
	instBuffDesc2.ByteWidth = sizeof( InstanceData ) * numBarriers;
	instBuffDesc2.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instBuffDesc2.CPUAccessFlags = 0;
	instBuffDesc2.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA instData2;
	ZeroMemory( &instData2, sizeof(instData2) );

	instData2.pSysMem = &barrierInstanceData[0];
	hr = _pd3dDevice->CreateBuffer(&instBuffDesc2, &instData2, &barrierInstanceBuff);

	//The barrier's world matrix
	barrierWorld = XMMatrixIdentity();
		
	//Create Leaf geometry (standard quad)
	Vertex v[] =
	{
		//Front Face
		Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(-1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f),
		Vertex( 1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f),
		Vertex( 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f),
	};

	DWORD indices[] = {
		//Front Face
		0,  1,  2,
		0,  2,  3,
	};

	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory( &indexBufferDesc, sizeof(indexBufferDesc) );

	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(DWORD) * 2 * 3;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;

	iinitData.pSysMem = indices;
	_pd3dDevice->CreateBuffer(&indexBufferDesc, &iinitData, &quadIndexBuffer);
	
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory( &vertexBufferDesc, sizeof(vertexBufferDesc) );

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof( Vertex ) * 4;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData; 

	ZeroMemory( &vertexBufferData, sizeof(vertexBufferData) );
	vertexBufferData.pSysMem = v;
	hr = _pd3dDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, &quadVertBuffer);

	//Now we load in the leaf texture (leaf.png)
	hr = D3DX11CreateShaderResourceViewFromFile(_pd3dDevice, L"leaf.png", NULL, NULL, &leafTexture, NULL );

	//Create each individual leaf's world matrix and add it to the cbPerInst buffer

	//Start by initializing the matrix array
	srand(100);
	XMFLOAT3 fTPos;
	XMMATRIX rotationMatrix;
	XMMATRIX tempMatrix;
	for(int i = 0; i < numLeavesPerTree; i++)
	{
		float rotX =(rand() % 2000) / 500.0f; //Value between 0 and 4 PI (two circles)
		float rotY = (rand() % 2000) / 500.0f;
		float rotZ = (rand() % 2000) / 500.0f;

		float distFromCenter = 6.0f - ((rand() % 1000) / 250.0f);	

		if(distFromCenter > 4.0f)
			distFromCenter = 4.0f;

		//Rotate the vector to create a sort of sphere
		tempPos = XMVectorSet(distFromCenter, 0.0f, 0.0f, 0.0f);
		rotationMatrix = XMMatrixRotationRollPitchYaw(rotX, rotY, rotZ);
		tempPos = XMVector3TransformCoord(tempPos, rotationMatrix );

		if(XMVectorGetY(tempPos) < -1.0f)
			tempPos = XMVectorSetY(tempPos, -XMVectorGetY(tempPos));

		//Create the leaves "tree" matrix (this is not the leaves "world matrix", because we are not
		//defining the leaves position, orientation, and scale in world space, but instead in "tree" space
		XMStoreFloat3(&fTPos, tempPos);

		Scale = XMMatrixScaling( 0.25f, 0.25f, 0.25f );
		Translation = XMMatrixTranslation(fTPos.x, fTPos.y + 8.0f, fTPos.z );
		tempMatrix = Scale * rotationMatrix * Translation;

		//To make things simple, we just store the matrix directly into our cbPerInst structure
		cbPerInst.leafOnTree[i] = XMMatrixTranspose(tempMatrix);
	}

	//Since the leaves go out further than the branches of the tree, we need to modify the bounding box to take
	//into account the leaves radius of 4 units from the center of the tree. We add the extra .25 since we scale
	//the leaves down to 25% of their original size. 
	treeAABB[0].x = -4.25f;
	treeAABB[0].z = -4.25f;
	treeAABB[1].x = 4.25f;
	treeAABB[1].z = 4.25f;
	treeAABB[1].y = 12.25f;

	//Recalculate the AABB's center
	XMStoreFloat3(&treeAABB[2], ((XMLoadFloat3(&treeAABB[1]) - XMLoadFloat3(&treeAABB[0])) / 2.0f) + XMLoadFloat3(&treeAABB[0]));

	//Store the billboards width and height in the inst cb
	cbPerInst.treeBillWidth = treeAABB[1].x - treeAABB[0].x; 
	cbPerInst.treeBillHeight = treeAABB[1].y - treeAABB[0].y; 
	
	//Compile Shaders from shader file (lighting.fx)
	hr = D3DX11CompileFromFile(L"Lighting.fx", 0, 0, "VS", "vs_4_0", 0, 0, 0, &VS_Buffer, 0, 0);
	hr = D3DX11CompileFromFile(L"Lighting.fx", 0, 0, "PS", "ps_4_0", 0, 0, 0, &PS_Buffer, 0, 0);
	hr = D3DX11CompileFromFile(L"Lighting.fx", 0, 0, "D2D_PS", "ps_4_0", 0, 0, 0, &D2D_PS_Buffer, 0, 0);
	hr = D3DX11CompileFromFile(L"Lighting.fx", 0, 0, "SKYMAP_VS", "vs_4_0", 0, 0, 0, &SKYMAP_VS_Buffer, 0, 0);
	hr = D3DX11CompileFromFile(L"Lighting.fx", 0, 0, "SKYMAP_PS", "ps_4_0", 0, 0, 0, &SKYMAP_PS_Buffer, 0, 0);
	hr = D3DX11CompileFromFile(L"Lighting.fx", 0, 0, "GS_Billboard", "gs_4_0", 0, 0, 0, &GS_Buffer, 0, 0);

	//Create the Shader Objects
	hr = _pd3dDevice->CreateVertexShader(VS_Buffer->GetBufferPointer(), VS_Buffer->GetBufferSize(), NULL, &VS);
	hr = _pd3dDevice->CreatePixelShader(PS_Buffer->GetBufferPointer(), PS_Buffer->GetBufferSize(), NULL, &PS);

	hr = _pd3dDevice->CreatePixelShader(D2D_PS_Buffer->GetBufferPointer(), D2D_PS_Buffer->GetBufferSize(), NULL, &D2D_PS);
	hr = _pd3dDevice->CreateVertexShader(SKYMAP_VS_Buffer->GetBufferPointer(), SKYMAP_VS_Buffer->GetBufferSize(), NULL, &SKYMAP_VS);
	hr = _pd3dDevice->CreatePixelShader(SKYMAP_PS_Buffer->GetBufferPointer(), SKYMAP_PS_Buffer->GetBufferSize(), NULL, &SKYMAP_PS);
	hr = _pd3dDevice->CreateGeometryShader(GS_Buffer->GetBufferPointer(), GS_Buffer->GetBufferSize(), NULL, &GS);
	
	//Set Vertex and Pixel Shaders
	_pImmediateContext->VSSetShader(VS, 0, 0);
	_pImmediateContext->PSSetShader(PS, 0, 0);

	light.pos = XMFLOAT3(0.0f, 7.0f, 0.0f);
	light.dir = XMFLOAT3(-0.5f, 0.75f, -0.5f);
	light.range = 1000.0f;
	light.cone = 12.0f;
	light.att = XMFLOAT3(0.4f, 0.02f, 0.000f);
	light.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	light.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	//Create the Input Layout
	hr = _pd3dDevice->CreateInputLayout( layout, numElements, VS_Buffer->GetBufferPointer(), 
		VS_Buffer->GetBufferSize(), &vertLayout );

	//Create the leaf Input Layout (instance is different for the leaves)
	hr = _pd3dDevice->CreateInputLayout( leafLayout, numLeafElements, VS_Buffer->GetBufferPointer(), 
		VS_Buffer->GetBufferSize(), &leafVertLayout );

	//Set the Input Layout
	_pImmediateContext->IASetInputLayout( vertLayout );

	//Set Primitive Topology
	_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	//Create the Viewport
	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = Width;
	viewport.Height = Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//Set the Viewport
	_pImmediateContext->RSSetViewports(1, &viewport);

	//Create the buffer to send to the cbuffer in lighting file
	D3D11_BUFFER_DESC cbbd;	
	ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

	cbbd.Usage = D3D11_USAGE_DEFAULT;
	cbbd.ByteWidth = sizeof(cbPerObject);
	cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbbd.CPUAccessFlags = 0;
	cbbd.MiscFlags = 0;

	hr = _pd3dDevice->CreateBuffer(&cbbd, NULL, &cbPerObjectBuffer);

	//Create the buffer to send to the cbuffer per frame in lighting file
	ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

	cbbd.Usage = D3D11_USAGE_DEFAULT;
	cbbd.ByteWidth = sizeof(cbPerFrame);
	cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbbd.CPUAccessFlags = 0;
	cbbd.MiscFlags = 0;

	hr = _pd3dDevice->CreateBuffer(&cbbd, NULL, &cbPerFrameBuffer);

	//Create the buffer to send to the cbuffer per instance in lighting file
	ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));

	cbbd.Usage = D3D11_USAGE_DEFAULT;
	cbbd.ByteWidth = sizeof(cbPerScene);
	cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbbd.CPUAccessFlags = 0;
	cbbd.MiscFlags = 0;

	hr = _pd3dDevice->CreateBuffer(&cbbd, NULL, &cbPerInstanceBuffer);

	//Set the constant buffer per instance
	_pImmediateContext->UpdateSubresource(cbPerInstanceBuffer, 0, NULL, &cbPerInst, 0, 0);

	//Make sure models and scaled/positioned correctly using the config file
	Scale = XMMatrixScaling( 1.0f, 1.0f, 1.0f );			
	Translation = XMMatrixTranslation(cf.Value("playerStartLocation", "playerStartX"), cf.Value("playerStartLocation", "playerStartY"), cf.Value("playerStartLocation", "playerStartZ"));
	carWorldM = Scale * Translation;

	Scale2 = Scale = XMMatrixScaling( 2.0f, 2.0f, 2.0f );	
	Translation2 = XMMatrixTranslation(cf.Value("enemyStartLocation", "enemyStartX"), cf.Value("enemyStartLocation", "enemyStartY"), cf.Value("enemyStartLocation", "enemyStartZ"));
	enemyMeshWorld = Scale2 * Translation2;
	
	//Camera information
	camPosition = XMVectorSet( 0.0f, 10.0f, 8.0f, 0.0f );
	camTarget = XMVectorSet( 0.0f, 3.0f, 0.0f, 0.0f );
	camUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	//Set the View matrix
	camView = XMMatrixLookAtLH( camPosition, camTarget, camUp );

	//Set the Projection matrix
	camProjection = XMMatrixPerspectiveFovLH( 3.14f/4.0f, (float)Width/Height, 1.0f, 1000.0f);

	D3D11_BLEND_DESC blendDesc;
	ZeroMemory( &blendDesc, sizeof(blendDesc) );

	D3D11_RENDER_TARGET_BLEND_DESC rtbd;
	ZeroMemory( &rtbd, sizeof(rtbd) );

	rtbd.BlendEnable			 = true;
	rtbd.SrcBlend				 = D3D11_BLEND_SRC_COLOR;
	rtbd.DestBlend				 = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.BlendOp				 = D3D11_BLEND_OP_ADD;
	rtbd.SrcBlendAlpha			 = D3D11_BLEND_ONE;
	rtbd.DestBlendAlpha			 = D3D11_BLEND_ZERO;
	rtbd.BlendOpAlpha			 = D3D11_BLEND_OP_ADD;
	rtbd.RenderTargetWriteMask	 = D3D10_COLOR_WRITE_ENABLE_ALL;

	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.RenderTarget[0] = rtbd;

	_pd3dDevice->CreateBlendState(&blendDesc, &d2dTransparency);

	ZeroMemory( &rtbd, sizeof(rtbd) );

	rtbd.BlendEnable			 = true;
	rtbd.SrcBlend				 = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.DestBlend				 = D3D11_BLEND_SRC_ALPHA;
	rtbd.BlendOp				 = D3D11_BLEND_OP_ADD;
	rtbd.SrcBlendAlpha			 = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.DestBlendAlpha			 = D3D11_BLEND_SRC_ALPHA;
	rtbd.BlendOpAlpha			 = D3D11_BLEND_OP_ADD;
	rtbd.RenderTargetWriteMask	 = D3D10_COLOR_WRITE_ENABLE_ALL;

	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.RenderTarget[0] = rtbd;

	_pd3dDevice->CreateBlendState(&blendDesc, &Transparency);

	ZeroMemory( &rtbd, sizeof(rtbd) );

	rtbd.BlendEnable			 = true;
	rtbd.SrcBlend				 = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.DestBlend				 = D3D11_BLEND_SRC_ALPHA;
	rtbd.BlendOp				 = D3D11_BLEND_OP_ADD;
	rtbd.SrcBlendAlpha			 = D3D11_BLEND_INV_SRC_ALPHA;
	rtbd.DestBlendAlpha			 = D3D11_BLEND_SRC_ALPHA;
	rtbd.BlendOpAlpha			 = D3D11_BLEND_OP_ADD;
	rtbd.RenderTargetWriteMask	 = D3D10_COLOR_WRITE_ENABLE_ALL;

	blendDesc.AlphaToCoverageEnable = true;
	blendDesc.RenderTarget[0] = rtbd;

	_pd3dDevice->CreateBlendState(&blendDesc, &leafTransparency);

	//Load the Skymap texture (skymap.dds)
	D3DX11_IMAGE_LOAD_INFO loadSMInfo;
	loadSMInfo.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	ID3D11Texture2D* SMTexture = 0;
	hr = D3DX11CreateTextureFromFile(_pd3dDevice, L"desert_skymap.dds", &loadSMInfo, 0, (ID3D11Resource**)&SMTexture, 0);

	if(FAILED(hr))
	{
		_pSwapChain->SetFullscreenState(false, NULL);	//Make sure fullscreen is false

		//Create error message
		std::wstring message = L"Could not open: desert_skymap.dds";

		MessageBox(0, message.c_str(),	//Display error message
			L"Error", MB_OK);

		return false;
	}

	D3D11_TEXTURE2D_DESC SMTextureDesc;
	SMTexture->GetDesc(&SMTextureDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC SMViewDesc;
	SMViewDesc.Format = SMTextureDesc.Format;
	SMViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	SMViewDesc.TextureCube.MipLevels = SMTextureDesc.MipLevels;
	SMViewDesc.TextureCube.MostDetailedMip = 0;

	hr = _pd3dDevice->CreateShaderResourceView(SMTexture, &SMViewDesc, &smrv);

	//Describe the Sample State
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory( &sampDesc, sizeof(sampDesc) );
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;    
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	//Create the Sample State
	hr = _pd3dDevice->CreateSamplerState( &sampDesc, &CubesTexSamplerState );

	D3D11_RASTERIZER_DESC cmdesc;

	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.CullMode = D3D11_CULL_BACK;
	cmdesc.FrontCounterClockwise = true;
	hr = _pd3dDevice->CreateRasterizerState(&cmdesc, &CCWcullMode);

	cmdesc.FrontCounterClockwise = false;

	hr = _pd3dDevice->CreateRasterizerState(&cmdesc, &CWcullMode);

	cmdesc.CullMode = D3D11_CULL_NONE;
	hr = _pd3dDevice->CreateRasterizerState(&cmdesc, &RSCullNone);

	D3D11_DEPTH_STENCIL_DESC dssDesc;
	ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dssDesc.DepthEnable = true;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	_pd3dDevice->CreateDepthStencilState(&dssDesc, &DSLessEqual);

	//Create the billboards instance buffer
	ZeroMemory( &instBuffDesc, sizeof(instBuffDesc) );

	instBuffDesc.Usage = D3D11_USAGE_DEFAULT;
	instBuffDesc.ByteWidth = sizeof( InstanceData ) * numTrees;
	instBuffDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instBuffDesc.CPUAccessFlags = 0;
	instBuffDesc.MiscFlags = 0;

	ZeroMemory( &instData, sizeof(instData) );

	//This will be filled later but create it now
	hr = _pd3dDevice->CreateBuffer( &instBuffDesc, 0, &treeBillboardInstanceBuff);

	//Create the vertex buffer that will be sent to the shaders for the billboard data
	D3D11_BUFFER_DESC billboardBufferDesc;
	ZeroMemory( &billboardBufferDesc, sizeof(billboardBufferDesc) );

	billboardBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	billboardBufferDesc.ByteWidth = sizeof(Vertex);
	billboardBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	billboardBufferDesc.CPUAccessFlags = 0;
	billboardBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA ginitData;

	//Create a single vertex at the point 0,0,0. None of the other members will be used for billboarding
	Vertex gv;
	gv.pos = XMFLOAT3(0.0f, 0.0f, 0.0f);

	ginitData.pSysMem = &gv;
	_pd3dDevice->CreateBuffer(&billboardBufferDesc, &ginitData, &treeBillboardVertBuff);

	//The function above changes the viewport, so we have to change the viewport back to the one we defined above
	_pImmediateContext->RSSetViewports(1, &viewport);

	//Load in the billboard texture (treeBillTexture.png)
	hr = D3DX11CreateShaderResourceViewFromFile( _pd3dDevice, L"treeBillTexture.png", NULL, NULL, &treeBillboardSRV, NULL );
	
	return true;
}

void DrawScene()
{
	//Clear the render target and depth/stencil view
	float bgColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	_pImmediateContext->ClearRenderTargetView(_pRenderTargetView, bgColor);	
	_pImmediateContext->ClearDepthStencilView(_depthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	constbuffPerFrame.light = light;
	constbuffPerFrame.camPos = camPosition;
	_pImmediateContext->UpdateSubresource( cbPerFrameBuffer, 0, NULL, &constbuffPerFrame, 0, 0 );
	_pImmediateContext->PSSetConstantBuffers(0, 1, &cbPerFrameBuffer);	

	//Set the Render Target
	_pImmediateContext->OMSetRenderTargets( 1, &_pRenderTargetView, _depthStencilView );

	cullAABB(getFrustumPlanes(camView * camProjection));
	
	//Use same blending as the d2d stuff
	_pImmediateContext->OMSetBlendState(d2dTransparency, NULL, 0xffffffff);

	//Set topology to points
	_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );

	//Set the shaders, the vertex shader is the default one
	_pImmediateContext->VSSetShader(VS, 0, 0);
	//The geometry shader is the only one defined in the effects file at the time
	_pImmediateContext->GSSetShader(GS, 0, 0);
	//The pixel shader is the one used for d2d, since all that is needed is to render a texture to the billboard
	_pImmediateContext->PSSetShader(D2D_PS, 0, 0);

	//Draw billboards
	UINT strides[2] = {sizeof( Vertex ), sizeof( InstanceData )};
	UINT offsets[2] = {0, 0};

	//Store the vertex and instance buffers into an array (leaves use same instance buffers as trees)
	ID3D11Buffer* vertBillInstBuffers[2] = {treeBillboardVertBuff, treeBillboardInstanceBuff};

	//Set the models vertex and isntance buffer using the arrays created above
	_pImmediateContext->IASetVertexBuffers( 0, 2, vertBillInstBuffers, strides, offsets );

	//Set the WVP matrix and send it to the constant buffer in lighting file
	WVP = camView * camProjection;

	cbPerObj.WVP = XMMatrixTranspose(WVP);	
	cbPerObj.World = XMMatrixTranspose(treeWorld);		
	cbPerObj.hasTexture = true;		
	cbPerObj.hasNormMap = false;	
	cbPerObj.isInstance = true;		//Check if it's using instanced data
	cbPerObj.isLeaf = false;		//And if that is the leaf instancing
	_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );	

	//Two constant buffers are being sent to the vertex shader now, so create an array of them
	ID3D11Buffer* vsBillConstBuffers[2] = {cbPerObjectBuffer, cbPerInstanceBuffer};
	_pImmediateContext->VSSetConstantBuffers( 0, 2, vsBillConstBuffers );
	//Set Geometry shaders (3 cb's in this case)
	ID3D11Buffer* gsBillConstBuffers[3] = {cbPerFrameBuffer, cbPerObjectBuffer, cbPerInstanceBuffer};
	_pImmediateContext->GSSetConstantBuffers(0, 3, gsBillConstBuffers);	
	_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );
	_pImmediateContext->PSSetShaderResources( 0, 1, &treeBillboardSRV );
	_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

	_pImmediateContext->RSSetState(RSCullNone);
	_pImmediateContext->DrawInstanced( 1, numBillboardTreesToDraw, 0, 0 );

	//Set gs and ps back to defaults
	_pImmediateContext->GSSetShader(NULL, 0, 0);
	_pImmediateContext->PSSetShader(PS, 0, 0);

	//Reset topology back to triangles
	_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	//Reset the default blend state (no blending) for opaque objects
	_pImmediateContext->OMSetBlendState(0, 0, 0xffffffff);
	
	//Draw instanced leaf models
	//Store the vertex and instance buffers into an array
	//The leaves use the same instance buffer as the trees, because each leaf needs to go to a certain tree
	ID3D11Buffer* vertInstBuffers[2] = {quadVertBuffer, treeInstanceBuff};

	//Set the leaf input layout. This is where the special input layout is used
	_pImmediateContext->IASetInputLayout( leafVertLayout );

	//Set the models index buffer
	_pImmediateContext->IASetIndexBuffer(quadIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	//Set the models vertex and isntance buffer using the arrays created above
	_pImmediateContext->IASetVertexBuffers(0, 2, vertInstBuffers, strides, offsets );

	//Set the WVP matrix and send it to the constant buffer in effect file
	WVP = treeWorld * camView * camProjection;

	cbPerObj.WVP = XMMatrixTranspose(WVP);	
	cbPerObj.World = XMMatrixTranspose(treeWorld);		
	cbPerObj.hasTexture = true;		
	cbPerObj.hasNormMap = false;	
	cbPerObj.isInstance = true;		//Tell shaders if this is instanced data
	cbPerObj.isLeaf = true;		//Tell shaders if this is the leaf instance
	_pImmediateContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );	

	//Two constant buffers again
	ID3D11Buffer* vsConstBuffers[2] = {cbPerObjectBuffer, cbPerInstanceBuffer};
	_pImmediateContext->VSSetConstantBuffers( 0, 2, vsConstBuffers );
	_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );
	_pImmediateContext->PSSetShaderResources( 0, 1, &leafTexture );
	_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

	_pImmediateContext->RSSetState(RSCullNone);
	_pImmediateContext->DrawIndexedInstanced( 6, numLeavesPerTree * numTreesToDraw, 0, 0, 0 );

	//Reset the default Input Layout
	_pImmediateContext->IASetInputLayout( vertLayout );

	//Draw the tree instances
	for(int i = 0; i < treeSubsets; ++i)
	{
		//Store the vertex and instance buffers into an array
		ID3D11Buffer* vertInstBuffers[2] = {treeVertBuff, treeInstanceBuff};

		//Set the models index buffer
		_pImmediateContext->IASetIndexBuffer(treeIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the models vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 2, vertInstBuffers, strides, offsets );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = treeWorld * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(treeWorld);	
		cbPerObj.difColor = material[treeSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[treeSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = material[treeSubsetTexture[i]].hasNormMap;
		cbPerObj.isInstance = true;	
		cbPerObj.isLeaf = false;	
		
		_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );

		if(material[treeSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[treeSubsetTexture[i]].texArrayIndex] );
		
		if(material[treeSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[treeSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = treeSubsetIndexStart[i];
		int indexDrawAmount =  treeSubsetIndexStart[i+1] - treeSubsetIndexStart[i];
		if(!material[treeSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexedInstanced( indexDrawAmount, numTreesToDraw, indexStart, 0, 0 );
	}
	
	//Draw the barrier instances
	for(int i = 0; i < barrierSubsets; ++i)
	{
		//Store the vertex and instance buffers into an array
		ID3D11Buffer* vertInstBuffers[2] = {barrierVertBuff, barrierInstanceBuff};

		//Set the models index buffer
		_pImmediateContext->IASetIndexBuffer(barrierIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the models vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 2, vertInstBuffers, strides, offsets );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = barrierWorld * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(barrierWorld);	
		cbPerObj.difColor = material[barrierSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[barrierSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = material[barrierSubsetTexture[i]].hasNormMap;
		cbPerObj.isInstance = true;	
		cbPerObj.isLeaf = false;	
		
		_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );

		if(material[barrierSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[barrierSubsetTexture[i]].texArrayIndex] );
		
		if(material[barrierSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[barrierSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = barrierSubsetIndexStart[i];
		int indexDrawAmount =  barrierSubsetIndexStart[i+1] - barrierSubsetIndexStart[i];
		if(!material[barrierSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexedInstanced( indexDrawAmount, numBarriersToDraw, indexStart, 0, 0 );
	}

	UINT stride = sizeof( Vertex );
	UINT offset = 0;

	//Set Vertex and Pixel Shaders
	_pImmediateContext->VSSetShader(VS, 0, 0);
	_pImmediateContext->PSSetShader(PS, 0, 0);
		
	//Draw the grounds's NON-transparent subsets (transparent subsets go later)
	for(int i = 0; i < meshSubsets; ++i)
	{
		//Set the grounds index buffer
		_pImmediateContext->IASetIndexBuffer( meshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 1, &meshVertBuff, &stride, &offset );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = meshWorld * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(meshWorld);	
		cbPerObj.difColor = material[meshSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[meshSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = material[meshSubsetTexture[i]].hasNormMap;
		cbPerObj.isInstance = false;	
		cbPerObj.isLeaf = false;
		
		_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );

		if(material[meshSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[meshSubsetTexture[i]].texArrayIndex] );
		
		if(material[meshSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[meshSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = meshSubsetIndexStart[i];
		int indexDrawAmount =  meshSubsetIndexStart[i+1] - meshSubsetIndexStart[i];
		if(!material[meshSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexed( indexDrawAmount, indexStart, 0 );
	}

	//Draw the Cars's NON-transparent subsets
	for(int i = 0; i < carMeshSubsets; ++i)
	{
		//Set the grounds index buffer
		_pImmediateContext->IASetIndexBuffer(carMeshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 1, &carMeshVertBuff, &stride, &offset );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = carWorldM * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(carWorldM);	
		cbPerObj.difColor = material[carMeshSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[carMeshSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = false;
		cbPerObj.isInstance = false;		
		cbPerObj.isLeaf = false;	
		
		_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );
		
		if(material[carMeshSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[carMeshSubsetTexture[i]].texArrayIndex] );
		
		if(material[carMeshSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[carMeshSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = carMeshSubsetIndexStart[i];
		int indexDrawAmount =  carMeshSubsetIndexStart[i+1] - carMeshSubsetIndexStart[i];
		if(!material[carMeshSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexed(indexDrawAmount, indexStart, 0 );
	}

	//Draw the enemys NON-transparent subsets
	for(int i = 0; i < enemyMeshSubsets; ++i)
	{
		//Set the grounds index buffer
		_pImmediateContext->IASetIndexBuffer(enemyMeshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 1, &enemyMeshVertBuff, &stride, &offset );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = enemyMeshWorld * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(enemyMeshWorld);	
		cbPerObj.difColor = material[enemyMeshSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[enemyMeshSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = false;
		cbPerObj.isInstance = false;		
		cbPerObj.isLeaf = false;
		
		_pImmediateContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );
		
		if(material[enemyMeshSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[enemyMeshSubsetTexture[i]].texArrayIndex] );
		
		if(material[enemyMeshSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[enemyMeshSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = enemyMeshSubsetIndexStart[i];
		int indexDrawAmount =  enemyMeshSubsetIndexStart[i+1] - enemyMeshSubsetIndexStart[i];
		if(!material[enemyMeshSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexed(indexDrawAmount, indexStart, 0 );
	}

	//Draw the Sky's Sphere
	//Set the spheres index buffer
	_pImmediateContext->IASetIndexBuffer( sphereIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	//Set the spheres vertex buffer
	_pImmediateContext->IASetVertexBuffers( 0, 1, &sphereVertBuffer, &stride, &offset );

	//Set the WVP matrix and send it to the constant buffer in effect file
	WVP = sphereWorld * camView * camProjection;
	cbPerObj.WVP = XMMatrixTranspose(WVP);	
	cbPerObj.World = XMMatrixTranspose(sphereWorld);
	cbPerObj.isInstance = false;		
	cbPerObj.isLeaf = false;	
	_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
	_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
	_pImmediateContext->PSSetShaderResources( 0, 1, &smrv ); //Send the skymap resource view to pixel shader
	_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

	//Set the new VS and PS shaders
	_pImmediateContext->VSSetShader(SKYMAP_VS, 0, 0);
	_pImmediateContext->PSSetShader(SKYMAP_PS, 0, 0);
	//Set the new depth/stencil and RS states
	_pImmediateContext->OMSetDepthStencilState(DSLessEqual, 0);
	_pImmediateContext->RSSetState(RSCullNone);
	_pImmediateContext->DrawIndexed( NumSphereFaces * 3, 0, 0 );	

	//Set the default VS, PS shaders and depth/stencil state
	_pImmediateContext->VSSetShader(VS, 0, 0);
	_pImmediateContext->PSSetShader(PS, 0, 0);
	_pImmediateContext->OMSetDepthStencilState(NULL, 0);

	//Draw the TRANSPARENT subsets now (after the change in blend state)
	//Set our blend state
	_pImmediateContext->OMSetBlendState(Transparency, NULL, 0xffffffff);

	//Draw the ground's TRANSPARENT subsets
	for(int i = 0; i < meshSubsets; ++i)
	{
		//Set the grounds index buffer
		_pImmediateContext->IASetIndexBuffer( meshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 1, &meshVertBuff, &stride, &offset );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = meshWorld * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(meshWorld);	
		cbPerObj.difColor = material[meshSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[meshSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = material[meshSubsetTexture[i]].hasNormMap;
		cbPerObj.isInstance = false;		
		cbPerObj.isLeaf = false;		
		
		_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );
		
		if(material[meshSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[meshSubsetTexture[i]].texArrayIndex] );
		
		if(material[meshSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[meshSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = meshSubsetIndexStart[i];
		int indexDrawAmount =  meshSubsetIndexStart[i+1] - meshSubsetIndexStart[i];

		if(material[meshSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexed( indexDrawAmount, indexStart, 0 );
	}

	//Draw the cars TRANSPARENT subsets
	for(int i = 0; i < carMeshSubsets; ++i)
	{
		//Set the grounds index buffer
		_pImmediateContext->IASetIndexBuffer(carMeshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 1, &carMeshVertBuff, &stride, &offset );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = carWorldM * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(carWorldM);	
		cbPerObj.difColor = material[carMeshSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[carMeshSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = false;
		cbPerObj.isInstance = false;		
		cbPerObj.isLeaf = false;		
		
		_pImmediateContext->UpdateSubresource( cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );
		
		if(material[carMeshSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[carMeshSubsetTexture[i]].texArrayIndex] );
		
		if(material[carMeshSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[carMeshSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = carMeshSubsetIndexStart[i];
		int indexDrawAmount =  carMeshSubsetIndexStart[i+1] - carMeshSubsetIndexStart[i];

		if(material[carMeshSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexed( indexDrawAmount, indexStart, 0 );
	}

	//Draw the enemyes TRANSPARENT subsets
	for(int i = 0; i < enemyMeshSubsets; ++i)
	{
		//Set the grounds index buffer
		_pImmediateContext->IASetIndexBuffer(enemyMeshIndexBuff, DXGI_FORMAT_R32_UINT, 0);
		//Set the grounds vertex buffer
		_pImmediateContext->IASetVertexBuffers( 0, 1, &enemyMeshVertBuff, &stride, &offset );

		//Set the WVP matrix and send it to the constant buffer in effect file
		WVP = enemyMeshWorld * camView * camProjection;
		cbPerObj.WVP = XMMatrixTranspose(WVP);	
		cbPerObj.World = XMMatrixTranspose(enemyMeshWorld);	
		cbPerObj.difColor = material[enemyMeshSubsetTexture[i]].difColor;
		cbPerObj.hasTexture = material[enemyMeshSubsetTexture[i]].hasTexture;
		cbPerObj.hasNormMap = false;
		cbPerObj.isInstance = false;		
		cbPerObj.isLeaf = false;		
		
		_pImmediateContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0 );
		_pImmediateContext->VSSetConstantBuffers( 0, 1, &cbPerObjectBuffer );
		_pImmediateContext->PSSetConstantBuffers( 1, 1, &cbPerObjectBuffer );
		
		if(material[enemyMeshSubsetTexture[i]].hasTexture)
			_pImmediateContext->PSSetShaderResources( 0, 1, &meshSRV[material[enemyMeshSubsetTexture[i]].texArrayIndex] );
		
		if(material[enemyMeshSubsetTexture[i]].hasNormMap)
			_pImmediateContext->PSSetShaderResources( 1, 1, &meshSRV[material[enemyMeshSubsetTexture[i]].normMapTexArrayIndex] );
		
		_pImmediateContext->PSSetSamplers( 0, 1, &CubesTexSamplerState );

		_pImmediateContext->RSSetState(RSCullNone);
		int indexStart = enemyMeshSubsetIndexStart[i];
		int indexDrawAmount =  enemyMeshSubsetIndexStart[i+1] - enemyMeshSubsetIndexStart[i];

		if(material[enemyMeshSubsetTexture[i]].transparent)
			_pImmediateContext->DrawIndexed( indexDrawAmount, indexStart, 0 );
	}

	//Present the backbuffer to the screen
	_pSwapChain->Present(0, 0);
}

void MoveChar(XMMATRIX& worldMatrix, bool n, bool e, bool s, bool w)
{
	//Get the current characters position in the world, from it's world matrix
	charPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	charPosition = XMVector3TransformCoord(charPosition, worldMatrix);

	//Prevent the car from passing the sides of the track and going too far backward
	if(XMVectorGetX(charPosition) > 25.0f)
		charPosition = charPosition - XMVectorSet(0.3f, 0.0f, 0.0f, 0.0f);
	if(XMVectorGetX(charPosition) < -25.0f)
		charPosition = charPosition + XMVectorSet(0.3f, 0.0f, 0.0f, 0.0f);
	if(XMVectorGetZ(charPosition) > 20.0f)
		charPosition = charPosition - XMVectorSet(0.0f, 0.0f, 0.8f, 0.0f);
	if(XMVectorGetZ(charPosition) < -1000.0f)
		charPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	
	//Basic boolean logic to work out which way the car is travelling
	XMVECTOR destinationDirection;
	if (n == true)
	{
		if (e == true)
			destinationDirection = XMVectorSet(-0.8f, 0.0f, -2.8f, 0.0f);
		else if (w == true)
			destinationDirection = XMVectorSet(0.8f, 0.0f, -2.8f, 0.0f);
		else
			destinationDirection = XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f);
	}
	else if (s == true)
	{
		if (e == true)
			destinationDirection = XMVectorSet(0.8f, 0.0f, 2.8f, 0.0f);
		else if (w == true)
			destinationDirection = XMVectorSet(-0.8f, 0.0f, 2.8f, 0.0f);
		else
			destinationDirection = XMVectorSet(0.0f, 0.0f, 3.0f, 0.0f);
	}
	else
		destinationDirection = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	
	//Rotate the character smoothly when changing direction
	float destDirLength = 0.1f;	//Left and right rotation speed
	if(!s) //If not reversing
		if(XMVectorGetZ(oldCharDirection) > 0.0f)
			currCharDirection = destinationDirection; //Reset the currCharDirection back to forwards if the user was reversing
		else
			currCharDirection = (oldCharDirection) + (destinationDirection * destDirLength); //Forward movement
	else
		currCharDirection = destinationDirection;
				
	currCharDirection = XMVector3Normalize(currCharDirection);	//Normalize the cars current direction vector

	//The angle on the car needs to be flipped (if moving left) so the car moves correctly
	float charDirAngle = XMVectorGetX(XMVector3AngleBetweenNormals( XMVector3Normalize(currCharDirection), XMVector3Normalize(DefaultForward)));
	if(XMVectorGetY(XMVector3Cross(currCharDirection, DefaultForward)) > 0.0f)
		charDirAngle = -charDirAngle;

	//Update cars position based off the speed, old position, and the direction it is facing
	float speed = 15.0f * frameTime;
	charPosition = charPosition + (destinationDirection * speed);

	//Update cars world matrix
	XMMATRIX rotationMatrix;
	Scale = XMMatrixScaling( 1.0f, 1.0f, 1.0f );
		
	Translation = XMMatrixTranslation(XMVectorGetX(charPosition), 0.0f, XMVectorGetZ(charPosition) );
	if(s)
	{
		rotationMatrix = XMMatrixRotationY(charDirAngle); //If reversing make sure car still faces forwards
	}
	else
		rotationMatrix = XMMatrixRotationY(charDirAngle - 3.14159265f);	//Subtract PI from angle so the car doesn't move backwards
	
	worldMatrix = Scale * rotationMatrix * Translation;

	//Set the cars old direction
	oldCharDirection = currCharDirection;
}

void UpdateCamera()
{
	if (reversePerson == true)
	{
		camPitch = 0.0f;
		camYaw = 0.0f;
		camRotationMatrix = XMMatrixRotationRollPitchYaw(0, 0, 0);
		camTarget = XMVector3TransformCoord(-oldCharDirection, camRotationMatrix );
		camTarget = XMVector3Normalize(camTarget);

		XMMATRIX RotateYTempMatrix;
		RotateYTempMatrix = XMMatrixRotationY(camPitch);

		camRight = XMVector3TransformCoord(DefaultRight, RotateYTempMatrix);
		camUp = XMVector3TransformCoord(camUp, RotateYTempMatrix);
		camForward = XMVector3TransformCoord(DefaultForward, RotateYTempMatrix);

		camPosition = charPosition + XMVectorSet(0.0f, 5.0f, 0.0f, 0.0f);

		camTarget = camPosition + camTarget;	

		camView = XMMatrixLookAtLH( camPosition, camTarget, camUp );
	}
	else if (firstPerson == true)
	{
		camPitch = 0.0f;
		camYaw = 0.0f;
		camRotationMatrix = XMMatrixRotationRollPitchYaw(0, 0, 0);
		camTarget = XMVector3TransformCoord(oldCharDirection, camRotationMatrix );
		camTarget = XMVector3Normalize(camTarget);

		XMMATRIX RotateYTempMatrix;
		RotateYTempMatrix = XMMatrixRotationY(camPitch);

		camRight = XMVector3TransformCoord(DefaultRight, RotateYTempMatrix);
		camUp = XMVector3TransformCoord(camUp, RotateYTempMatrix);
		camForward = XMVector3TransformCoord(DefaultForward, RotateYTempMatrix);

		camPosition = charPosition + XMVectorSet(0.0f, 5.0f, 0.0f, 0.0f);

		camTarget = camPosition + camTarget;	

		camView = XMMatrixLookAtLH( camPosition, camTarget, camUp );
	}
	else if (topDown == true)
	{
		camPitch = 20.1f;
		camYaw = 0.0f;
		camTarget = charPosition;
		camTarget = XMVectorSetY(camTarget, XMVectorGetY(camTarget)+50.0f);	

		camRotationMatrix = XMMatrixRotationRollPitchYaw(-camPitch, camYaw, 0);
		camPosition = XMVector3TransformNormal(DefaultForward, camRotationMatrix );
		camPosition = XMVector3Normalize(camPosition);

		camPosition = (camPosition * charCamDist) + camTarget;

		camForward = XMVector3Normalize(camTarget - camPosition);	//Get forward vector based on target
		camForward = XMVectorSetY(camForward, 0.0f);	//Set forwards y component to 0 so it lays only on the xz plane
		camForward = XMVector3Normalize(camForward);
	
		camRight = XMVectorSet(-XMVectorGetZ(camForward), 0.0f, XMVectorGetX(camForward), 0.0f);

		camUp =XMVector3Normalize(XMVector3Cross(XMVector3Normalize(camPosition - camTarget), camRight));

		camView = XMMatrixLookAtLH( camPosition, camTarget, camUp );
		camPitch = 0.0f;
	}
	else
	{
		//Third Person Camera rotateable camera
		camTarget = charPosition;
		camTarget = XMVectorSetY(camTarget, XMVectorGetY(camTarget)+5.0f);	

		camRotationMatrix = XMMatrixRotationRollPitchYaw(-camPitch, camYaw, 0);
		camPosition = XMVector3TransformNormal(DefaultForward, camRotationMatrix );
		camPosition = XMVector3Normalize(camPosition);

		camPosition = (camPosition * charCamDist) + camTarget;

		camForward = XMVector3Normalize(camTarget - camPosition);	//Get forward vector based on target
		camForward = XMVectorSetY(camForward, 0.0f);	//Set forwards y component to 0 so it lays only on the xz plane
		camForward = XMVector3Normalize(camForward);
	
		camRight = XMVectorSet(-XMVectorGetZ(camForward), 0.0f, XMVectorGetX(camForward), 0.0f);

		camUp =XMVector3Normalize(XMVector3Cross(XMVector3Normalize(camPosition - camTarget), camRight));

		camView = XMMatrixLookAtLH( camPosition, camTarget, camUp );
	}
	
}

void DetectInput(double time)
{
	DIMOUSESTATE mouseCurrState;

	BYTE keyboardState[256];

	DIKeyboard->Acquire();
	DIMouse->Acquire();

	DIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrState);

	DIKeyboard->GetDeviceState(sizeof(keyboardState),(LPVOID)&keyboardState);

	if(keyboardState[DIK_ESCAPE] & 0x80)
		PostMessage(hwnd, WM_DESTROY, 0, 0);

	float speed = 10.0f * time;	
	bool moveChar = false;
	bool left = false;
	bool right = false;
	bool up = false; 
	bool down = false;
	if(keyboardState[DIK_A] & 0x80)
	{
		left = true;
	}
	else
		left = false;
	if(keyboardState[DIK_D] & 0x80)
	{
		right = true;
	}
	else
		right = false;
	if(keyboardState[DIK_W] & 0x80)
	{
		up = true;
		moveChar = true;
		moveAI = true;
	}
	else
	{
		up = false;
	}
	if(keyboardState[DIK_S] & 0x80)
	{
		down = true;
		moveChar = true;
	}
	else
		down = false;
	if(keyboardState[DIK_F] & 0x80)
	{
		firstPerson = true;
	}
	else
		firstPerson = false;
	if(keyboardState[DIK_R] & 0x80)
	{
		reversePerson = true;
	}
	else
		reversePerson = false;
	if(keyboardState[DIK_T] & 0x80)
	{
		topDown = true;
	}
	else
		topDown = false;
	if(keyboardState[DIK_P] & 0x80)
		moveAI = true;
		
	if((mouseCurrState.lX != mouseLastState.lX) || (mouseCurrState.lY != mouseLastState.lY))
	{
		camYaw += mouseLastState.lX * 0.002f;

		camPitch += mouseCurrState.lY * 0.002f;

		//Check that the camera doesn't go over the top or under the car
		if(!topDown)
		{
			if(camPitch > 0.85f)
				camPitch = 0.85f;
			if(camPitch < -0.85f)
				camPitch = -0.85f;
		}
		
		mouseLastState = mouseCurrState;
	}

	if(moveChar == true)
		MoveChar(carWorldM, up, right, down, left);

	UpdateCamera();

	if(moveAI == true)
		UpdateAI(enemyMeshWorld);

	return;
}

void UpdateAI(XMMATRIX& worldMatrix)
{
	aiPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	aiPosition = XMVector3TransformCoord(aiPosition, worldMatrix);

	if(XMVectorGetZ(aiPosition) < -1000.0f)
		aiPosition = XMVectorSet(10.0f, 0.0f, 0.0f, 0.0f);

	XMVECTOR destinationDirection;
	destinationDirection = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
	
	aiPosition = aiPosition + (destinationDirection);

	aiCurrCharDirection = (aiOldCharDirection) + (destinationDirection );

	float charDirAngle = XMVectorGetX(XMVector3AngleBetweenNormals( XMVector3Normalize(aiCurrCharDirection), XMVector3Normalize(DefaultForward)));
	if(XMVectorGetY(XMVector3Cross(aiCurrCharDirection, DefaultForward)) > 0.0f)
		charDirAngle = -charDirAngle;

	XMMATRIX rotationMatrix;
	Translation = XMMatrixTranslation(XMVectorGetX(aiPosition), 0.0f, XMVectorGetZ(aiPosition) );
	rotationMatrix = XMMatrixRotationY(charDirAngle - 3.14159265f);
	Scale = XMMatrixScaling( 2.0f, 2.0f, 2.0f );	

	worldMatrix = Scale * rotationMatrix * Translation;

	aiOldCharDirection = aiCurrCharDirection;
}

void CleanUp()
{
	_pSwapChain->SetFullscreenState(false, NULL);
	PostMessage(hwnd, WM_DESTROY, 0, 0);

	//Release the objects created
	_pSwapChain->Release();
	_pd3dDevice->Release();
	_pImmediateContext->Release();
	_pRenderTargetView->Release();
	VS->Release();
	PS->Release();
	VS_Buffer->Release();
	PS_Buffer->Release();
	vertLayout->Release();
	_depthStencilView->Release();
	_depthStencilBuffer->Release();
	cbPerObjectBuffer->Release();
	Transparency->Release();
	CCWcullMode->Release();
	CWcullMode->Release();

	d3d101Device->Release();
	D2DRenderTarget->Release();	
	Brush->Release();
	BackBuffer11->Release();
	sharedTex11->Release();
	DWriteFactory->Release();
	TextFormat->Release();
	d2dTexture->Release();

	cbPerFrameBuffer->Release();

	DIKeyboard->Unacquire();
	DIMouse->Unacquire();
	DirectInput->Release();

	sphereIndexBuffer->Release();
	sphereVertBuffer->Release();

	SKYMAP_VS->Release();
	SKYMAP_PS->Release();
	SKYMAP_VS_Buffer->Release();
	SKYMAP_PS_Buffer->Release();

	smrv->Release();

	DSLessEqual->Release();
	RSCullNone->Release();

	meshVertBuff->Release();
	meshIndexBuff->Release();

}

bool LoadObjModel(std::wstring filename, 
	ID3D11Buffer** vertBuff, 
	ID3D11Buffer** indexBuff,
	std::vector<int>& subsetIndexStart,
	std::vector<int>& subsetMaterialArray,
	std::vector<SurfaceMaterial>& material, 
	int& subsetCount,
	bool isRHCoordSys,
	bool computeNormals,
	std::vector<XMFLOAT3> &AABB)
{
	HRESULT hr = 0;

	std::wifstream fileIn (filename.c_str());	//Open file
	std::wstring meshMatLib;					

	//Arrays to store our model's information
	std::vector<DWORD> indices;
	std::vector<XMFLOAT3> vertPos;
	std::vector<XMFLOAT3> vertNorm;
	std::vector<XMFLOAT2> vertTexCoord;
	std::vector<std::wstring> meshMaterials;

	//Vertex definition indices
	std::vector<int> vertPosIndex;
	std::vector<int> vertNormIndex;
	std::vector<int> vertTCIndex;

	//Make sure we have a default if no tex coords or normals are defined
	bool hasTexCoord = false;
	bool hasNorm = false;

	//Temp variables to store into vectors
	std::wstring meshMaterialsTemp;
	int vertPosIndexTemp;
	int vertNormIndexTemp;
	int vertTCIndexTemp;

	wchar_t checkChar;		
	std::wstring face;		
	int vIndex = 0;			
	int triangleCount = 0;	
	int totalVerts = 0;
	int meshTriangles = 0;

	//Check to see if the file was opened
	if (fileIn)
	{
		while(fileIn)
		{			
			checkChar = fileIn.get();	//Get next char

			switch (checkChar)
			{		
			case '#':
				checkChar = fileIn.get();
				while(checkChar != '\n')
					checkChar = fileIn.get();
				break;
			case 'v':	//Get Vertex Descriptions
				checkChar = fileIn.get();
				if(checkChar == ' ')	
				{
					float vz, vy, vx;
					fileIn >> vx >> vy >> vz;	//Store the next three types

					if(isRHCoordSys)	//If model is from an RH Coord System
						vertPos.push_back(XMFLOAT3( vx, vy, vz * -1.0f));	//Invert the Z axis
					else
						vertPos.push_back(XMFLOAT3( vx, vy, vz));
				}
				if(checkChar == 't')	//vt - vert tex coords
				{			
					float vtcu, vtcv;
					fileIn >> vtcu >> vtcv;		

					if(isRHCoordSys)	//If model is from an RH Coord System
						vertTexCoord.push_back(XMFLOAT2(vtcu, 1.0f-vtcv));	//Reverse the "v" axis
					else
						vertTexCoord.push_back(XMFLOAT2(vtcu, vtcv));	

					hasTexCoord = true;	//We know the model uses texture coords
				}
				//Since we compute the normals later, we don't need to check for normals
				if(checkChar == 'n')	//vn - vert normal
				{
					float vnx, vny, vnz;
					fileIn >> vnx >> vny >> vnz;	//Store next three types

					if(isRHCoordSys)	//If model is from an RH Coord System
						vertNorm.push_back(XMFLOAT3( vnx, vny, vnz * -1.0f ));	//Invert the Z axis
					else
						vertNorm.push_back(XMFLOAT3( vnx, vny, vnz ));	

					hasNorm = true;	//We know the model defines normals
				}
				break;

				//New group (Subset)
			case 'g':	//g - defines a group
				checkChar = fileIn.get();
				if(checkChar == ' ')
				{
					subsetIndexStart.push_back(vIndex);		//Start index for this subset
					subsetCount++;
				}
				break;

				//Get Face Index
			case 'f':	//f - defines the faces
				checkChar = fileIn.get();
				if(checkChar == ' ')
				{
					face = L"";
					std::wstring VertDef;	//Holds one vertex definition at a time
					triangleCount = 0;

					checkChar = fileIn.get();
					while(checkChar != '\n')
					{
						face += checkChar;			//Add the char to our face string
						checkChar = fileIn.get();	//Get the next Character
						if(checkChar == ' ')		
							triangleCount++;		
					}

					//Check for space at the end of our face string
					if(face[face.length()-1] == ' ')
						triangleCount--;	//Each space adds to our triangle count

					triangleCount -= 1;		

					std::wstringstream ss(face);

					if(face.length() > 0)
					{
						int firstVIndex, lastVIndex;	//Holds the first and last vertice's index

						for(int i = 0; i < 3; ++i)		//First three vertices (first triangle)
						{
							ss >> VertDef;	

							std::wstring vertPart;
							int whichPart = 0;		

							//Parse this string
							for(int j = 0; j < VertDef.length(); ++j)
							{
								if(VertDef[j] != '/')	//If there is no divider "/", add a char to our vertPart
									vertPart += VertDef[j];

								//If the current char is a divider "/", or its the last character in the string
								if(VertDef[j] == '/' || j ==  VertDef.length()-1)
								{
									std::wistringstream wstringToInt(vertPart);	//Used to convert wstring to int

									if(whichPart == 0)	//If vPos
									{
										wstringToInt >> vertPosIndexTemp;
										vertPosIndexTemp -= 1;		//subtract one since c++ arrays start with 0, and obj start with 1

										//Check to see if the vert pos was the only thing specified
										if(j == VertDef.length()-1)
										{
											vertNormIndexTemp = 0;
											vertTCIndexTemp = 0;
										}
									}

									else if(whichPart == 1)	//If vTexCoord
									{
										if(vertPart != L"")	//Check to see if there even is a tex coord
										{
											wstringToInt >> vertTCIndexTemp;
											vertTCIndexTemp -= 1;	//subtract one since c++ arrays start with 0, and obj start with 1
										}
										else	//If there is no tex coord, make a default
											vertTCIndexTemp = 0;

										if(j == VertDef.length()-1)
											vertNormIndexTemp = 0;

									}								
									else if(whichPart == 2)	//If vNorm
									{
										std::wistringstream wstringToInt(vertPart);

										wstringToInt >> vertNormIndexTemp;
										vertNormIndexTemp -= 1;		//subtract one since c++ arrays start with 0, and obj start with 1
									}

									vertPart = L"";	//Get ready for next vertex part
									whichPart++;	//Move on to next vertex part					
								}
							}

							//Check to make sure there is at least one subset
							if(subsetCount == 0)
							{
								subsetIndexStart.push_back(vIndex);		//Start index for this subset
								subsetCount++;
							}

							//Avoid duplicate vertices
							bool vertAlreadyExists = false;
							if(totalVerts >= 3)	//Make sure we at least have one triangle to check
							{
								//Loop through all the vertices
								for(int iCheck = 0; iCheck < totalVerts; ++iCheck)
								{
									if(vertPosIndexTemp == vertPosIndex[iCheck] && !vertAlreadyExists)
									{
										if(vertTCIndexTemp == vertTCIndex[iCheck])
										{
											indices.push_back(iCheck);		//Set index for this vertex
											vertAlreadyExists = true;		
										}
									}
								}
							}

							//If this vertex is not already in our vertex arrays, put it there
							if(!vertAlreadyExists)
							{
								vertPosIndex.push_back(vertPosIndexTemp);
								vertTCIndex.push_back(vertTCIndexTemp);
								vertNormIndex.push_back(vertNormIndexTemp);
								totalVerts++;	//We created a new vertex
								indices.push_back(totalVerts-1);	//Set index for this vertex
							}							

							if(i == 0)
							{
								firstVIndex = indices[vIndex];	

							}

							if(i == 2)
							{								
								lastVIndex = indices[vIndex];	//The last vertex index of this TRIANGLE
							}
							vIndex++;	//Increment index count
						}

						meshTriangles++;	//One triangle down

						for(int l = 0; l < triangleCount-1; ++l)	//Loop through the next vertices to create new triangles
						{
							//First vertex of this triangle (the very first vertex of the face too)
							indices.push_back(firstVIndex);			//Set index for this vertex
							vIndex++;

							//Second Vertex of this triangle (the last vertex used in the tri before this one)
							indices.push_back(lastVIndex);			//Set index for this vertex
							vIndex++;

							//Get the third vertex for this triangle
							ss >> VertDef;

							std::wstring vertPart;
							int whichPart = 0;

							//Parse this string (same as above)
							for(int j = 0; j < VertDef.length(); ++j)
							{
								if(VertDef[j] != '/')
									vertPart += VertDef[j];
								if(VertDef[j] == '/' || j ==  VertDef.length()-1)
								{
									std::wistringstream wstringToInt(vertPart);

									if(whichPart == 0)
									{
										wstringToInt >> vertPosIndexTemp;
										vertPosIndexTemp -= 1;

										//Check to see if the vert pos was the only thing specified
										if(j == VertDef.length()-1)
										{
											vertTCIndexTemp = 0;
											vertNormIndexTemp = 0;
										}
									}
									else if(whichPart == 1)
									{
										if(vertPart != L"")
										{
											wstringToInt >> vertTCIndexTemp;
											vertTCIndexTemp -= 1;
										}
										else
											vertTCIndexTemp = 0;
										if(j == VertDef.length()-1)
											vertNormIndexTemp = 0;

									}								
									else if(whichPart == 2)
									{
										std::wistringstream wstringToInt(vertPart);

										wstringToInt >> vertNormIndexTemp;
										vertNormIndexTemp -= 1;
									}

									vertPart = L"";
									whichPart++;							
								}
							}					

							//Check for duplicate vertices
							bool vertAlreadyExists = false;
							if(totalVerts >= 3)	//Make sure we at least have one triangle to check
							{
								for(int iCheck = 0; iCheck < totalVerts; ++iCheck)
								{
									if(vertPosIndexTemp == vertPosIndex[iCheck] && !vertAlreadyExists)
									{
										if(vertTCIndexTemp == vertTCIndex[iCheck])
										{
											indices.push_back(iCheck);			//Set index for this vertex
											vertAlreadyExists = true;		//If we've made it here, the vertex already exists
										}
									}
								}
							}

							if(!vertAlreadyExists)
							{
								vertPosIndex.push_back(vertPosIndexTemp);
								vertTCIndex.push_back(vertTCIndexTemp);
								vertNormIndex.push_back(vertNormIndexTemp);
								totalVerts++;					//New vertex created, add to total verts
								indices.push_back(totalVerts-1);		//Set index for this vertex
							}

							//Set the second vertex for the next triangle to the last vertex we got		
							lastVIndex = indices[vIndex];	//The last vertex index of this TRIANGLE

							meshTriangles++;	//New triangle defined
							vIndex++;		
						}
					}
				}
				break;

			case 'm':	//mtllib - material library filename
				checkChar = fileIn.get();
				if(checkChar == 't')
				{
					checkChar = fileIn.get();
					if(checkChar == 'l')
					{
						checkChar = fileIn.get();
						if(checkChar == 'l')
						{
							checkChar = fileIn.get();
							if(checkChar == 'i')
							{
								checkChar = fileIn.get();
								if(checkChar == 'b')
								{
									checkChar = fileIn.get();
									if(checkChar == ' ')
									{
										//Store the material libraries file name
										fileIn >> meshMatLib;
									}
								}
							}
						}
					}
				}

				break;

			case 'u':	//usemtl - which material to use
				checkChar = fileIn.get();
				if(checkChar == 's')
				{
					checkChar = fileIn.get();
					if(checkChar == 'e')
					{
						checkChar = fileIn.get();
						if(checkChar == 'm')
						{
							checkChar = fileIn.get();
							if(checkChar == 't')
							{
								checkChar = fileIn.get();
								if(checkChar == 'l')
								{
									checkChar = fileIn.get();
									if(checkChar == ' ')
									{
										meshMaterialsTemp = L"";	//Make sure this is cleared

										fileIn >> meshMaterialsTemp; //Get next type (string)

										meshMaterials.push_back(meshMaterialsTemp);
									}
								}
							}
						}
					}
				}
				break;

			default:				
				break;
			}
		}
	}
	else	//If we could not open the file
	{
		_pSwapChain->SetFullscreenState(false, NULL);	

		//create message
		std::wstring message = L"Could not open: ";
		message += filename;

		MessageBox(0, message.c_str(),
			L"Error", MB_OK);

		return false;
	}

	subsetIndexStart.push_back(vIndex); 

	if(subsetIndexStart[1] == 0)
	{
		subsetIndexStart.erase(subsetIndexStart.begin()+1);
		meshSubsets--;
	}

	//Make sure we have a default for the tex coord and normal if one or both are not specified
	if(!hasNorm)
		vertNorm.push_back(XMFLOAT3(0.0f, 0.0f, 0.0f));
	if(!hasTexCoord)
		vertTexCoord.push_back(XMFLOAT2(0.0f, 0.0f));

	//Close the obj file, and open the mtl file
	fileIn.close();
	fileIn.open(meshMatLib.c_str());

	std::wstring lastStringRead;
	int matCount = material.size();	//total materials

	//kdset - If our diffuse color was not set, we can use the ambient color (which is usually the same)
	//If the diffuse color WAS set, then we don't need to set our diffuse color to ambient
	bool kdset = false;

	if (fileIn)
	{
		while(fileIn)
		{
			checkChar = fileIn.get();	//Get next char

			switch (checkChar)
			{
				//Check for comment
			case '#':
				checkChar = fileIn.get();
				while(checkChar != '\n')
					checkChar = fileIn.get();
				break;

				//Set diffuse color
			case 'K':
				checkChar = fileIn.get();
				if(checkChar == 'd')	//Diffuse Color
				{
					checkChar = fileIn.get();	//remove space

					fileIn >> material[matCount-1].difColor.x;
					fileIn >> material[matCount-1].difColor.y;
					fileIn >> material[matCount-1].difColor.z;

					kdset = true;
				}

				//Ambient Color (We'll store it in diffuse if there isn't a diffuse already)
				if(checkChar == 'a')	
				{					
					checkChar = fileIn.get();	//remove space
					if(!kdset)
					{
						fileIn >> material[matCount-1].difColor.x;
						fileIn >> material[matCount-1].difColor.y;
						fileIn >> material[matCount-1].difColor.z;
					}
				}
				break;

				//Check for transparency
			case 'T':
				checkChar = fileIn.get();
				if(checkChar == 'r')
				{
					checkChar = fileIn.get();	//remove space
					float Transparency;
					fileIn >> Transparency;

					material[matCount-1].difColor.w = Transparency;

					if(Transparency > 0.0f)
						material[matCount-1].transparent = true;
				}
				break;

				//Some obj files specify d for transparency
			case 'd':
				checkChar = fileIn.get();
				if(checkChar == ' ')
				{
					float Transparency;
					fileIn >> Transparency;

					//'d' - 0 being most transparent, and 1 being opaque, opposite of Tr
					Transparency = 1.0f - Transparency;

					material[matCount-1].difColor.w = Transparency;

					if(Transparency > 0.0f)
						material[matCount-1].transparent = true;					
				}
				break;

				//Get the diffuse map (texture)
			case 'm':
				checkChar = fileIn.get();
				if(checkChar == 'a')
				{
					checkChar = fileIn.get();
					if(checkChar == 'p')
					{
						checkChar = fileIn.get();
						if(checkChar == '_')
						{
							//map_Kd - Diffuse map
							checkChar = fileIn.get();
							if(checkChar == 'K')
							{
								checkChar = fileIn.get();
								if(checkChar == 'd')
								{
									std::wstring fileNamePath;

									fileIn.get();	//Remove whitespace between map_Kd and file

									//Get the file path 
									bool texFilePathEnd = false;
									while(!texFilePathEnd)
									{
										checkChar = fileIn.get();

										fileNamePath += checkChar;

										if(checkChar == '.')
										{
											for(int i = 0; i < 3; ++i)
												fileNamePath += fileIn.get();

											texFilePathEnd = true;
										}							
									}

									//check if this texture has already been loaded
									bool alreadyLoaded = false;
									for(int i = 0; i < textureNameArray.size(); ++i)
									{
										if(fileNamePath == textureNameArray[i])
										{
											alreadyLoaded = true;
											material[matCount-1].texArrayIndex = i;
											material[matCount-1].hasTexture = true;
										}
									}

									//if the texture is not already loaded, load it now
									if(!alreadyLoaded)
									{
										ID3D11ShaderResourceView* tempMeshSRV;
										hr = D3DX11CreateShaderResourceViewFromFile( _pd3dDevice, fileNamePath.c_str(),
											NULL, NULL, &tempMeshSRV, NULL );
										if(SUCCEEDED(hr))
										{
											textureNameArray.push_back(fileNamePath.c_str());
											material[matCount-1].texArrayIndex = meshSRV.size();
											meshSRV.push_back(tempMeshSRV);
											material[matCount-1].hasTexture = true;
										}
									}	
								}
							}
							//map_d - alpha map
							else if(checkChar == 'd')
							{
								material[matCount-1].transparent = true;
							}

							//map_bump - bump map (we're usinga normal map though)
							else if(checkChar == 'b')
							{
								checkChar = fileIn.get();
								if(checkChar == 'u')
								{
									checkChar = fileIn.get();
									if(checkChar == 'm')
									{
										checkChar = fileIn.get();
										if(checkChar == 'p')
										{
											std::wstring fileNamePath;

											fileIn.get();	//Remove whitespace between map_bump and file

											bool texFilePathEnd = false;
											while(!texFilePathEnd)
											{
												checkChar = fileIn.get();

												fileNamePath += checkChar;

												if(checkChar == '.')
												{
													for(int i = 0; i < 3; ++i)
														fileNamePath += fileIn.get();

													texFilePathEnd = true;
												}							
											}

											//check if this texture has already been loaded
											bool alreadyLoaded = false;
											for(int i = 0; i < textureNameArray.size(); ++i)
											{
												if(fileNamePath == textureNameArray[i])
												{
													alreadyLoaded = true;
													material[matCount-1].normMapTexArrayIndex = i;
													material[matCount-1].hasNormMap = true;
												}
											}

											//if the texture is not already loaded, load it now
											if(!alreadyLoaded)
											{
												ID3D11ShaderResourceView* tempMeshSRV;
												hr = D3DX11CreateShaderResourceViewFromFile( _pd3dDevice, fileNamePath.c_str(),
													NULL, NULL, &tempMeshSRV, NULL );
												if(SUCCEEDED(hr))
												{
													textureNameArray.push_back(fileNamePath.c_str());
													material[matCount-1].normMapTexArrayIndex = meshSRV.size();
													meshSRV.push_back(tempMeshSRV);
													material[matCount-1].hasNormMap = true;
												}
											}	
										}
									}
								}
							}
						}
					}
				}
				break;

			case 'n':	//newmtl - Declare new material
				checkChar = fileIn.get();
				if(checkChar == 'e')
				{
					checkChar = fileIn.get();
					if(checkChar == 'w')
					{
						checkChar = fileIn.get();
						if(checkChar == 'm')
						{
							checkChar = fileIn.get();
							if(checkChar == 't')
							{
								checkChar = fileIn.get();
								if(checkChar == 'l')
								{
									checkChar = fileIn.get();
									if(checkChar == ' ')
									{
										//New material, set its defaults
										SurfaceMaterial tempMat;
										material.push_back(tempMat);
										fileIn >> material[matCount].matName;
										material[matCount].transparent = false;
										material[matCount].hasTexture = false;
										material[matCount].hasNormMap = false;
										material[matCount].normMapTexArrayIndex = 0;
										material[matCount].texArrayIndex = 0;
										matCount++;
										kdset = false;
									}
								}
							}
						}
					}
				}
				break;

			default:
				break;
			}
		}
	}	
	else
	{
		_pSwapChain->SetFullscreenState(false, NULL);	//Make sure we are out of fullscreen

		std::wstring message = L"Could not open: ";
		message += meshMatLib;

		MessageBox(0, message.c_str(),
			L"Error", MB_OK);

		return false;
	}

	//Set the subsets material to the index value
	//of the its material in our material array
	for(int i = 0; i < meshSubsets; ++i)
	{
		bool hasMat = false;
		for(int j = 0; j < material.size(); ++j)
		{
			if(meshMaterials[i] == material[j].matName)
			{
				subsetMaterialArray.push_back(j);
				hasMat = true;
			}
		}
		if(!hasMat)
			subsetMaterialArray.push_back(0); //Use first material in array
	}

	std::vector<Vertex> vertices;
	Vertex tempVert;

	//Create our vertices using the information we got 
	//from the file and store them in a vector
	for(int j = 0 ; j < totalVerts; ++j)
	{
		tempVert.pos = vertPos[vertPosIndex[j]];
		tempVert.normal = vertNorm[vertNormIndex[j]];
		tempVert.texCoord = vertTexCoord[vertTCIndex[j]];

		vertices.push_back(tempVert);
	}

	//Compute normals
	if(computeNormals)
	{
		std::vector<XMFLOAT3> tempNormal;

		//normalized and unnormalized normals
		XMFLOAT3 unnormalized = XMFLOAT3(0.0f, 0.0f, 0.0f);

		//tangent stuff
		std::vector<XMFLOAT3> tempTangent;
		XMFLOAT3 tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float tcU1, tcV1, tcU2, tcV2;

		//Used to get vectors (sides) from the position of the verts
		float vecX, vecY, vecZ;

		//Two edges of our triangle
		XMVECTOR edge1 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR edge2 = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

		//Compute face normals
		//And Tangents
		for(int i = 0; i < meshTriangles; ++i)
		{
			//Get the vector describing one edge of our triangle (edge 0,2)
			vecX = vertices[indices[(i*3)]].pos.x - vertices[indices[(i*3)+2]].pos.x;
			vecY = vertices[indices[(i*3)]].pos.y - vertices[indices[(i*3)+2]].pos.y;
			vecZ = vertices[indices[(i*3)]].pos.z - vertices[indices[(i*3)+2]].pos.z;		
			edge1 = XMVectorSet(vecX, vecY, vecZ, 0.0f);	//Create our first edge

			//Get the vector describing another edge of our triangle (edge 2,1)
			vecX = vertices[indices[(i*3)+2]].pos.x - vertices[indices[(i*3)+1]].pos.x;
			vecY = vertices[indices[(i*3)+2]].pos.y - vertices[indices[(i*3)+1]].pos.y;
			vecZ = vertices[indices[(i*3)+2]].pos.z - vertices[indices[(i*3)+1]].pos.z;		
			edge2 = XMVectorSet(vecX, vecY, vecZ, 0.0f);	//Create our second edge

			//Cross multiply the two edge vectors to get the un-normalized face normal
			XMStoreFloat3(&unnormalized, XMVector3Cross(edge1, edge2));

			tempNormal.push_back(unnormalized);

			//Find first texture coordinate edge 2d vector
			tcU1 = vertices[indices[(i*3)]].texCoord.x - vertices[indices[(i*3)+2]].texCoord.x;
			tcV1 = vertices[indices[(i*3)]].texCoord.y - vertices[indices[(i*3)+2]].texCoord.y;

			//Find second texture coordinate edge 2d vector
			tcU2 = vertices[indices[(i*3)+2]].texCoord.x - vertices[indices[(i*3)+1]].texCoord.x;
			tcV2 = vertices[indices[(i*3)+2]].texCoord.y - vertices[indices[(i*3)+1]].texCoord.y;

			//Find tangent using both tex coord edges and position edges
			tangent.x = (tcV1 * XMVectorGetX(edge1) - tcV2 * XMVectorGetX(edge2)) * (1.0f / (tcU1 * tcV2 - tcU2 * tcV1));
			tangent.y = (tcV1 * XMVectorGetY(edge1) - tcV2 * XMVectorGetY(edge2)) * (1.0f / (tcU1 * tcV2 - tcU2 * tcV1));
			tangent.z = (tcV1 * XMVectorGetZ(edge1) - tcV2 * XMVectorGetZ(edge2)) * (1.0f / (tcU1 * tcV2 - tcU2 * tcV1));

			tempTangent.push_back(tangent);
		}

		//Compute vertex normals (normal Averaging)
		XMVECTOR normalSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		XMVECTOR tangentSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		int facesUsing = 0;
		float tX, tY, tZ;	//temp axis variables

		//Go through each vertex
		for(int i = 0; i < totalVerts; ++i)
		{
			//Check which triangles use this vertex
			for(int j = 0; j < meshTriangles; ++j)
			{
				if(indices[j*3] == i ||
					indices[(j*3)+1] == i ||
					indices[(j*3)+2] == i)
				{
					tX = XMVectorGetX(normalSum) + tempNormal[j].x;
					tY = XMVectorGetY(normalSum) + tempNormal[j].y;
					tZ = XMVectorGetZ(normalSum) + tempNormal[j].z;

					normalSum = XMVectorSet(tX, tY, tZ, 0.0f);	//If a face is using the vertex, add the unormalized face normal to the normalSum

					//We can reuse tX, tY, tZ to sum up tangents
					tX = XMVectorGetX(tangentSum) + tempTangent[j].x;
					tY = XMVectorGetY(tangentSum) + tempTangent[j].y;
					tZ = XMVectorGetZ(tangentSum) + tempTangent[j].z;

					tangentSum = XMVectorSet(tX, tY, tZ, 0.0f); //sum up face tangents using this vertex

					facesUsing++;
				}
			}

			//Get the actual normal by dividing the normalSum by the number of faces sharing the vertex
			normalSum = normalSum / facesUsing;
			tangentSum = tangentSum / facesUsing;

			//Normalize the normalSum vector and tangent
			normalSum = XMVector3Normalize(normalSum);
			tangentSum =  XMVector3Normalize(tangentSum);

			//Store the normal and tangent in our current vertex
			vertices[i].normal.x = XMVectorGetX(normalSum);
			vertices[i].normal.y = XMVectorGetY(normalSum);
			vertices[i].normal.z = XMVectorGetZ(normalSum);

			vertices[i].tangent.x = XMVectorGetX(tangentSum);
			vertices[i].tangent.y = XMVectorGetY(tangentSum);
			vertices[i].tangent.z = XMVectorGetZ(tangentSum);

			//Clear normalSum, tangentSum and facesUsing for next vertex
			normalSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			tangentSum = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			facesUsing = 0;

		}
	}

	//Create index buffer
	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory( &indexBufferDesc, sizeof(indexBufferDesc) );

	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(DWORD) * meshTriangles*3;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;

	iinitData.pSysMem = &indices[0];
	_pd3dDevice->CreateBuffer(&indexBufferDesc, &iinitData, indexBuff);

	//Create Vertex Buffer
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory( &vertexBufferDesc, sizeof(vertexBufferDesc) );


	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof( Vertex ) * totalVerts;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData; 

	ZeroMemory( &vertexBufferData, sizeof(vertexBufferData) );
	vertexBufferData.pSysMem = &vertices[0];
	hr = _pd3dDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, vertBuff);


	// Compute AABB and true center
	AABB = CreateAABB(vertPos);

	return true;
}

void CreateSphere(int LatLines, int LongLines)
{
	NumSphereVertices = ((LatLines-2) * LongLines) + 2;
	NumSphereFaces  = ((LatLines-3)*(LongLines)*2) + (LongLines*2);

	float sphereYaw = 0.0f;
	float spherePitch = 0.0f;

	std::vector<Vertex> vertices(NumSphereVertices);

	XMVECTOR currVertPos = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	vertices[0].pos.x = 0.0f;
	vertices[0].pos.y = 0.0f;
	vertices[0].pos.z = 1.0f;

	for(DWORD i = 0; i < LatLines-2; ++i)
	{
		spherePitch = (i+1) * (3.14f/(LatLines-1));
		Rotationx = XMMatrixRotationX(spherePitch);
		for(DWORD j = 0; j < LongLines; ++j)
		{
			sphereYaw = j * (6.28f/(LongLines));
			Rotationy = XMMatrixRotationZ(sphereYaw);
			currVertPos = XMVector3TransformNormal( XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), (Rotationx * Rotationy) );	
			currVertPos = XMVector3Normalize( currVertPos );
			vertices[i*LongLines+j+1].pos.x = XMVectorGetX(currVertPos);
			vertices[i*LongLines+j+1].pos.y = XMVectorGetY(currVertPos);
			vertices[i*LongLines+j+1].pos.z = XMVectorGetZ(currVertPos);
		}
	}

	vertices[NumSphereVertices-1].pos.x =  0.0f;
	vertices[NumSphereVertices-1].pos.y =  0.0f;
	vertices[NumSphereVertices-1].pos.z = -1.0f;


	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory( &vertexBufferDesc, sizeof(vertexBufferDesc) );

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof( Vertex ) * NumSphereVertices;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData; 

	ZeroMemory( &vertexBufferData, sizeof(vertexBufferData) );
	vertexBufferData.pSysMem = &vertices[0];
	hr = _pd3dDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, &sphereVertBuffer);


	std::vector<DWORD> indices(NumSphereFaces * 3);

	int k = 0;
	for(DWORD l = 0; l < LongLines-1; ++l)
	{
		indices[k] = 0;
		indices[k+1] = l+1;
		indices[k+2] = l+2;
		k += 3;
	}

	indices[k] = 0;
	indices[k+1] = LongLines;
	indices[k+2] = 1;
	k += 3;

	for(DWORD i = 0; i < LatLines-3; ++i)
	{
		for(DWORD j = 0; j < LongLines-1; ++j)
		{
			indices[k]   = i*LongLines+j+1;
			indices[k+1] = i*LongLines+j+2;
			indices[k+2] = (i+1)*LongLines+j+1;

			indices[k+3] = (i+1)*LongLines+j+1;
			indices[k+4] = i*LongLines+j+2;
			indices[k+5] = (i+1)*LongLines+j+2;

			k += 6; //next quad
		}

		indices[k]   = (i*LongLines)+LongLines;
		indices[k+1] = (i*LongLines)+1;
		indices[k+2] = ((i+1)*LongLines)+LongLines;

		indices[k+3] = ((i+1)*LongLines)+LongLines;
		indices[k+4] = (i*LongLines)+1;
		indices[k+5] = ((i+1)*LongLines)+1;

		k += 6;
	}

	for(DWORD l = 0; l < LongLines-1; ++l)
	{
		indices[k] = NumSphereVertices-1;
		indices[k+1] = (NumSphereVertices-1)-(l+1);
		indices[k+2] = (NumSphereVertices-1)-(l+2);
		k += 3;
	}

	indices[k] = NumSphereVertices-1;
	indices[k+1] = (NumSphereVertices-1)-LongLines;
	indices[k+2] = NumSphereVertices-2;

	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory( &indexBufferDesc, sizeof(indexBufferDesc) );

	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(DWORD) * NumSphereFaces * 3;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;

	iinitData.pSysMem = &indices[0];
	_pd3dDevice->CreateBuffer(&indexBufferDesc, &iinitData, &sphereIndexBuffer);

}

void InitD2DScreenTexture()
{
	//Create the vertex buffer
	Vertex v[] =
	{
		// Front Face
		Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 1.0f,-1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(-1.0f,  1.0f, -1.0f, 0.0f, 0.0f,-1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 0.0f),
		Vertex( 1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 0.0f),
		Vertex( 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f),
	};

	DWORD indices[] = {
		// Front Face
		0,  1,  2,
		0,  2,  3,
	};

	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory( &indexBufferDesc, sizeof(indexBufferDesc) );

	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(DWORD) * 2 * 3;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA iinitData;

	iinitData.pSysMem = indices;
	_pd3dDevice->CreateBuffer(&indexBufferDesc, &iinitData, &d2dIndexBuffer);


	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory( &vertexBufferDesc, sizeof(vertexBufferDesc) );

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof( Vertex ) * 4;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData; 

	ZeroMemory( &vertexBufferData, sizeof(vertexBufferData) );
	vertexBufferData.pSysMem = v;
	hr = _pd3dDevice->CreateBuffer( &vertexBufferDesc, &vertexBufferData, &d2dVertBuffer);

	//Create a shader resource view from the texture D2D will render to,
	//So we can use it to texture a square which overlays the scene
	_pd3dDevice->CreateShaderResourceView(sharedTex11, NULL, &d2dTexture);
}

void StartTimer()
{
	LARGE_INTEGER frequencyCount;
	QueryPerformanceFrequency(&frequencyCount);

	countsPerSecond = double(frequencyCount.QuadPart);

	QueryPerformanceCounter(&frequencyCount);
	CounterStart = frequencyCount.QuadPart;
}

double GetTime()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	return double(currentTime.QuadPart-CounterStart)/countsPerSecond;
}

double GetFrameTime()
{
	LARGE_INTEGER currentTime;
	__int64 tickCount;
	QueryPerformanceCounter(&currentTime);

	tickCount = currentTime.QuadPart-frameTimeOld;
	frameTimeOld = currentTime.QuadPart;

	if(tickCount < 0.0f)
		tickCount = 0.0f;

	return float(tickCount)/countsPerSecond;
}

void UpdateScene(double time)
{
	//Reset sphereWorld
	sphereWorld = XMMatrixIdentity();

	//Define sphereWorld's world space matrix
	Scale = XMMatrixScaling( 5.0f, 5.0f, 5.0f );
	//Make sure the sphere is always centered around camera
	Translation = XMMatrixTranslation( XMVectorGetX(camPosition), XMVectorGetY(camPosition), XMVectorGetZ(camPosition) );

	//Set sphereWorld's world space using the transformations
	sphereWorld = Scale * Translation;

	//The loaded models world space
	meshWorld = XMMatrixIdentity();

	Rotation = XMMatrixRotationY(3.14f);
	Scale = XMMatrixScaling( 10.0f, 1.0f, 10.0f );
	Translation = XMMatrixTranslation( 0.0f, 0.0f, 0.0f );

	meshWorld = Rotation * Scale * Translation;
}

LRESULT CALLBACK WndProc(HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch( msg )
	{
	case WM_KEYDOWN:
		if( wParam == VK_ESCAPE ){
			DestroyWindow(hwnd);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,
		msg,
		wParam,
		lParam);
}

bool InitializeWindow(HINSTANCE hInstance,
	int ShowWnd,
	int width, int height,
	bool windowed)
{
	typedef struct _WNDCLASS {
		UINT cbSize;
		UINT style;
		WNDPROC lpfnWndProc;
		int cbClsExtra;
		int cbWndExtra;
		HANDLE hInstance;
		HICON hIcon;
		HCURSOR hCursor;
		HBRUSH hbrBackground;
		LPCTSTR lpszMenuName;
		LPCTSTR lpszClassName;
	} WNDCLASS;

	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WndClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Error registering class",	
			L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	hwnd = CreateWindowEx(
		NULL,
		WndClassName,
		L"Racing Game",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		NULL,
		NULL,
		hInstance,
		NULL
		);

	if (!hwnd)
	{
		MessageBox(NULL, L"Error creating window",
			L"Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	ShowWindow(hwnd, ShowWnd);
	UpdateWindow(hwnd);

	return true;
}

bool InitializeDirect3d11App(HINSTANCE hInstance)
{	
	//Describe our _pSwapChain Buffer
	DXGI_MODE_DESC bufferDesc;

	ZeroMemory(&bufferDesc, sizeof(DXGI_MODE_DESC));

	bufferDesc.Width = Width;
	bufferDesc.Height = Height;
	bufferDesc.RefreshRate.Numerator = 60;
	bufferDesc.RefreshRate.Denominator = 1;
	bufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	//Describe our _pSwapChain
	DXGI_SWAP_CHAIN_DESC _pSwapChainDesc; 

	ZeroMemory(&_pSwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

	_pSwapChainDesc.BufferDesc = bufferDesc;
	_pSwapChainDesc.SampleDesc.Count = 1;
	_pSwapChainDesc.SampleDesc.Quality = 0;
	_pSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	_pSwapChainDesc.BufferCount = 1;
	_pSwapChainDesc.OutputWindow = hwnd; 
	_pSwapChainDesc.Windowed = true; 
	_pSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// Create DXGI factory to enumerate adapters
	IDXGIFactory1 *DXGIFactory;

	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&DXGIFactory);	

	// Use the first adapter	
	IDXGIAdapter1 *Adapter;

	hr = DXGIFactory->EnumAdapters1(0, &Adapter);

	DXGIFactory->Release();	

	//Create our Direct3D 11 Device and _pSwapChain
	hr = D3D11CreateDeviceAndSwapChain(Adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		NULL, NULL,	D3D11_SDK_VERSION, &_pSwapChainDesc, &_pSwapChain, &_pd3dDevice, NULL, &_pImmediateContext);

	//Initialize Direct2D, Direct3D 10.1, DirectWrite
	//InitD2D_D3D101_DWrite(Adapter);

	//Release the Adapter interface
	Adapter->Release();

	//Create our BackBuffer and Render Target
	hr = _pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&BackBuffer11 );
	hr = _pd3dDevice->CreateRenderTargetView( BackBuffer11, NULL, &_pRenderTargetView );

	//Describe our Depth/Stencil Buffer
	D3D11_TEXTURE2D_DESC depthStencilDesc;

	depthStencilDesc.Width     = Width;
	depthStencilDesc.Height    = Height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count   = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage          = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags      = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0; 
	depthStencilDesc.MiscFlags      = 0;

	//Create the Depth/Stencil View
	_pd3dDevice->CreateTexture2D(&depthStencilDesc, NULL, &_depthStencilBuffer);
	_pd3dDevice->CreateDepthStencilView(_depthStencilBuffer, NULL, &_depthStencilView);

	return true;
}

bool InitDirectInput(HINSTANCE hInstance)
{
	hr = DirectInput8Create(hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&DirectInput,
		NULL); 

	hr = DirectInput->CreateDevice(GUID_SysKeyboard,
		&DIKeyboard,
		NULL);

	hr = DirectInput->CreateDevice(GUID_SysMouse,
		&DIMouse,
		NULL);

	hr = DIKeyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = DIKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	hr = DIMouse->SetDataFormat(&c_dfDIMouse);
	hr = DIMouse->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_NOWINKEY | DISCL_FOREGROUND);

	return true;
}

