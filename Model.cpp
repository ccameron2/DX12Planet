#include "Model.h"

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
		aiProcess_ConvertToLeftHanded);

	if (!scene)
	{
		LPCWSTR str = LPCWSTR(importer.GetErrorString());
		MessageBox(0, L"Error importing models", L"Error", MB_OK);
	}

	mDirectory = fileName.substr(0, fileName.find_last_of('/'));

	ProcessNode(scene->mRootNode, scene);

	for (auto& mesh : mMeshes)
	{
		mesh->CalculateBufferData(device, commandList);
	}

	//mMesh = new GeometryData();

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

	//mMesh->CalculateBufferData(device,commandList);
}

Model::~Model()
{
	delete mMesh;
	for (auto& mesh : mMeshes)
	{
		delete mesh;
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

GeometryData* Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
	auto modelGeometry = new GeometryData();
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
			vertex.Colour = { 1,0.2,0.2,0 };
		}
		// Texture Coordinates
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
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		// Diffuse maps
		vector<Texture*> diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
		modelGeometry->mTextures.insert(modelGeometry->mTextures.end(), diffuseMaps.begin(), diffuseMaps.end());
		
		// Specular maps
		vector<Texture*> specularMaps = LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
		modelGeometry->mTextures.insert(modelGeometry->mTextures.end(), specularMaps.begin(), specularMaps.end());
	}

	return modelGeometry;
}

vector<Texture*> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
{
	vector<Texture*> textures;

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
			texture->ID = LoadTextureFromFile(str.C_Str(), mDirectory);
			texture->Type = typeName;
			texture->Path = str;
			textures.push_back(texture);
			mLoadedTextures.push_back(texture);
		}
	}
	return textures;
}

int Model::LoadTextureFromFile(const char* path, string directory)
{
	// Generate texture ID and load texture data 
	string filename = string(path);
	filename = directory + '/' + filename;
	//DirectX::CreateDDSTextureFromFile();
	return 0;
}
