#include "Model.h"
#include <regex>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>
#include <iostream>

Model::Model(std::string fileName, ID3D12GraphicsCommandList* commandList, Mesh* mesh, string texOverride)
{
	mTexOverride = texOverride;

	if (mesh == nullptr)
	{
		Assimp::Importer importer;

		const aiScene* scene = importer.ReadFile(fileName,
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ImproveCacheLocality |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_SortByPType |
			aiProcess_FindInvalidData |
			aiProcess_PreTransformVertices |
			//aiProcess_OptimizeMeshes |
			//aiProcess_OptimizeGraph |
			//aiProcess_FlipUVs |
			//aiProcess_GenUVCoords|
			//aiProcess_TransformUVCoords|
			aiProcess_CalcTangentSpace |
			aiProcess_ConvertToLeftHanded);

		if (!scene)
		{
			// Output assimp error
			string str = "Error importing models : " + string(importer.GetErrorString());
			wstring wstr(str.begin(), str.end());
			LPCWSTR lstr(wstr.c_str());
			MessageBox(0, lstr, L"Error", MB_OK);
		}

		// Save dir and file name
		mDirectory = fileName.substr(0, fileName.find_last_of('/'));
		mFileName = fileName.substr(fileName.find_last_of('/') + 1, fileName.find_last_of('.') - fileName.find_last_of('/') - 1);
		
		// Process scene nodes
		ProcessNode(scene->mRootNode, scene);

		// Set textured to true if textures found
		for (auto mesh : mMeshes)
		{
			if (mesh->mTextures.size() > 0)
			{
				mTextured = true;
			}
		}
		
		// Calculate buffer data for meshes
		for (auto& mesh : mMeshes)
		{
			mesh->CalculateBufferData(D3DDevice.Get(), commandList);
		}
	}
	else
	{
		// Use mesh from constructor
		mConstructorMesh = mesh;
	}
}

Model::~Model()
{
	for (auto& mesh : mMeshes)
	{
		delete mesh;
	}
}

void Model::Draw(ID3D12GraphicsCommandList* commandList)
{
	// Get reference to current per object constant buffer
	auto objectCB = FrameResources[CurrentFrameResourceIndex]->mPerObjectConstantBuffer->GetBuffer();

	// Get size of the per object constant buffer 
	UINT objCBByteSize = CalculateConstantBufferSize(sizeof(PerObjectConstants));

	auto matCB = FrameResources[CurrentFrameResourceIndex]->mPerMaterialConstantBuffer->GetBuffer();

	UINT matCBByteSize = CalculateConstantBufferSize(sizeof(PerMaterialConstants));

	// Offset to the CBV for this object
	auto objCBAddress = objectCB->GetGPUVirtualAddress() + mObjConstantBufferIndex * objCBByteSize;
	commandList->SetGraphicsRootConstantBufferView(1, objCBAddress);

	// If not using mesh from constructor
	if (!mConstructorMesh)
	{
		for (auto& mesh : mMeshes)
		{
			if (mTextured)
			{
				if (mesh->mMaterial->DiffuseSRVIndex > -1)
				{
					// Offset to texture diffuse SRV from model
					CD3DX12_GPU_DESCRIPTOR_HANDLE tex(SrvDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
					tex.Offset(mesh->mMaterial->DiffuseSRVIndex, CbvSrvUavDescriptorSize);
					commandList->SetGraphicsRootDescriptorTable(0, tex);
				}		
			}

			// Offset to Mat CBV for this mesh
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress;
			matCBAddress = matCB->GetGPUVirtualAddress() + mesh->mMaterial->CBIndex * matCBByteSize;
			commandList->SetGraphicsRootConstantBufferView(3, matCBAddress);

			mesh->Draw(commandList);
		}
	}
	else
	{
		if (mConstructorMesh->mMaterial)
		{
			// Use constructor mesh's matCB index
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + mConstructorMesh->mMaterial->CBIndex * matCBByteSize;
			commandList->SetGraphicsRootConstantBufferView(3, matCBAddress);
		}

		mConstructorMesh->Draw(commandList);
	}
}

void Model::SetPosition(XMFLOAT3 position, bool update)
{
	mPosition = position;
	if(update) UpdateWorldMatrix();
}

void Model::SetRotation(XMFLOAT3 rotation, bool update)
{
	mRotation = rotation;
	if (update) UpdateWorldMatrix();
}

void Model::SetScale(XMFLOAT3 scale, bool update)
{
	mScale = scale;
	if (update) UpdateWorldMatrix();
}

void Model::ProcessNode(aiNode* node, const aiScene* scene)
{
	// Process each mesh
	for (int i = 0; i < node->mNumMeshes; i++)
	{
		// Node only indexes objects in the scene
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		mMeshes.push_back(ProcessMesh(mesh, scene));
	}

	// Process each child node
	for (int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene);
	}
}

