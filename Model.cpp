#include "Model.h"
#include <regex>
Model::Model(std::string fileName, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(fileName,
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType |
		aiProcess_OptimizeGraph |
		aiProcess_OptimizeMeshes |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_ConvertToLeftHanded);

	if (!scene)
	{
		LPCWSTR str = LPCWSTR(importer.GetErrorString());
		MessageBox(0, L"Error importing models", L"Error", MB_OK);
	}

	mDirectory = fileName.substr(0, fileName.find_last_of('/'));
	mFileName = fileName.substr(fileName.find_last_of('/') + 1, fileName.find_last_of('.') - fileName.find_last_of('/') - 1);
	ProcessNode(scene->mRootNode, scene);

	if (mBaseMaterials.size() > 0)
	{
		for (auto& mesh : mMeshes)
		{
			// temp dont do this
			for (auto& vertex : mesh->mVertices)
			{
				vertex.Colour = mBaseMaterials[mesh->mMaterialIndex]->DiffuseColour;
			}

			mesh->CalculateBufferData(device, commandList);
		}
	}
	else
	{
		for (auto& mesh : mMeshes)
		{
			mesh->CalculateBufferData(device, commandList);
		}
	}
	

	//int numVerts = 0;
	//for (auto& mesh : mMeshes)
	//{
	//	for (auto& vertex : mesh->mVertices)
	//	{
	//		mMesh->mVertices.push_back(vertex);
	//	}
	//	for (int i = 0; i < mesh->mIndices.size(); i++)
	//	{
	//		if (numVerts == 0) mMesh->mIndices.push_back(mesh->mIndices[i]);
	//		else mMesh->mIndices.push_back(mesh->mIndices[i] + numVerts - 1);
	//	}
	//	numVerts = mMesh->mVertices.size();
	//}
}

Model::~Model()
{
	for (auto& mesh : mMeshes)
	{
		delete mesh;
	}
	for (auto& material : mBaseMaterials)
	{
		delete material;
	}
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

	// Process materials
	if (scene->HasMaterials())
	{
		if (mesh->mMaterialIndex >= 0)
		{
			modelGeometry->mMaterialIndex = mesh->mMaterialIndex;
			aiMaterial* assimpMaterial = scene->mMaterials[mesh->mMaterialIndex];

			// Diffuse maps
			vector<Texture*> diffuseMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_DIFFUSE, "texture_diffuse", scene);
			modelGeometry->mTextures.insert(modelGeometry->mTextures.end(), diffuseMaps.begin(), diffuseMaps.end());

			// Base Colour maps
			vector<Texture*> baseColourMaps = LoadMaterialTextures(assimpMaterial, aiTextureType_BASE_COLOR, "texture_base_colour", scene);
			modelGeometry->mTextures.insert(modelGeometry->mTextures.end(), baseColourMaps.begin(), baseColourMaps.end());

			if (baseColourMaps.size() == 0 && diffuseMaps.size() == 0)
			{
				// No textures
				Material* material = new Material;
				aiString materialName;
				assimpMaterial->Get(AI_MATKEY_NAME, materialName);
				material->Name = materialName.C_Str();

				aiColor4D diffuseColour;
				assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColour);
				material->DiffuseColour = XMFLOAT4{ diffuseColour.r,diffuseColour.g,diffuseColour.b,diffuseColour.a };

				aiColor3D specularColour;
				assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specularColour);
				material->SpecularColour = XMFLOAT3{ specularColour.r,specularColour.g,specularColour.b };

				aiColor3D ambientColour;
				assimpMaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambientColour);
				material->AmbientColour = XMFLOAT3{ ambientColour.r,ambientColour.g,ambientColour.b };

				aiColor3D emmissiveColour;
				assimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emmissiveColour);
				material->EmissiveColour = XMFLOAT3{ emmissiveColour.r,emmissiveColour.g,emmissiveColour.b };

				aiColor3D baseColour;
				assimpMaterial->Get(AI_MATKEY_BASE_COLOR, baseColour);
				material->BaseColour = XMFLOAT3{ baseColour.r,baseColour.g,baseColour.b };

				mBaseMaterials.push_back(material);
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
				if (mLoadedTextures[j]->Path == str)
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
				texture->Path = str;
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