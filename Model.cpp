#include "Model.h"
#include <regex>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>

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
			aiProcess_OptimizeMeshes |
			aiProcess_OptimizeGraph |
			aiProcess_CalcTangentSpace |
			aiProcess_ConvertToLeftHanded);

		if (!scene)
		{
			LPCWSTR str = LPCWSTR(importer.GetErrorString());
			MessageBox(0, L"Error importing models", L"Error", MB_OK);
		}

		mDirectory = fileName.substr(0, fileName.find_last_of('/'));
		mFileName = fileName.substr(fileName.find_last_of('/') + 1, fileName.find_last_of('.') - fileName.find_last_of('/') - 1);
		ProcessNode(scene->mRootNode, scene);

		for (auto& mesh : mMeshes)
		{
			mesh->CalculateBufferData(D3DDevice.Get(), commandList);
		}
	}
	else
	{
		mConstructorMesh = mesh;
	}
}

Model::~Model()
{
	for (auto& mesh : mMeshes)
	{
		delete mesh;
	}
	for (auto& texture : mLoadedTextures)
	{
		delete texture;
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

	if (!mConstructorMesh)
	{
		int index = 0;;
		for (auto& mesh : mMeshes)
		{
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress;
			if (mTextured)
			{
				// Offset to texture diffuse SRV from model
				CD3DX12_GPU_DESCRIPTOR_HANDLE tex(SrvDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
				tex.Offset(mesh->mMaterial->DiffuseSRVIndex, CbvSrvUavDescriptorSize);
				commandList->SetGraphicsRootDescriptorTable(0, tex);
			}

			// Offset to Mat CBV for this mesh
			matCBAddress = matCB->GetGPUVirtualAddress() + mesh->mMaterial->CBIndex * matCBByteSize;
			commandList->SetGraphicsRootConstantBufferView(3, matCBAddress);
			index++;

			mesh->Draw(commandList);
		}
	}
	else
	{
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + mConstructorMesh->mMaterial->CBIndex * matCBByteSize;
		commandList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		mConstructorMesh->Draw(commandList);
	}

	for (auto mesh : mMeshes)
	{
		if (!mTextured)
		{
		}	
		
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
	auto newMesh = new Mesh();
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

	for (int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++)
		{
			newMesh->mIndices.push_back(face.mIndices[j]);
		}
	}

	// Create material
	newMesh->mMaterial = new Material();

	// Use either model name or file name
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

	// Load albedo map
	newMesh->mTextures.push_back(new Texture());
	auto texIndex = 0;

	newMesh->mTextures[texIndex]->Path = matName + L"-albedo.dds";
	CreateDDSTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
	if (newMesh->mTextures[texIndex]->Resource != nullptr)
	{
		mTextured = true;
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
				mTextured = false;
			}
			else { mTextured = true; mPNG = true; };
		}
		else { mTextured = true; mJPG = true; };
	}

	// If textures found, load the rest
	if(mTextured)
	{
		// Load albedo
		auto modelTex = newMesh->mTextures[texIndex]->Resource;

		// Fill out the heap with actual descriptors.
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(SrvDescriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
		// next descriptor
		hDescriptor.Offset(CurrentSRVOffset, CbvSrvUavDescriptorSize);

		newMesh->mMaterial->DiffuseSRVIndex = CurrentSRVOffset;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelTex->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelTex->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelTex.Get(), &srvDesc, hDescriptor);

		newMesh->mTextures.push_back(new Texture());
		texIndex++;
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
		else
		{
			newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), true);
		}
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		auto modelRough = newMesh->mTextures[texIndex]->Resource;

		//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelRough->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelRough->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelRough.Get(), &srvDesc, hDescriptor);

		// Load normal texture
		newMesh->mTextures.push_back(new Texture());
		texIndex++;
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
		else
		{
			newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
			CreateWICTextureFromFile(device, upload, newMesh->mTextures[texIndex]->Path.c_str(), newMesh->mTextures[texIndex]->Resource.ReleaseAndGetAddressOf(), false);
		}

		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		auto modelNorm = newMesh->mTextures[texIndex]->Resource;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelNorm->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelNorm->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelNorm.Get(), &srvDesc, hDescriptor);

		// Load metalness texture
		newMesh->mTextures.push_back(new Texture());
		texIndex++;
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
		else
		{
			newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
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

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelMetal->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelMetal->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelMetal.Get(), &srvDesc, hDescriptor);

		// Load height texture
		newMesh->mTextures.push_back(new Texture());
		texIndex++;
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
		else
		{
			newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
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
		
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelHeight->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelHeight->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelHeight.Get(), &srvDesc, hDescriptor);

		// Load ao map
		newMesh->mTextures.push_back(new Texture());
		texIndex++;
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
		else
		{
			newMesh->mTextures[texIndex]->Path = L"Models/missing.png";
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

		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelAO->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelAO->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelAO.Get(), &srvDesc, hDescriptor);

		// Upload the resources to the GPU.
		auto finish = upload.End(CommandQueue.Get());

		// Wait for the upload thread to terminate
		finish.wait();

		aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];

		aiColor4D diffuseColour;
		assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColour);
		newMesh->mMaterial->DiffuseAlbedo = XMFLOAT4{ diffuseColour.r,diffuseColour.g,diffuseColour.b,diffuseColour.a };

		float roughness = 0.0f;
		assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
		newMesh->mMaterial->Roughness = roughness;

		float metalness = 0.0f;
		assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness);
		newMesh->mMaterial->Metalness = metalness;

		CurrentSRVOffset += 6;
	}
	else
	{
		auto finish = upload.End(CommandQueue.Get());
		finish.wait();

		// Process base materials
		if (scene->HasMaterials())
		{
			if (mesh->mMaterialIndex >= 0)
			{
				aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];

				// Diffuse maps
				vector<Texture*> diffuseMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_DIFFUSE, "texture_diffuse", scene);
				newMesh->mTextures.insert(newMesh->mTextures.end(), diffuseMaps.begin(), diffuseMaps.end());

				// Base Colour maps
				vector<Texture*> baseColourMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_BASE_COLOR, "texture_base_colour", scene);
				newMesh->mTextures.insert(newMesh->mTextures.end(), baseColourMaps.begin(), baseColourMaps.end());

				Material* material = new Material;
				aiString materialName;
				assimpMaterial->Get(AI_MATKEY_NAME, materialName);
				material->AiName = materialName;

				aiColor4D diffuseColour;
				assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColour);
				material->DiffuseAlbedo = XMFLOAT4{ diffuseColour.r,diffuseColour.g,diffuseColour.b,diffuseColour.a };

				float roughness = 0.0f;
				assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
				material->Roughness = roughness;

				float metalness = 0.0f;
				assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness);
				material->Metalness = metalness;

				newMesh->mMaterial = material;
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