Mesh* Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
	// Make a new mesh
	auto newMesh = new Mesh();

	// For each vertex, extract information
	for (int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;

		if (mesh->HasPositions())
		{
			vertex.Pos.x = mesh->mVertices[i].x;
			vertex.Pos.y = mesh->mVertices[i].y;
			vertex.Pos.z = mesh->mVertices[i].z;
		}

		if (mesh->HasNormals())
		{
			vertex.Normal.x = mesh->mNormals[i].x;
			vertex.Normal.y = mesh->mNormals[i].y;
			vertex.Normal.z = mesh->mNormals[i].z;
		}

		if (mesh->HasVertexColors(i))
		{
			vertex.Colour.x = mesh->mColors[i]->r;
			vertex.Colour.y = mesh->mColors[i]->g;
			vertex.Colour.z = mesh->mColors[i]->b;
			vertex.Colour.w = mesh->mColors[i]->a;
		}
		else
		{
			vertex.Colour = { 0.1,0.1,0,0 };
		}

		if (mesh->mTextureCoords[0])
		{
			XMFLOAT2 vec;
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.UV = vec;
		}
		else
		{
			vertex.UV = XMFLOAT2(0.0f, 0.0f);
		}

		if (mesh->HasTangentsAndBitangents())
		{
			vertex.Tangent.x = mesh->mTangents[i].x;
			vertex.Tangent.y = mesh->mTangents[i].y;
			vertex.Tangent.z = mesh->mTangents[i].z;
		}

		newMesh->mVertices.push_back(vertex);
	}

	// For each face extract indices
	for (int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++)
		{
			newMesh->mIndices.push_back(face.mIndices[j]);
		}
	}

	// Create new material
	newMesh->mMaterial = new Material();

	// Use either file name or override name
	string str = "";
	if (mTexOverride == "")
	{

		str = mDirectory + "/" + mFileName;
	}
	else
	{
		str = mDirectory + "/" + mTexOverride;
	}

	// Set material name 
	std::wstring ws(str.begin(), str.end());
	newMesh->mMaterial->Name = ws;

	auto matName = newMesh->mMaterial->Name;

	// Create resource upload batch
	auto device = D3DDevice.Get();
	ResourceUploadBatch upload(device);
	upload.Begin();

	// Make a new texture
	newMesh->mTextures.push_back(new Texture());
	auto texIndex = 0;

	// Test for albedo map using model name
	newMesh->mTextures[texIndex]->Path = matName + L"-albedo.dds";
	CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
	
	// If texture found, set DDS to true and set model as textured per model.
	if (newMesh->mTextures[texIndex]->Resource != nullptr)
	{
		mModelTextured = true;
		mDDS = true;
	}

	// Test other file extensions if DDS not found
	if (!mDDS)
	{
		// Test jpg
		newMesh->mTextures[texIndex]->Path = matName + L"-albedo.jpg";
		CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		if (!newMesh->mTextures[texIndex]->Resource)
		{
			// Test png
			newMesh->mTextures[texIndex]->Path = matName + L"-albedo.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
			if (!newMesh->mTextures[texIndex]->Resource)
			{
				// No textures found
				newMesh->mMaterial = nullptr;
				newMesh->mTextures.clear();
				mModelTextured = false;
			}
			else { mModelTextured = true; mPNG = true; };
		}
		else { mModelTextured = true; mJPG = true; };
	}

	// If textures found, load the rest
	if(mModelTextured)
	{
		// Load albedo
		auto modelTex = newMesh->mTextures[texIndex]->Resource;

		// Fill out the heap with descriptors
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(SrvDescriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
		
		// Offset to next descriptor
		hDescriptor.Offset(CurrentSRVOffset, CbvSrvUavDescriptorSize);

		// Set material SRVIndex
		newMesh->mMaterial->DiffuseSRVIndex = CurrentSRVOffset;

		// Create descriptor
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelTex->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelTex->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// Create SRV
		device->CreateShaderResourceView(modelTex.Get(), &srvDesc, hDescriptor);

		// Create new texture and increment index
		newMesh->mTextures.push_back(new Texture());
		texIndex++;

		// Load correct file type
		if (mDDS)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-roughness.dds";
			CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mJPG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-roughness.jpg";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), true);
		}
		else if(mPNG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-roughness.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), true);
		}

		// Offset to next descriptor
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		// If no textures found, load missing texture
		auto modelRough = newMesh->mTextures[texIndex]->Resource;
		if (!modelRough) 
		{
			newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), true);
			modelRough = newMesh->mTextures[texIndex]->Resource;
		}

		// Create descriptor
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelRough->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelRough->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// Create SRV
		device->CreateShaderResourceView(modelRough.Get(), &srvDesc, hDescriptor);

		// Load normal texture
		newMesh->mTextures.push_back(new Texture());
		texIndex++;

		// Load correct file type
		if (mDDS)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-normal.dds";
			CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mJPG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-normal.jpg";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mPNG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-normal.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}

		// Offset to next descriptor
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		auto modelNorm = newMesh->mTextures[texIndex]->Resource;

		// Load missing texture if normal map not found
		if (!modelNorm)
		{
			newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
			modelNorm = newMesh->mTextures[texIndex]->Resource;

		}

		// Create descriptor
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelNorm->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelNorm->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// Create SRV
		device->CreateShaderResourceView(modelNorm.Get(), &srvDesc, hDescriptor);

		// Load metalness texture
		newMesh->mTextures.push_back(new Texture());
		texIndex++;

		// Load correct file type
		if (mDDS)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-metalness.dds";
			CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mJPG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-metalness.jpg";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mPNG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-metalness.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		
		// Load white texture if no metalness map
		auto modelMetal = newMesh->mTextures[texIndex]->Resource;
		if (!modelMetal)
		{
			newMesh->mTextures[texIndex]->Path = L"Models/default.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
			modelMetal = newMesh->mTextures[texIndex]->Resource;
		}

		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		// Create descriptor
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelMetal->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelMetal->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// Create SRV
		device->CreateShaderResourceView(modelMetal.Get(), &srvDesc, hDescriptor);

		// Load height texture
		newMesh->mTextures.push_back(new Texture());
		texIndex++;

		// Load correct file type
		if (mDDS)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-height.dds";
			CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mJPG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-height.jpg";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mPNG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-height.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}

		// Load white texture if no height map
		auto modelHeight = newMesh->mTextures[texIndex]->Resource;
		if (!modelHeight)
		{
			newMesh->mTextures[texIndex]->Path = L"Models/default.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
			modelHeight = newMesh->mTextures[texIndex]->Resource;
			mParallax = false;
		}
		
		// Offset to next descriptor
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		// Create SRV descriptor
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelHeight->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelHeight->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// Create SRV
		device->CreateShaderResourceView(modelHeight.Get(), &srvDesc, hDescriptor);

		// Load ao map
		newMesh->mTextures.push_back(new Texture());
		texIndex++;

		// Load correct file type
		if (mDDS)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-ao.dds";
			CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mJPG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-ao.jpg";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mPNG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-ao.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}

		// Load white texture if no AO
		auto modelAO = newMesh->mTextures[texIndex]->Resource;
		if (!modelAO)
		{
			newMesh->mTextures[texIndex]->Path = L"Models/default.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
			modelAO = newMesh->mTextures[texIndex]->Resource;
		}

		// Offset to next descriptor
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);
		
		// Create SRV descriptor
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelAO->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelAO->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// Create SRV
		device->CreateShaderResourceView(modelAO.Get(), &srvDesc, hDescriptor);

		// Load emissive map
		newMesh->mTextures.push_back(new Texture());
		texIndex++;

		// Load correct file type
		if (mDDS)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-emissive.dds";
			CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mJPG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-emissive.jpg";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}
		else if (mPNG)
		{
			newMesh->mTextures[texIndex]->Path = matName + L"-emissive.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}

		// Load black texture if no emissive
		auto modelEmissive = newMesh->mTextures[texIndex]->Resource;
		if (!modelEmissive)
		{
			newMesh->mTextures[texIndex]->Path = L"Models/defaultBlack.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
			modelEmissive = newMesh->mTextures[texIndex]->Resource;
		}

		// Offset to next descriptor
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		// Create SRV descriptor
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelEmissive->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelEmissive->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		// Create SRV
		device->CreateShaderResourceView(modelEmissive.Get(), &srvDesc, hDescriptor);

		// Load assimp material
		aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];
		
		// Extract PBR info
		aiColor4D diffuseColour;
		assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColour);
		newMesh->mMaterial->DiffuseAlbedo = XMFLOAT4{ diffuseColour.r,diffuseColour.g,diffuseColour.b,diffuseColour.a };

		float roughness = 0.0f;
		assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
		newMesh->mMaterial->Roughness = roughness;

		float metalness = 0.0f;
		assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness);
		newMesh->mMaterial->Metalness = metalness;

		// Offset SRV index
		CurrentSRVOffset += 7;

		// Finish upload
		auto finish = upload.End(CommandQueue.Get());
		finish.wait();
	}
	else
	{
		// Process base materials
		if (scene->HasMaterials())
		{
			if (mesh->mMaterialIndex >= 0)
			{
				auto numMaterials = scene->mNumMaterials;
				aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];
			
				// Diffuse maps
				//vector<Texture*> diffuseMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_DIFFUSE, "texture_diffuse", scene);
				//newMesh->mTextures.insert(newMesh->mTextures.end(), diffuseMaps.begin(), diffuseMaps.end());

				// Base Colour maps
				//vector<Texture*> baseColourMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_BASE_COLOR, "texture_base_colour", scene);
				//newMesh->mTextures.insert(newMesh->mTextures.end(), baseColourMaps.begin(), baseColourMaps.end());

				// Create new material
				newMesh->mMaterial = new Material;

				// Get material name
				aiString materialName;
				assimpMaterial->Get(AI_MATKEY_NAME, materialName);
				newMesh->mMaterial->AiName = materialName;

				bool thisMeshTextured = false;
				// Load textures from aiMat name if exist

				// Test for albedo map using mesh material name
				newMesh->mTextures.push_back(new Texture());
				auto texIndex = 0;
				
				string str = mDirectory + "/" + materialName.C_Str();
				wstring wstr(str.begin(), str.end());
				newMesh->mTextures[texIndex]->Path = wstr + L"-albedo.dds";				
				

				if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
				{
					CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
				}
				
				// If albedo dds found
				if (newMesh->mTextures[texIndex]->Resource != nullptr)
				{
					mPerMeshTextured = true;
					thisMeshTextured = true;
					mDDS = true;
				}

				// Test other file extensions if DDS not found
				if (!mDDS)
				{
					// Test jpg
					newMesh->mTextures[texIndex]->Path = wstr + L"-albedo.jpg";
					if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
					{
						CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
					}
					if (!newMesh->mTextures[texIndex]->Resource)
					{
						// Test png
						newMesh->mTextures[texIndex]->Path = wstr + L"-albedo.png";
						if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
						{
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
						}
						if (!newMesh->mTextures[texIndex]->Resource)
						{
							// No textures found
							//newMesh->mMaterial = nullptr;
							newMesh->mTextures.clear();
							thisMeshTextured = false;
						}
						else { mPerMeshTextured = true; thisMeshTextured = true; mPNG = true; };
					}
					else { mPerMeshTextured = true; thisMeshTextured = true; mJPG = true; };
				}

				if (thisMeshTextured)
				{
					// Fill out the heap with actual descriptors.
					CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(SrvDescriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
					// next descriptor
					hDescriptor.Offset(CurrentSRVOffset, CbvSrvUavDescriptorSize);

					newMesh->mMaterial->DiffuseSRVIndex = CurrentSRVOffset;

					D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
					srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					srvDesc.Format = newMesh->mTextures[texIndex]->Resource->GetDesc().Format;
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Texture2D.MostDetailedMip = 0;
					srvDesc.Texture2D.MipLevels = newMesh->mTextures[texIndex]->Resource->GetDesc().MipLevels;
					srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

					device->CreateShaderResourceView(newMesh->mTextures[texIndex]->Resource.Get(), &srvDesc, hDescriptor);
					mLoadedTextures.push_back(newMesh->mTextures[texIndex]);
					
					CurrentSRVOffset++;

					// Test for roughness PBR texture
					// Create new texture and increment index
					newMesh->mTextures.push_back(new Texture());
					texIndex++;

					// Load correct file type
					if (mDDS)
					{
						newMesh->mTextures[texIndex]->Path = wstr + L"-roughness.dds";
						if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
						{
							CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
						}
					}
					else if (mJPG)
					{
						newMesh->mTextures[texIndex]->Path = wstr + L"-roughness.jpg";
						if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
						{
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), true);
						}
					}
					else if (mPNG)
					{
						newMesh->mTextures[texIndex]->Path = wstr + L"-roughness.png";
						if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
						{
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), true);
						}
					}

					// Offset to next descriptor
					hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

					auto modelRough = newMesh->mTextures[texIndex]->Resource;
					if (modelRough)
					{
						mPerMeshPBR = true;
					}
					else { delete newMesh->mTextures[1]; }

					if (mPerMeshPBR && thisMeshTextured)
					{
						// Create descriptor
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Format = modelRough->GetDesc().Format;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = modelRough->GetDesc().MipLevels;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// Create SRV
						device->CreateShaderResourceView(modelRough.Get(), &srvDesc, hDescriptor);
						mLoadedTextures.push_back(newMesh->mTextures[texIndex]);

						// Load normal texture
						newMesh->mTextures.push_back(new Texture());
						texIndex++;

						// Load correct file type
						if (mDDS)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-normal.dds";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mJPG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-normal.jpg";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mPNG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-normal.png";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}

						// Offset to next descriptor
						hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

						auto modelNorm = newMesh->mTextures[texIndex]->Resource;

						// Load missing texture if normal map not found
						if (!modelNorm)
						{
							newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							modelNorm = newMesh->mTextures[texIndex]->Resource;
						}

						// Create descriptor
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Format = modelNorm->GetDesc().Format;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = modelNorm->GetDesc().MipLevels;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// Create SRV
						device->CreateShaderResourceView(modelNorm.Get(), &srvDesc, hDescriptor);
						mLoadedTextures.push_back(newMesh->mTextures[texIndex]);

						// Load metalness texture
						newMesh->mTextures.push_back(new Texture());
						texIndex++;

						// Load correct file type
						if (mDDS)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-metalness.dds";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mJPG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-metalness.jpg";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mPNG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-metalness.png";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}

						// Load white texture if no metalness map
						auto modelMetal = newMesh->mTextures[texIndex]->Resource;
						if (!modelMetal)
						{
							newMesh->mTextures[texIndex]->Path = L"Models/default.png";
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							modelMetal = newMesh->mTextures[texIndex]->Resource;
						}

						hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

						// Create descriptor
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Format = modelMetal->GetDesc().Format;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = modelMetal->GetDesc().MipLevels;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// Create SRV
						device->CreateShaderResourceView(modelMetal.Get(), &srvDesc, hDescriptor);
						mLoadedTextures.push_back(newMesh->mTextures[texIndex]);

						// Load height texture
						newMesh->mTextures.push_back(new Texture());
						texIndex++;

						// Load correct file type
						if (mDDS)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-height.dds";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mJPG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-height.jpg";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mPNG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-height.png";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}

						// Load white texture if no height map
						auto modelHeight = newMesh->mTextures[texIndex]->Resource;
						if (!modelHeight)
						{
							newMesh->mTextures[texIndex]->Path = L"Models/default.png";
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							modelHeight = newMesh->mTextures[texIndex]->Resource;
							mParallax = false;
						}

						// Offset to next descriptor
						hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

						// Create SRV descriptor
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Format = modelHeight->GetDesc().Format;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = modelHeight->GetDesc().MipLevels;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// Create SRV
						device->CreateShaderResourceView(modelHeight.Get(), &srvDesc, hDescriptor);
						mLoadedTextures.push_back(newMesh->mTextures[texIndex]);
						
						// Load ao map
						newMesh->mTextures.push_back(new Texture());
						texIndex++;

						// Load correct file type
						if (mDDS)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-ao.dds";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mJPG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-ao.jpg";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mPNG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-ao.png";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex])) 
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}

						// Load white texture if no AO
						auto modelAO = newMesh->mTextures[texIndex]->Resource;
						if (!modelAO)
						{
							newMesh->mTextures[texIndex]->Path = L"Models/default.png";
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							modelAO = newMesh->mTextures[texIndex]->Resource;
						}

						// Offset to next descriptor
						hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

						// Create SRV descriptor
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Format = modelAO->GetDesc().Format;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = modelAO->GetDesc().MipLevels;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// Create SRV
						device->CreateShaderResourceView(modelAO.Get(), &srvDesc, hDescriptor);
						mLoadedTextures.push_back(newMesh->mTextures[texIndex]);

						// Load emissive map
						newMesh->mTextures.push_back(new Texture());
						texIndex++;

						// Load correct file type
						if (mDDS)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-emissive.dds";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mJPG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-emissive.jpg";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}
						else if (mPNG)
						{
							newMesh->mTextures[texIndex]->Path = wstr + L"-emissive.png";
							if (!CheckTextureLoaded(newMesh->mTextures[texIndex]))
							{
								CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							}
						}

						// Load black texture if no Emissive
						auto modelEmissive = newMesh->mTextures[texIndex]->Resource;
						if (!modelEmissive)
						{
							newMesh->mTextures[texIndex]->Path = L"Models/defaultBlack.png";
							CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
							modelEmissive = newMesh->mTextures[texIndex]->Resource;
						}

						// Offset to next descriptor
						hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

						// Create SRV descriptor
						srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						srvDesc.Format = modelEmissive->GetDesc().Format;
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						srvDesc.Texture2D.MostDetailedMip = 0;
						srvDesc.Texture2D.MipLevels = modelEmissive->GetDesc().MipLevels;
						srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

						// Create SRV
						device->CreateShaderResourceView(modelEmissive.Get(), &srvDesc, hDescriptor);
						
						mLoadedTextures.push_back(newMesh->mTextures[texIndex]);

						CurrentSRVOffset += 6; // Albedo already incremented once
					}
					
				}

				aiColor4D diffuseColour;
				assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColour);
				newMesh->mMaterial->DiffuseAlbedo = XMFLOAT4{ diffuseColour.r,diffuseColour.g,diffuseColour.b,diffuseColour.a };

				float roughness = 0.0f;
				assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
				newMesh->mMaterial->Roughness = roughness;

				float metalness = 0.0f;
				assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness);
				newMesh->mMaterial->Metalness = metalness;

				auto finish = upload.End(CommandQueue.Get());
				finish.wait();
			}
		}

	}

	return newMesh;
}

