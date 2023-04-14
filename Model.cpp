#include "Model.h"
#include <regex>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>

Model::Model(std::string fileName, ID3D12GraphicsCommandList* commandList, Mesh* mesh, bool textured, bool dds, string texOverride)
{
	mDDS = dds;
	mTextured = textured;
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

	if (mTextured)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(SrvDescriptorHeap->mHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(mMaterials[0]->DiffuseSRVIndex, CbvSrvUavDescriptorSize);

		commandList->SetGraphicsRootDescriptorTable(0, tex);

		// Offset to Mat CBV for this mesh
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + mMaterials[0]->CBIndex * matCBByteSize;
		commandList->SetGraphicsRootConstantBufferView(3, matCBAddress);

	}
	
	if (mConstructorMesh)
	{
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + mConstructorMesh->mMaterial->CBIndex * matCBByteSize;
		commandList->SetGraphicsRootConstantBufferView(3, matCBAddress);
		mConstructorMesh->Draw(commandList);
	}

	for (auto mesh : mMeshes)
	{
		if (!mTextured)
		{
			// Offset to Mat CBV for this mesh
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + mesh->mMaterial->CBIndex * matCBByteSize;
			commandList->SetGraphicsRootConstantBufferView(3, matCBAddress);
		}	
		mesh->Draw(commandList);
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
	auto modelGeometry = new Mesh();
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

		modelGeometry->mVertices.push_back(vertex);
	}

	for (int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (int j = 0; j < face.mNumIndices; j++)
		{
			modelGeometry->mIndices.push_back(face.mIndices[j]);
		}
	}

	if (mTextured)
	{
		// Create material
		mMaterials.push_back(new Material());

		string str = "";

		if (mTexOverride == "")
		{
			str = mDirectory + "/" + mFileName;
		}
		else
		{
			str = mDirectory + "/" + mTexOverride;
		}
	
		std::wstring ws(str.begin(), str.end());
		mMaterials[0]->Name = ws;

		auto device = D3DDevice.Get();
		ResourceUploadBatch upload(device);

		upload.Begin();

		// Load albedo map
		mTextures.push_back(new Texture());

		if (mDDS) mTextures[0]->Path = mMaterials[0]->Name + L"-albedo.dds";
		else mTextures[0]->Path = mMaterials[0]->Name + L"_Albedo.jpg";

		if (mDDS) CreateDDSTextureFromFile(device, upload, mTextures[0]->Path.c_str(), mTextures[0]->Resource.ReleaseAndGetAddressOf(), false);
		else    CreateWICTextureFromFile(device, upload, mTextures[0]->Path.c_str(), mTextures[0]->Resource.ReleaseAndGetAddressOf(), false);

		auto modelTex = mTextures[0]->Resource;

		if (!modelTex)
		{

		}

		// Fill out the heap with actual descriptors.
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(SrvDescriptorHeap->mHeap->GetCPUDescriptorHandleForHeapStart());
		// next descriptor
		hDescriptor.Offset(CurrentSRVOffset, CbvSrvUavDescriptorSize);

		mMaterials[0]->DiffuseSRVIndex = CurrentSRVOffset;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelTex->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelTex->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelTex.Get(), &srvDesc, hDescriptor);

		mTextures.push_back(new Texture());

		if (mDDS) mTextures[1]->Path = mMaterials[0]->Name + L"-roughness.dds";
		else mTextures[1]->Path = mMaterials[0]->Name + L"_Roughness.jpg";

		if (mDDS) CreateDDSTextureFromFile(device, upload, mTextures[1]->Path.c_str(), mTextures[1]->Resource.ReleaseAndGetAddressOf(), false);
		else   CreateWICTextureFromFile(device, upload, mTextures[1]->Path.c_str(), mTextures[1]->Resource.ReleaseAndGetAddressOf(), false);

		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		auto modelRough = mTextures[1]->Resource;

		//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelRough->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelRough->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelRough.Get(), &srvDesc, hDescriptor);

		// Load normal texture
		mTextures.push_back(new Texture());

		if (mDDS) mTextures[2]->Path = mMaterials[0]->Name + L"-normal.dds";
		else mTextures[2]->Path = mMaterials[0]->Name + L"_Normal.jpg";

		if (mDDS)  CreateDDSTextureFromFile(device, upload, mTextures[2]->Path.c_str(), mTextures[2]->Resource.ReleaseAndGetAddressOf(), false);
		else CreateWICTextureFromFile(device, upload, mTextures[2]->Path.c_str(), mTextures[2]->Resource.ReleaseAndGetAddressOf(), false);

		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		auto modelNorm = mTextures[2]->Resource;

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelNorm->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelNorm->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelNorm.Get(), &srvDesc, hDescriptor);

		// Load metalness texture
		mTextures.push_back(new Texture());

		if (mDDS) mTextures[3]->Path = mMaterials[0]->Name + L"-metalness.dds";
		else mTextures[3]->Path = mMaterials[0]->Name + L"_Metalness.jpg";

		if (mDDS) CreateDDSTextureFromFile(device, upload, mTextures[3]->Path.c_str(), mTextures[3]->Resource.ReleaseAndGetAddressOf(), false);
		else   CreateWICTextureFromFile(device, upload, mTextures[3]->Path.c_str(), mTextures[3]->Resource.ReleaseAndGetAddressOf(), false);
		
		auto modelMetal = mTextures[3]->Resource;

		if (!modelMetal)
		{
			mTextures[3]->Path = L"Models/default.png";
			CreateWICTextureFromFile(device, upload, mTextures[3]->Path.c_str(), mTextures[3]->Resource.ReleaseAndGetAddressOf(), false);
			modelMetal = mTextures[3]->Resource;
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
		mTextures.push_back(new Texture());
		//mTextures[4]->Path = L"Models/default.png";

		if (mDDS) mTextures[4]->Path = mMaterials[0]->Name + L"-height.dds";
		else mTextures[4]->Path = mMaterials[0]->Name + L"_Displacement.jpg";

		if (mDDS) CreateDDSTextureFromFile(device, upload, mTextures[4]->Path.c_str(), mTextures[4]->Resource.ReleaseAndGetAddressOf(), false);
		else    CreateWICTextureFromFile(device, upload, mTextures[4]->Path.c_str(), mTextures[4]->Resource.ReleaseAndGetAddressOf(), false);

		auto modelDisplacement = mTextures[4]->Resource;
		
		if (!modelDisplacement)
		{
			mTextures[5]->Path = L"Models/default.png";
			CreateWICTextureFromFile(device, upload, mTextures[4]->Path.c_str(), mTextures[4]->Resource.ReleaseAndGetAddressOf(), false);
			modelDisplacement = mTextures[4]->Resource;
			mParallax = false;
		}
		
		hDescriptor.Offset(1, CbvSrvUavDescriptorSize);

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = modelDisplacement->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = modelDisplacement->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(modelDisplacement.Get(), &srvDesc, hDescriptor);

		// Load ao map
		mTextures.push_back(new Texture());

		if (mDDS) mTextures[5]->Path = mMaterials[0]->Name + L"-ao.dds";
		else mTextures[5]->Path = mMaterials[0]->Name + L"_AO.jpg";

		if (mDDS) CreateDDSTextureFromFile(device, upload, mTextures[5]->Path.c_str(), mTextures[5]->Resource.ReleaseAndGetAddressOf(), false);
		else    CreateWICTextureFromFile(device, upload, mTextures[5]->Path.c_str(), mTextures[5]->Resource.ReleaseAndGetAddressOf(), false);
		
		auto modelAO = mTextures[5]->Resource;

		if (!modelAO)
		{
			mTextures[5]->Path = L"Models/default.png";
			CreateWICTextureFromFile(device, upload, mTextures[5]->Path.c_str(), mTextures[5]->Resource.ReleaseAndGetAddressOf(), false);
			modelAO = mTextures[5]->Resource;
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
		mMaterials[0]->DiffuseAlbedo = XMFLOAT4{ diffuseColour.r,diffuseColour.g,diffuseColour.b,diffuseColour.a };

		float roughness = 0.0f;
		assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
		mMaterials[0]->Roughness = roughness;

		float metalness = 0.0f;
		assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness);
		mMaterials[0]->Metalness = metalness;


		modelGeometry->mMaterial = mMaterials[0];
		CurrentSRVOffset += 6;
	}
	else
	{
		// Process materials
		if (scene->HasMaterials())
		{
			if (mesh->mMaterialIndex >= 0)
			{
				aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];

				// Diffuse maps
				vector<Texture*> diffuseMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_DIFFUSE, "texture_diffuse", scene);
				modelGeometry->mTextures.insert(modelGeometry->mTextures.end(), diffuseMaps.begin(), diffuseMaps.end());

				// Base Colour maps
				vector<Texture*> baseColourMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_BASE_COLOR, "texture_base_colour", scene);
				modelGeometry->mTextures.insert(modelGeometry->mTextures.end(), baseColourMaps.begin(), baseColourMaps.end());

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

				modelGeometry->mMaterial = material;
			}
		}

	}
	
	return modelGeometry;
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