vector<Texture*> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene)
{
	vector<Texture*> textures;

	if (mat->GetTextureCount(type) > 0)
	{
		for (int i = 0; i < mat->GetTextureCount(type); i++)
		{
			aiString str;
			mat->GetTexture(type, i, &str);
			bool skip = false;
			for (int j = 0; j < mLoadedTextures.size(); j++)
			{
				if (mLoadedTextures[j]->AIPath == str)
				{
					textures.push_back(mLoadedTextures[j]);
					skip = true; // Texture has already been laoded
					break;
				}
			}
			if (!skip)
			{
				Texture* texture = new Texture();
				const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());
				if (embeddedTexture != nullptr)
				{
					LoadEmbeddedTexture(embeddedTexture);
				}
				else
				{
					texture->ID = LoadTextureFromFile(str.C_Str(), mDirectory);
				}
				texture->Type = typeName;
				texture->AIPath = str;
				textures.push_back(texture);
				mLoadedTextures.push_back(texture);
			}
		}
	}
	return textures;
}

int Model::LoadTextureFromFile(const char* path, string directory)
{
	// Generate texture ID and load texture data 
	string filename = string(path);
	filename = directory + '/' + filename;

	// Make texture
	//DirectX::CreateDDSTextureFromFile();
	return 0;
}

void Model::LoadEmbeddedTexture(const aiTexture* embeddedTexture)
{
	// Unsupported as assimp sample does not work

	//if (embeddedTexture->mHeight != 0)
	//{
	//	// Make texture
	//	auto width = embeddedTexture->mWidth;
	//	auto height = embeddedTexture->mHeight;
	//}
}

void Model::UpdateWorldMatrix()
{
	XMStoreFloat4x4(&mWorldMatrix, XMMatrixIdentity()
		* XMMatrixScaling(mScale.x,mScale.y,mScale.z)
		* XMMatrixRotationX(mRotation.x)
		* XMMatrixRotationY(mRotation.y)
		* XMMatrixRotationZ(mRotation.z)
		* XMMatrixTranslation(mPosition.x,mPosition.y,mPosition.z));
}

bool Model::CheckTextureLoaded(Texture* texture)
{
	for (auto loadedTex : mLoadedTextures)
	{
		if (texture->Path == loadedTex->Path)
		{
			texture->Resource = loadedTex->Resource;
			return true;
		}
	}
	return false;
}
